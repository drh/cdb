#define NDEBUG
#include "c.h"
#undef NDEBUG
#include "glue.h"
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include "table.h"

static char rcsid[] = "$Id$";

static char *leader;
static int maxalign;
static List typelist;
static int ntypes;
static int ncoordinates;
static int nfiles;
static int nsymbols;
static int nmodules;
static Symbol module;
static char *filelist[32+1];
static int epoints;
static List coordlist;
static Symbol coordinates;
static List symbollist;
static Symbol link;
static Symbol nub_bp;
static Symbol nub_tos;
static Symbol tos;
static struct string {
	char *str;
	uint16 index;
	struct string *link;
} *stringlist, **laststring = &stringlist;
static Table_T strings;

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

/* emit_string - emits the index of a string */
static int emit_string(int lc, char *str) {
	if (str) {
		struct string *p = Table_get(strings, str);
		if (p == NULL) {
			static int index = 1;
			NEW(p, PERM);
			p->str = str;
			p->index = index;
			index += strlen(str) + 1;
			p->link = NULL;
			*laststring = p;
			laststring = &p->link;
			Table_put(strings, str, p);
		}
		return emit_value(lc, unsignedshort, p->index);
	} else
		return emit_value(lc, unsignedshort, 0);
}


/* annotate - annotate a type ty with the label of its emitted stype structure.
Other parts of stab.c annotate types and append them to typelist by
calling annotate, which traverses the type, generates the necessary
symbol, and stores it in the type's undocumented x.xtfield, which
also marks the type as annotated.
*/
static Symbol annotate(Type ty) {
	if (ty->x.xt == NULL) {
		ty->x.xt = genident(STATIC, array(unsignedtype, 0, 0), GLOBAL);
		typelist = append(ty, typelist);
		switch (ty->op) {	/* traverse the type */
		case CONST: case VOLATILE: case CONST+VOLATILE:
		case POINTER: case ARRAY:
			annotate(ty->type);
			break;
		case STRUCT: case UNION: {
			Field f;
			for (f = fieldlist(ty); f; f = f->link)
				annotate(f->type);
			break;
			}
		case FUNCTION: {
			int i;
			annotate(ty->type);
			if (ty->u.f.proto)
				for (i = 0; ty->u.f.proto[i]; i++)
					annotate(ty->u.f.proto[i]);
			break;
			}
		}
	}
	return ty->x.xt;
}

/* emit_symbol - emits an initialized ssymbol for p, annotates p,
and returns symbol-table entry for that initialized variable
*/
static Symbol emit_symbol(Symbol p) {
	while (p && (!p->defined
	&& (p->sclass == EXTERN || isfunc(p->type) && p->sclass == AUTO)))
		p = p->up;
	if (p == NULL)
		return NULL;
	if (!p->y.emitted) {
		int lc;
		Symbol up;
		p->y.emitted = 1;
		up = emit_symbol(p->up);
		if (p->y.p == NULL)
			p->y.p = genident(STATIC, array(inttype, 0, 0), GLOBAL);
		comment("%s\n", typestring(p->type, p->name));
		defglobal(p->y.p, DATA);
		switch (p->sclass) {
		case ENUM:
			lc = emit_value(0, inttype, (long)p->u.value);
			lc = emit_value(lc, voidptype, NULL);
			break;
		case TYPEDEF:
			lc = emit_value(0, inttype, 0L);
			lc = emit_value(lc, voidptype, NULL);
			break;
		case STATIC: case EXTERN:
			lc = emit_value(0, inttype, 0L);
			lc = emit_value(lc, voidptype, p);
			break;
		default:
			lc = emit_value(0, inttype, 0L);
			lc = emit_value(lc, voidptype, p->scope >= PARAM ? NULL : p);
		}
		lc = emit_string(lc, p->name);
		if (p->src.file)
			lc = emit_string(lc, p->src.file);
		else
			lc = emit_value(lc, voidptype, NULL);
		lc = emit_value(lc, unsignedchar, (unsigned long)(p->scope > LOCAL ? LOCAL : p->scope));
		lc = emit_value(lc, unsignedchar, (unsigned long)p->sclass);
		lc = emit_value(lc, voidptype, module);
		lc = emit_value(lc, voidptype, annotate(p->type));
		if (up == NULL)
			lc = emit_value(lc, voidptype, NULL);
		else if (p->scope == GLOBAL || up->scope != GLOBAL)
			lc = emit_value(lc, voidptype, up->y.p);
		else
			lc = emit_value(lc, voidptype, link);
		lc = pad(maxalign, lc);
		nsymbols += lc;

	}
	return p;
}

/* emit_header - emit len, op, and size fields of a sstype */
static int emit_header(unsigned long len, unsigned long op, unsigned long size) {
	int lc;

	lc = emit_value( 0, unsignedshort, offsetof(struct stype, u) + len);
	lc = emit_value(lc, unsignedchar,  op);
	lc = emit_value(lc, unsignedtype,  size);
	return lc;	
}

/* emit_type - emits an stype structure for ty */
static void emit_type(Type ty) {
	struct stype stype;
	int lc;

	assert(ty);
	comment("%t\n", ty);
	assert(ty->x.xt);
	defglobal(ty->x.xt, DATA);
	switch (ty->op) {
	case VOLATILE: case CONST+VOLATILE: case CONST:
		lc = emit_header(sizeof stype.u.q, ty->op, ty->size);
		lc = emit_value(lc, voidptype, ty->type->x.xt);
		break;
	case POINTER:
		lc = emit_header(sizeof stype.u.p, ty->op, ty->size);
		lc = emit_value(lc, voidptype, ty->type->x.xt);
		break;
	case ARRAY:
		lc = emit_header(sizeof stype.u.a, ty->op, ty->size);
		lc = emit_value(lc, voidptype, ty->type->x.xt);
		if (ty->type->size > 0)
			lc = emit_value(lc, unsignedtype, (unsigned long)ty->size/ty->type->size);
		else
			lc = emit_value(lc, unsignedtype, 0UL);
		break;
	case FUNCTION: {
		unsigned len = sizeof stype.u.f.type;
		if (ty->u.f.proto && ty->u.f.proto[0]) {
			Type *tp = ty->u.f.proto;
			for ( ; *tp; tp++)
				len += sizeof stype.u.f.args;
		}
		lc = emit_header(len, ty->op, ty->size);
		lc = emit_value(lc, voidptype, ty->type->x.xt, lc);
		if (ty->u.f.proto && ty->u.f.proto[0]) {
			Type *tp = ty->u.f.proto;
			for ( ; *tp; tp++)
				lc = emit_value(lc, voidptype, (*tp)->x.xt);
		}
		break;
		}
	case STRUCT: case UNION: {
		unsigned len = sizeof stype.u.s.tag;
		Field f;
		for (f = fieldlist(ty); f; f = f->link)
			len += sizeof stype.u.s.fields;
		lc = emit_header(len, ty->op, ty->size);
		lc = emit_string(lc, ty->u.sym->name);
		for (f = fieldlist(ty); f; f = f->link) {
			lc = emit_string(lc, f->name);
			lc = emit_value(lc, voidptype, f->type->x.xt);
			if (f->lsb) {
				union offset o;
				if (IR->little_endian) {
					o.le.size = -fieldsize(f) + 1;
					o.le.lsb = fieldright(f);
					o.le.offset = f->offset;
				} else {
					o.be.size = -fieldsize(f) + 1;
					o.be.lsb = fieldright(f);
					o.be.offset = f->offset;
				}
				lc = emit_value(lc, unsignedtype, o.offset);
			} else
				lc = emit_value(lc, unsignedtype, f->offset);
		}
		break;
		}
	case ENUM: {
		Symbol *p;
		unsigned len = sizeof stype.u.e.tag;
		for (p = ty->u.sym->u.idlist; *p; p++)
			len += sizeof stype.u.e.enums;
		lc = emit_header(len, ty->op, ty->size);
		lc = emit_string(lc, ty->u.sym->name);
		for (p = ty->u.sym->u.idlist; *p; p++) {
			lc = emit_string(lc, (*p)->name);
			lc = emit_value(lc, inttype, (*p)->u.value);
		}
		break;
		}
	case VOID: case FLOAT: case INT: case UNSIGNED:
		lc = emit_header(0, ty->op, ty->size);
		break;
	default:assert(0);
	}
	lc = pad(maxalign, lc);
	ntypes += lc;
}

/* stabend - emits the symbol table */
static void stabend(Coordinate *cp, Symbol symroot, Coordinate *cpp[], Symbol sp[], Symbol *ignore) {
	int nstrings;
	Symbol types, files, strs;

	{	/* emit symbols */
		Symbol p, *allsyms = ltov(&symbollist, PERM);
		int i, lc;
		for (i = 0; allsyms[i]; i++)
			emit_symbol(allsyms[i]);
		p = emit_symbol(symroot);
		comment("the link symbol\n");
		defglobal(link, DATA);
		lc = emit_value(0, inttype, 0L);
		lc = emit_value(lc, voidptype, NULL);
		lc = emit_value(lc, voidptype, NULL);
		lc = emit_value(lc, voidptype, NULL);
		lc = emit_value(lc, unsignedchar, 0L);
		lc = emit_value(lc, unsignedchar, 0L);
		lc = emit_value(lc, voidptype, NULL);
		lc = emit_value(lc, voidptype, p->y.p);
		lc = pad(maxalign, lc);
		nsymbols += lc;
	}
	{	/* emit coordinates */
		int i, lc;
		Coordinate **cpp = ltov(&coordlist, PERM);
		comment("coordinates:\n");
		defglobal(coordinates, DATA);
		lc = emit_value(0, unsignedtype, 0UL);
		lc = pad(maxalign, lc);
		for (i = 0; cpp[i]; i++) {
			static int n;
			Coordinate *cp = cpp[i];
			union scoordinate w;
			w.i = 0;
			if (IR->little_endian) {
				w.le.index = fileindex(cp->file);
				w.le.x = cp->x;
				w.le.y = cp->y;                                 
			} else {
				w.be.index = fileindex(cp->file);
				w.be.x = cp->x;
				w.be.y = cp->y;
			}                               
			comment("%d: %s(%d) %d.%d\n", ++n, cp->file ? cp->file : "",
				fileindex(cp->file), cp->y, cp->x);
			lc = emit_value(0, unsignedtype, (unsigned)w.i);
			lc = pad(maxalign, lc);
			ncoordinates += lc;
		}
		lc = emit_value(0, unsignedtype, 0UL);
		lc = pad(maxalign, lc);
		ncoordinates += lc;
	}
	{	/* emit files */
		int i = 0, lc;
		files = genident(STATIC, array(ptr(chartype), 1, 0), GLOBAL);
		comment("files:\n");
		defglobal(files, DATA);
		lc = emit_value(0, voidptype, NULL);
		lc = pad(maxalign, lc);
		nfiles += lc;
		for (i = 1; filelist[i]; i++) {
			lc = emit_value(0, voidptype, mkstr(filelist[i])->u.c.loc);
			lc = pad(maxalign, lc);
			nfiles += lc;

			}
		lc = emit_value(0, voidptype, NULL);
		lc = pad(maxalign, lc);
		nfiles += lc;
	}
	{	/* emit types */
		Type *alltypes = ltov(&typelist, PERM);
		int i;
		for (i = 0; alltypes[i] != NULL; i++)
			emit_type(alltypes[i]);
	}
	{	/* emit strings */
		struct string *p;
		strs = genident(STATIC, array(chartype, 1, 0), GLOBAL);
		comment("strings:\n");
		defglobal(strs, LIT);
		(*IR->defstring)(1, "");
		nstrings = 1;
		for (p = stringlist; p; p = p->link) {
			int len = strlen(p->str);
			(*IR->defstring)(len + 1, p->str);
			nstrings += len + 1;
		}
	}
	{	/* emit module */
		int lc;
		comment("module:\n");
		defglobal(module, DATA);
		lc = emit_value( 0, voidptype, coordinates);
		lc = emit_value(lc, voidptype, files);
		lc = emit_value(lc, voidptype, link);
		lc = emit_value(lc, unsignedshort, nstrings);
		lc = emit_value(lc, voidptype, strs);
		lc = pad(maxalign, lc);
		nmodules += lc;
	}
	Table_free(&strings);
#define printit(x) fprintf(stderr, "%7d " #x "\n", n##x); total += n##x
	{
		int total = 0;
		printit(types);
		printit(coordinates);
		printit(files);
		printit(symbols);
		printit(strings);
		printit(modules);
		fprintf(stderr, "%7d bytes total\n", total);
	}
#undef printit
}
/* fileindex - returns the index in filelist of name, adding it if necessary */
static int fileindex(char *name) {
	int i;

	if (name == NULL)
		return 0;
	for (i = 1; filelist[i]; i++)
		if (filelist[i] == name)
			return i;
	if (i >= NELEMS(filelist)-1)
		return 0;
	filelist[i] = name;
	return i;
}

/* tail - returns the current tail of the symbol table */
static Symbol tail(void) {
	Symbol p = allsymbols(identifiers);

	while (p && (!p->defined
	&& (p->sclass == EXTERN || isfunc(p->type) && p->sclass == AUTO)))
		p = p->up;
	if (p) {	/* append p to symbollist */
		if (p->y.p == NULL) {
			p->y.p = genident(STATIC, array(inttype, 0, 0), GLOBAL);
			symbollist = append(p, symbollist);
		}
		return p->y.p;
	} else
		return link;
}

/* point_hook - called at each execution point */
static void point_hook(void *cl, Coordinate *cp, Tree *e) {
	Coordinate *p;
		
	NEW(p, PERM);
	*p = *cp;
	coordlist = append(p, coordlist);
	epoints++;
	/*
	add breakpoint test to *e:
	(module.coordinates[i].i < 0 && _Nub_bp(i, tail), *e)
	*/
	*e = right(tree(AND, voidtype,
			(*optree['<'])(LT,
				rvalue((*optree['+'])(ADD,
					pointer(idtree(coordinates)),
					cnsttree(inttype, (long)epoints))),
				consttree(0, inttype)),
			calltree(pointer(idtree(nub_bp)), voidtype,
				tree(ARG+P, voidptype, retype(idtree(tail()), voidptype), tree(
				     ARG+I, inttype, cnsttree(inttype, (long)epoints), NULL)),
				NULL)
		       ), *e);
}

/* setoffset - emits code to set the offset field for p */
static void setoffset(Symbol p, void *tos) {
	if (p->y.p == NULL) {	/* append p to symbollist */
		p->y.p = genident(STATIC, array(inttype, 0, 0), GLOBAL);
		symbollist = append(p, symbollist);
	}
	walk(asgntree(ASGN,
		rvalue(pointer(idtree(p->y.p))),
		(*optree['-'])(SUB,
			cast(isarray(p->type) ? pointer(idtree(p)) : lvalue(idtree(p)), ptr(chartype)),
			cast(lvalue(idtree(tos)), ptr(chartype)))), 0, 0);
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
	addfield("func",  voidptype);
	addfield("module",voidptype);
	addfield("tail",  voidptype);
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
	set(func,       idtree(mkstr(cfunc->name)->u.c.loc));
	set(module,     idtree(module));
	set(tail,       cnsttree(voidptype, (void*)0));
#undef set
	walk(asgn(nub_tos, lvalue(idtree(tos))), 0, 0);
	foreach(identifiers, PARAM, setoffset, tos);
}

/* block_hook - called at block entry */
static void block_hook(void *cl, Symbol *p) {
	while (*p)
		setoffset(*p++, tos);
}

/* exit_hook - called at function exit */
static void exit_hook(void *cl, Symbol cfunc) {
	tos = NULL;
}

/* return_hook - called at return statements */
static void return_hook(void *cl, Symbol cfunc, Tree *e) {
	walk(asgn(nub_tos, field(lvalue(idtree(tos)), string("down"))), 0, 0);
}

/* call_hook - called at function calls.
If *e is a call, call_hook changes the expression to the equivalent of
the C expression

	(tos.ip = i, tos.tail = tail, *e)

where i is the index in coordinates for the execution point of the
expression in which the call appears, and tail is the corresponding
tail of the list of visible symbols.
*/
static void call_hook(void *cl, Coordinate *cp, Tree *e) {
	*e = right(
		asgntree(ASGN,
			field(lvalue(idtree(tos)), string("tail")),
			retype(idtree(tail()), voidptype)),
		*e);
	*e = right(
		asgntree(ASGN,
			field(lvalue(idtree(tos)), string("ip")),
			consttree(epoints, inttype)),
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
	strings = Table_new(0, NULL, NULL);
	fprintf(stderr, "strings = %x\n", strings);
	module = mksymbol(AUTO,
		stringf("_module__V%x%x", time(NULL), getpid()),
		array(unsignedtype, 0, 0));
	module->generated = 1;
	coordinates = genident(STATIC, array(inttype, 1, 0), GLOBAL);
	link = genident(STATIC, array(unsignedtype, 0, 0), GLOBAL);
	attach((Apply) entry_hook, NULL, &events.entry);
	attach((Apply) block_hook, NULL, &events.blockentry);
	attach((Apply) point_hook, NULL, &events.points);
	attach((Apply)  call_hook, NULL, &events.calls);
	attach((Apply)return_hook, NULL, &events.returns);
	attach((Apply)  exit_hook, NULL, &events.exit);
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
		if (strcmp(argv[i], "-g4") == 0) {
			IR->stabinit  = stabinit;
			IR->stabend   = stabend;
			IR->stabblock = 0;
			IR->stabfend  = 0;
			IR->stabline  = 0;
			IR->stabsym   = 0;
			IR->stabtype  = 0;
			glevel = 4;
			break;
		}
}
