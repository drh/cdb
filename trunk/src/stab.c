#define NDEBUG
#include "c.h"
#undef NDEBUG
#include "pkl-int.h"
#include "glue.h"
#include "sym.h"
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include "table.h"
#include "seq.h"

static char rcsid[] = "$Id$";

static char *leader;
static sym_module_ty pickle;
static Symbol module;
static Symbol bpflags;
static unsigned uname;
static Symbol nub_bp;
static Symbol nub_tos;
static Symbol tos;
static Seq_T locals;
static Seq_T statics;
static Table_T uidTable;
static int maxalign;

static int typeuid(const Type);
static int symboluid(const Symbol);

char *string(const char *str) {
	return (char *)Atom_string(str);
}

char *stringd(long n) {
	return (char *)Atom_int(n);
}

char *stringn(const char *str, int len) {
	return (char *)Atom_new(str, len);
}

/* comment - emits an assembly language comment */
static void comment(char *fmt, ...) {
	va_list ap;

	print(leader);
	va_start(ap, fmt);
	vfprint(stdout, NULL, fmt, ap);
	va_end(ap);
}

/* pad - emits padding, when necessary, and returns updated location counter */
static int pad(int align, int lc) {
	assert(align);
	if (lc%align) {
		(*IR->space)(align - lc%align);
		lc = roundup(lc, align);
	}
	if (align > maxalign)
		maxalign = align;
	return lc;
}

/* emit_value - emits an initialization for the type given by ty */
static int emit_value(int lc, Type ty, ...) {
	Value v;
	va_list ap;

	va_start(ap, ty);
	if (lc == 0)
		maxalign = 0;
	lc = pad(ty->align, lc);
	switch (ty->op) {
	case INT:      v.i = va_arg(ap, long);          break;
	case UNSIGNED: v.u = va_arg(ap, unsigned long); break;
	case FLOAT:    v.d = va_arg(ap, long double);   break;
	case POINTER:
		defpointer(va_arg(ap, Symbol));
		return lc + ty->size;
	default: assert(0);
	}
	va_end(ap);
	(*IR->defconst)(ty->op, ty->size, v);
	return lc + ty->size;
}

/* typeuid - returns ty's uid, adding the type, if necessary */
static int typeuid(const Type ty) {
	sym_type_ty type;

	if (ty->x.typeno != 0)
		return ty->x.typeno;
	ty->x.typeno = pickle->nuids++;
	switch (ty->op) {
#define xx(op) case op: type = sym_##op(ty->size, ty->align); break
	xx(INT);
	xx(UNSIGNED);
	xx(FLOAT);
	xx(VOID);
#undef xx
#define xx(op) case op: type = sym_##op(ty->size, ty->align, typeuid(ty->type)); break
	xx(POINTER);
	xx(ARRAY);
	xx(CONST);
	xx(VOLATILE);
#undef xx
case CONST+VOLATILE:
	type = sym_CONST(ty->size, ty->align, typeuid(ty->type));
	break;
	case ENUM: {
		list_ty ids = Seq_new(0);
		int i;
		for (i = 0; ty->u.sym->u.idlist[i] != NULL; i++)
			Seq_addhi(ids, sym_enum_(ty->u.sym->u.idlist[i]->name,
				ty->u.sym->u.idlist[i]->u.value));
		assert(i > 0);
		type = sym_ENUM(ty->size, ty->align, ty->u.sym->name, ids);
		break;
		}
	case STRUCT: case UNION: {
		list_ty fields = Seq_new(0);
		Field p = fieldlist(ty);
		for ( ; p != NULL; p = p->link)
			Seq_addhi(fields, sym_field(p->name, typeuid(p->type), p->offset, p->bitsize, p->lsb));
		if (ty->op == STRUCT)
			type = sym_STRUCT(ty->size, ty->align, ty->u.sym->name, fields);
		else
			type = sym_UNION (ty->size, ty->align, ty->u.sym->name, fields);
		break;
		}
	case FUNCTION: {
		list_ty formals = Seq_new(0);
		if (ty->u.f.proto != NULL && ty->u.f.proto[0] != NULL) {
			int i;
			for (i = 0; ty->u.f.proto[i] != NULL; i++)
				Seq_addhi(formals, to_generic_int(typeuid(ty->u.f.proto[i])));
		} else if (ty->u.f.proto != NULL && ty->u.f.proto[0] == NULL)
			Seq_addhi(formals, to_generic_int(typeuid(voidtype)));
		type = sym_FUNCTION(ty->size, ty->align, typeuid(ty->type), formals);
		break;
		}
	default: assert(0);
	}
	Seq_addhi(pickle->items, sym_Type(ty->x.typeno, type));
	return ty->x.typeno;
}

/* up - returns p's non-external ancestor */
static Symbol up(Symbol p) {
	while (p != NULL && p->defined == 0
	       && (p->sclass == EXTERN || isfunc(p->type) && p->sclass == AUTO))
		p = p->up;
	return p;
}

/* symboluid - returns sym's uid, adding the symbol, if necessary */
static int symboluid(const Symbol p) {
	int uid;
	sym_symbol_ty sym;

	if (p == NULL)
		return 0;
	sym = Table_get(uidTable, p);
	if (sym != NULL)
		return sym->uid;
	uid = pickle->nuids++;
	switch (p->sclass) {
	case ENUM:
		sym = sym_ENUMCONST(p->name, uid, uname, NULL, 0, 0,
			p->u.value);
		sym->type = typeuid(inttype);
		break;
	case TYPEDEF:
		sym = sym_TYPEDEF(p->name, uid, uname, NULL, 0, 0);
		sym->type = typeuid(p->type);
		break;
	case STATIC: case EXTERN:
		sym = sym_STATIC(p->name, uid, uname, NULL, 0, 0,
			Seq_length(statics));
		sym->type = typeuid(p->type);
		Seq_addhi(statics, p);
		Seq_addhi(pickle->globals, to_generic_int(uid));
		break;
	default:
		if (p->scope >= PARAM)
			sym = sym_AUTO(p->name, uid, uname, NULL, 0, 0,
				p->x.offset);
		else {
			sym = sym_STATIC(p->name, uid, uname, NULL, 0, 0,
				Seq_length(statics));
			Seq_addhi(statics, p);
			Seq_addhi(pickle->globals, to_generic_int(uid));
		}
		sym->type = typeuid(p->type);
	}
	Table_put(uidTable, p, sym);
	Seq_addhi(pickle->items, sym_Symbol(uid, sym));
	sym->src = sym_coordinate(p->src.file, p->src.x, p->src.y);
	sym->uplink = symboluid(up(p->up));
	return sym->uid;
}

/* stabend - emits the symbol table */
static void stabend(Coordinate *cp, Symbol symroot, Coordinate *cpp[], Symbol sp[], Symbol *ignore) {
	Symbol addresses;
	int naddresses, nmodules, nbpflags;

	{	/* emit breakpoint flags */
		int i, lc;
		comment("breakpoint flags:\n");
		defglobal(bpflags, DATA);
		nbpflags = Seq_length(pickle->spoints);
		(*IR->space)(nbpflags);
	}
	{	/* annotate top-level symbols */
		Symbol p;
		for (p = symroot; p != NULL; p = up(p->up))
			symboluid(p);
	}
	{	/* emit addresses of top-level and static symbols */
		int i, lc = 0, count = Seq_length(statics);
		addresses = genident(STATIC, array(voidptype, 1, 0), GLOBAL);
		comment("addresses:\n");
		defglobal(addresses, LIT);
		for (i = 0; i < count; i++) {
			Symbol p = Seq_get(statics, i);
			lc = emit_value(lc, voidptype, p);
		}
		lc = pad(maxalign, lc);
		naddresses = lc;
		Seq_free(&statics);
	}
	{	/* emit module */
		int lc;
		comment("module:\n");
		defglobal(module, LIT);
		lc = emit_value( 0, unsignedtype, (unsigned long)uname);
		lc = emit_value(lc, voidptype, bpflags);
		lc = emit_value(lc, voidptype, addresses);
		lc = pad(maxalign, lc);
		nmodules = lc;
	}
	Seq_free(&locals);
#define printit(x) fprintf(stderr, "%7d " #x "\n", n##x); total += n##x
	{
		int total = 0;
		printit(bpflags);
		printit(addresses);
		printit(modules);
		fprintf(stderr, "%7d bytes total\n", total);
	}
#undef printit
	{	/* complete and write symbol-table pickle */
		FILE *f = fopen("sym.pickle", "wb");
		sym_write_module(pickle, f);
		fclose(f);
	}
}

/* tail - returns the current tail of the symbol table */
static int tail(void) {
	Symbol p = allsymbols(identifiers);

	p = up(p);
	if (p)
		return symboluid(p);
	else
		return 0;
}

/* point_hook - called at each execution point */
static void point_hook(void *cl, Coordinate *cp, Tree *e) {
	Tree t;
	
	/*
	add breakpoint test to *e:
	(module.bpflags[i] < 0 && _Nub_bp(i), *e)
	*/
	t = tree(AND, voidtype,
		(*optree[NEQ])(NE,
			rvalue((*optree['+'])(ADD,
				pointer(idtree(bpflags)),
				cnsttree(inttype, Seq_length(pickle->spoints)))),
			cnsttree(inttype, 0L)),
		vcall(nub_bp, voidtype, cnsttree(inttype, Seq_length(pickle->spoints)), NULL));
	if (*e)
		*e = tree(RIGHT, (*e)->type, t, *e);
	else
		*e = t;
	Seq_addhi(pickle->spoints, sym_spoint(sym_coordinate(cp->file, cp->x, cp->y), tail()));
}

/* setoffset - emits code to set the offset field for p */
static void setoffset(Symbol p, void *tos) {
	Seq_addhi(locals, p);
	p->addressed = 1;
}

/* entry_hook - called an function entry */
static void entry_hook(void *cl, Symbol cfunc) {
	static int nfuncs;
	Type ty;

	/*
	Simulate the declaration of an sframe structure,
	but without the tag.
	*/
	ty = newstruct(STRUCT, "");
#define addfield(name,t) \
	ty->size = roundup(ty->size, t->align);\
	if (ty->align < t->align) ty->align = t->align; \
	newfield(string(name), ty, t)->offset = ty->size; \
	ty->size += t->size
	addfield("up",    voidptype);
	addfield("down",  voidptype);
	addfield("func",  inttype);
	addfield("module",voidptype);
	addfield("ip",    inttype);     
#undef addfield
	ty->size = roundup(ty->size, ty->align);
	ty->u.sym->defined = 1;
	ty->u.sym->generated = 1;
	tos = genident(AUTO, ty, LOCAL);
	addlocal(tos);
	tos->defined = 1;
	/*
	 Generated the assignments to the shadow
	 frame fields.
	*/
#define set(name,e) walk(asgntree(ASGN,field(lvalue(idtree(tos)),string(#name)),(e)),0,0)
	set(down,       idtree(nub_tos));
	set(func,       cnsttree(inttype, symboluid(cfunc)));
	set(module,     idtree(module));
#undef set
	walk(asgn(nub_tos, lvalue(idtree(tos))), 0, 0);
	foreach(identifiers, PARAM, setoffset, tos);
}

/* block_hook - called at block entry */
static void block_hook(void *cl, Symbol *p) {
	while (*p)
		setoffset(*p++, tos);
}

/* stabfend - called at end end of compiling a function */
static void stabfend(Symbol cfunc, int lineno) {
	int count = Seq_length(locals);

	for ( ; count > 0; count--) {
		Symbol p = Seq_remlo(locals);
		p->x.offset -= tos->x.offset;
	}
	tos = NULL;
}

/* return_hook - called at return statements */
static void return_hook(void *cl, Symbol cfunc, Tree *e) {
	walk(asgn(nub_tos, field(lvalue(idtree(tos)), string("down"))), 0, 0);
}

/* call_hook - called at function calls.
If *e is a call, call_hook changes the expression to the equivalent of
the C expression

	(tos.ip = i, *e)

where i is the stopping point index for the execution point of the
expression in which the call appears.
*/
static void call_hook(void *cl, Coordinate *cp, Tree *e) {
	assert(*e);
	*e = tree(RIGHT, (*e)->type,
		asgntree(ASGN,
			field(lvalue(idtree(tos)), string("ip")),
			cnsttree(inttype, Seq_length(pickle->spoints))),
		*e);
}

/* stabinit - initialize for symbol table emission */
static void stabinit(char *file, int argc, char *argv[]) {
	extern Interface sparcIR, solarisIR, x86IR;
	extern int getpid(void);

	if (IR == &solarisIR || IR == &sparcIR)
		leader = "!";
	else if (IR == &x86IR)
		leader = ";";
	else
		leader = " #";  /* it's a MIPS or ALPHA */
	uname = time(NULL)<<7|getpid();
	pickle = sym_module(uname, 1, Seq_new(0), Seq_new(0), Seq_new(0));
	locals = Seq_new(0);
	statics = Seq_new(0);
	uidTable = Table_new(0, 0, 0);
	module = mksymbol(AUTO,	stringf("_module__V%x", uname), array(unsignedtype, 0, 0));
	module->generated = 1;
	bpflags = genident(STATIC, array(chartype, 1, 0), GLOBAL);
	attach((Apply) entry_hook, NULL, &events.entry);
	attach((Apply) block_hook, NULL, &events.blockentry);
	attach((Apply) point_hook, NULL, &events.points);
	attach((Apply)  call_hook, NULL, &events.calls);
	attach((Apply)return_hook, NULL, &events.returns);
	nub_bp = mksymbol(EXTERN, "_Nub_bp", ftype(voidtype, inttype));
	nub_bp->defined = 0;
	nub_tos = mksymbol(EXTERN, "_Nub_tos", voidptype);
	nub_tos->defined = 0;
}

void zstab_init(int argc, char *argv[]) {
	int i;
	static int inited;

	if (inited)
		return;
	assert(IR);
	inited = 1;
	for (i = 1; i < argc; i++)
		if (strcmp(argv[i], "-v") == 0)
			fprint(stderr, "%s %s\n", argv[0], rcsid);
		else if (strcmp(argv[i], "-g4") == 0) {
			IR->stabinit  = stabinit;
			IR->stabend   = stabend;
			IR->stabblock = 0;
			IR->stabfend  = stabfend;
			IR->stabline  = 0;
			IR->stabsym   = 0;
			IR->stabtype  = 0;
			glevel = 4;
		}
}
