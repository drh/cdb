#define NDEBUG
#include "c.h"
#undef NDEBUG
#include "glue.h"
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include "table.h"
#include "seq.h"

static char rcsid[] = "$Id$";

static char *leader;
static int maxalign;
static int nsymbols;
static Symbol module;
static u4 uid;
static char *filelist[32+1];
static int epoints;
static List coordlist;
static Symbol coordinates;
static List symbollist;
static Symbol link;
static Symbol nub_bp;
static Symbol nub_tos;
static Symbol tos;
struct cnst {
	enum { cString, cType, cSym } tag;
	u4 index;
	int length;
	const void *ptr;
};
static Seq_T constantList;
static Table_T constantTable;
static u4 constantIndex = 1;

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

/* stringindex - returns the string index of str, adding it if necessary */
static unsigned long stringindex(const char *str) {
	struct cnst *p = Table_get(constantTable, str);

	if (p == NULL) {
		NEW(p, PERM);
		p->tag = cString;
		p->ptr = str;
		Table_put(constantTable, str, p);
		p->index = constantIndex;
		p->length = strlen(str) + 1;
		constantIndex += p->length;
		Seq_addhi(constantList, p);
	}
	assert(p->index);
	return p->index;
}

/* emit_string - emits the index of a string */
static int emit_string(int lc, char *str) {
	assert(str);
	if (str) {
		return emit_value(lc, unsignedtype, stringindex(str));
	} else
		return emit_value(lc, unsignedtype, 0L);
}

/* typeindex - return the index in the constant table of type ty */
static unsigned long typeindex(Type ty) {
	struct stype stype;
	struct cnst *p = Table_get(constantTable, ty);

	if (p != NULL)
		return p->index;
	NEW(p, PERM);
	p->tag = cType;
	p->ptr = ty;
	constantIndex = roundup(constantIndex, unsignedtype->align);
	p->index = constantIndex;
	Table_put(constantTable, ty, p);
	Seq_addhi(constantList, p);
	p->length = offsetof(struct stype, u); 
	switch (ty->op) {
	case VOID: case FLOAT: case INT: case UNSIGNED:
		constantIndex += p->length;
		break;
	case VOLATILE: case CONST+VOLATILE: case CONST:
		p->length += sizeof stype.u.q;
		constantIndex += p->length;
		typeindex(ty->type);
		break;
	case POINTER:
		p->length += sizeof stype.u.p;
		constantIndex += p->length;
		typeindex(ty->type);
		break;
	case ARRAY:
		p->length += sizeof stype.u.a;
		constantIndex += p->length;
		typeindex(ty->type);
		break;
	case FUNCTION: {
		int i;
		p->length += sizeof stype.u.f - sizeof stype.u.f.args;
		if (ty->u.f.proto)
			for (i = 0; ty->u.f.proto[i] != NULL; i++)
				p->length += sizeof stype.u.f.args;
		constantIndex += p->length;
		for (i = 0; ty->u.f.proto[i] != NULL; i++)
			typeindex(ty->u.f.proto[i]);
		break;
		}
	case STRUCT: case UNION: {
		Field f;
		p->length += sizeof stype.u.s - sizeof stype.u.s.fields;
		for (f = fieldlist(ty); f != NULL; f = f->link)
			p->length += sizeof stype.u.s.fields;
		constantIndex += p->length;
		for (f = fieldlist(ty); f != NULL; f = f->link)
			typeindex(f->type);
		break;
		}
	case ENUM: {
		int i;
		p->length += sizeof stype.u.e - sizeof stype.u.e.enums;
		for (i = 0; ty->u.sym->u.idlist[i] != NULL; i++)
			p->length += sizeof stype.u.e.enums;
		constantIndex += p->length;
		break;
		}
	default:assert(0);
	}
	return p->index;
}

/* emit_type - emits an stype structure and returns its index */
static u4 emit_type(const Type ty) {
	int lc;
	struct stype stype;
	struct cnst *p = Table_get(constantTable, ty);

	assert(ty);
	assert(p);
	lc = emit_value( 0, unsignedshort, (unsigned long)p->length);
	lc = emit_value(lc, unsignedchar,  (unsigned long)ty->op);
	lc = emit_value(lc, unsignedtype,  (unsigned long)ty->size);
	switch (ty->op) {
	case VOID: case FLOAT: case INT: case UNSIGNED:
		break;
	case VOLATILE: case CONST+VOLATILE: case CONST:
	case POINTER:
		lc = emit_value(lc, unsignedtype, typeindex(ty->type));
		break;
	case ARRAY:
		lc = emit_value(lc, unsignedtype, typeindex(ty->type));
		if (ty->type->size > 0)
			lc = emit_value(lc, unsignedtype, (unsigned long)ty->size/ty->type->size);
		else
			lc = emit_value(lc, unsignedtype, 0UL);
		break;
	case FUNCTION: {
		int i;
		lc = emit_value(lc, unsignedtype, typeindex(ty->type), lc);
		if (ty->u.f.proto)
			for (i = 0; ty->u.f.proto[i] != NULL; i++)
				lc = emit_value(lc, unsignedtype, typeindex(ty->u.f.proto[i]));
		break;
		}
	case STRUCT: case UNION: {
		Field f;
		lc = emit_string(lc, ty->u.sym->name);
		for (f = fieldlist(ty); f; f = f->link) {
			lc = emit_string(lc, f->name);
			lc = emit_value(lc, unsignedtype, typeindex(f->type));
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
				lc = emit_value(lc, unsignedtype, (unsigned long)o.offset);
			} else
				lc = emit_value(lc, unsignedtype, (unsigned long)f->offset);
		}
		break;
		}
	case ENUM: {
		Symbol p;
		int i;
		lc = emit_string(lc, ty->u.sym->name);
		for ( ; (p = ty->u.sym->u.idlist[i]) != NULL; i++) {
			lc = emit_string(lc, p->name);
			lc = emit_value(lc, inttype, (long)p->u.value);
		}
		break;
		}
	default:assert(0);
	}
	lc = pad(maxalign, lc);
	assert(lc == p->length);
	return p->index;
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
		lc = emit_value(lc, unsignedtype, typeindex(p->type));
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

/* stabend - emits the symbol table */
static void stabend(Coordinate *cp, Symbol symroot, Coordinate *cpp[], Symbol sp[], Symbol *ignore) {
	int nconstants, nmodules, nfiles, ncoordinates;
	Symbol files, consts;

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
		lc = emit_value(lc, voidptype, module);
		lc = emit_value(lc, unsignedtype, 0L);
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
		ncoordinates = lc;
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
			lc = emit_value(0, unsignedtype, (unsigned long)w.i);
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
		nfiles = lc;
		for (i = 1; filelist[i]; i++) {
			lc = emit_value(0, voidptype, mkstr(filelist[i])->u.c.loc);
			lc = pad(maxalign, lc);
			nfiles += lc;

			}
		lc = emit_value(0, voidptype, NULL);
		lc = pad(maxalign, lc);
		nfiles += lc;
	}
	{	/* emit constants */
		int lc;
		consts = genident(STATIC, array(chartype, 1, 0), GLOBAL);
		comment("constants:\n");
		defglobal(consts, LIT);
		lc = emit_value(0, chartype, 0L);
		while (Seq_length(constantList) > 0) {
			struct cnst *p = Seq_remlo(constantList);
			switch (p->tag) {
			case cType:
				lc = pad(unsignedtype->align, lc);
				assert(lc == p->index);
				comment("cType index=%d %t\n", lc, p->ptr);
				emit_type((void *)p->ptr);
				break;
			case cString:
				assert(lc == p->index);
				comment("cString index=%d \"%s\"\n", lc, p->ptr);
				(*IR->defstring)(p->length, (char *)p->ptr);
				break;
			default: assert(0);
			}
			lc += p->length;
		}
		nconstants = pad(maxalign, lc);
		Table_free(&constantTable);
		Seq_free(&constantList);
	}
	{	/* emit module */
		int lc;
		comment("module:\n");
		defglobal(module, DATA);
		lc = emit_value( 0, unsignedtype, (unsigned long)uid);
		lc = emit_value( 0, voidptype, coordinates);
		lc = emit_value(lc, voidptype, files);
		lc = emit_value(lc, voidptype, link);
		lc = emit_value(lc, unsignedtype, (unsigned long)nconstants);
		lc = emit_value(lc, voidptype, consts);
		lc = pad(maxalign, lc);
		nmodules = lc;
	}
#define printit(x) fprintf(stderr, "%7d " #x "\n", n##x); total += n##x
	{
		int total = 0;
		printit(coordinates);
		printit(files);
		printit(symbols);
		printit(constants);
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
	constantTable = Table_new(0, NULL, NULL);
	constantList = Seq_new(0);
	uid = time(NULL)<<7|getpid();
	module = mksymbol(AUTO,	stringf("_module__V%x", uid), array(unsignedtype, 0, 0));
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
