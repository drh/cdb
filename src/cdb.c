/* <cdb.c>=                                                                 */
#define NDEBUG
#include "c.h"
#undef NDEBUG
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include "server.h"
#include "symtab.h"

static char rcsid[] = "$Id$";

/* <cdb types>=                                                             */
struct cdb_src {
        int n;
        Nub_coord_T first;
};

/* <cdb prototypes>=                                                        */
static void printparam(struct ssymbol *sym, Nub_state_T *frame);

/* <cdb prototypes>=                                                        */
static void onbreak(Nub_state_T);

/* <cdb prototypes>=                                                        */
static void tput(void *type);

/* <cdb prototypes>=                                                        */
static void vput(void *type, char *address);

/* <cdb data>=                                                              */
static FILE *in, *out;
static Nub_state_T focus;
static int frameno;

/* <cdb data>=                                                              */
static Nub_coord_T bkpts[100];
static int nbpts;

/* <cdb data>=                                                              */
static Nub_coord_T *brkpt = NULL;

/* <cdb static functions>=                                                  */
static int getvalue(int space, void *address, void *buf, int size) {
        int n;

        n = _Nub_fetch(space, address, buf, size);
        assert(n == size);
        return n;
}

static void put(char *fmt, ...) {
        va_list ap;

        va_start(ap, fmt);
        vfprintf(out, fmt, ap);
        va_end(ap);
}

static void prompt(void) {
        put("cdb> ");
        fflush(out);
}

static void printframe(int verbose, Nub_state_T *frame, int frameno) {
        put("%d\t%s(", frameno, frame->name);
        if (verbose > 0)
                /* <print parameters>=                                                      */
                {
                        void *context = frame->context;
                        while (context) {
                                struct ssymbol *sym = _Sym_symbol(context);
                                if (sym->scope == PARAM) {
                                        printparam(sym, frame);
                                        break;
                                } else if (sym->scope < PARAM) {
                                        put("void");
                                        break;
                                }
                                context = sym->uplink;
                        }
                }
        put(")\n");
        if (verbose > 1)
                /* <print locals>=                                                          */
                {
                        void *context = frame->context;
                        while (context) {
                                struct ssymbol *sym = _Sym_symbol(context);
                                if (sym->scope >= LOCAL
                                && (sym->sclass == AUTO || sym->sclass == REGISTER)) {
                                        put("\t");
                                        /* <emit sym's name and value>=                                             */
                                        {
                                                assert(sym);
                                                switch (sym->sclass) {
                                                case ENUM:
                                                        put("%s=%d", sym->name, sym->offset);
                                                        break;
                                                case TYPEDEF:
                                                        put("%s is a typedef for ", sym->name);
                                                        tput(sym->type);
                                                        break;
                                                case EXTERN: case STATIC:
                                                        assert(sym->address);
                                                        put("%s@0x%x=", sym->name, sym->address);
                                                        vput(sym->type, sym->address);
                                                        break;
                                                default: {
                                                        void *addr;
                                                        if (sym->scope >= PARAM) {
                                                                assert(sym->offset);
                                                                addr = frame->fp + sym->offset;
                                                        } else {
                                                                assert(sym->address);
                                                                addr = sym->address;
                                                        }
                                                        put("%s@0x%x=", sym->name, addr);
                                                        vput(sym->type, addr);
                                                        }
                                                }
                                        }

                                        put("\n");
                                } else if (sym->scope < LOCAL)
                                        break;
                                context = sym->uplink;
                        }
                }

}

/* <cdb static functions>=                                                  */
static void printparam(struct ssymbol *sym, Nub_state_T *frame) {
        if (sym->uplink) {
                struct ssymbol *next = _Sym_symbol(sym->uplink);
                if (next->scope == PARAM) {
                        printparam(next, frame);
                        put(",");
                }
        }
        /* <emit sym's name and value>=                                             */
        {
                assert(sym);
                switch (sym->sclass) {
                case ENUM:
                        put("%s=%d", sym->name, sym->offset);
                        break;
                case TYPEDEF:
                        put("%s is a typedef for ", sym->name);
                        tput(sym->type);
                        break;
                case EXTERN: case STATIC:
                        assert(sym->address);
                        put("%s@0x%x=", sym->name, sym->address);
                        vput(sym->type, sym->address);
                        break;
                default: {
                        void *addr;
                        if (sym->scope >= PARAM) {
                                assert(sym->offset);
                                addr = frame->fp + sym->offset;
                        } else {
                                assert(sym->address);
                                addr = sym->address;
                        }
                        put("%s@0x%x=", sym->name, addr);
                        vput(sym->type, addr);
                        }
                }
        }

}
        
/* <cdb static functions>=                                                  */
static void moveto(int n) {
        Nub_state_T new;
        int m = _Nub_frame(n, &new);

        if (m == n) {
                printframe(1, &new, m);
                focus = new;
                frameno = m;
        } else
                put("?There is no frame %d\n", n);
}

/* <cdb static functions>=                                                  */
static int equal(Nub_coord_T *s1, Nub_coord_T *s2) {
        return (s1->y == s2->y || s1->y == 0 || s2->y == 0)
            && (s1->x == s2->x || s1->x == 0 || s2->x == 0)
            && (s1->file[0] == 0 || s2->file[0] == 0 || strcmp(s1->file, s2->file) == 0);
}

/* <cdb static functions>=                                                  */
static void setbp(int i, Nub_coord_T *src, void *cl) {
        struct cdb_src *p = cl;

        p->n++;
        if (p->n == 1)
                p->first = *src;
        else if (p->n == 2) {
                put("Sweep and send one of the following commands:\n");
                put("b %s:%d.%d\n", p->first.file, p->first.y, p->first.x);
        }
        if (p->n > 1)
                put("b %s:%d.%d\n", src->file, src->y, src->x);
}

static void b_cmd(char *line) {
        Nub_coord_T src;
        struct cdb_src z;

        /* <parse source coordinate specification>=                                 */
        {
                int n;
                char *p = line;
                static Nub_coord_T z;

                src = z;
                if ((p = strchr(line, ':')) != NULL) {
                        *p++ = 0;
                        strncpy(src.file, line, sizeof src.file);
                } else
                        p = line;
                /* <skip white space>=                                                      */
                while (*p && isspace(*p))
                        p++;
    
                if (isdigit(*p)) {
                        /* <n := an integer>=                                                       */
                        for (n = 0; isdigit(*p); p++)
                                n = 10*n + (*p - '0');
;
                        src.y = n;
                }
                /* <skip white space>=                                                      */
                while (*p && isspace(*p))
                        p++;

                if (*p == '.')
                        p++;    
                /* <skip white space>=                                                      */
                while (*p && isspace(*p))
                        p++;

                if (isdigit(*p)) {
                        /* <n := an integer>=                                                       */
                        for (n = 0; isdigit(*p); p++)
                                n = 10*n + (*p - '0');
;
                        src.x = n;
                }
                /* <skip white space>=                                                      */
                while (*p && isspace(*p))
                        p++;

                if (*p)
                        put("?Unrecognized coordinate: %s", line);

        }

        z.n = 0;
        _Nub_src(src, setbp, &z);
        if (z.n == 0)
                put("?There is no execution point at %s", line);
        else if (z.n == 1) {
                int i;
                for (i = 0; i < nbpts; i++)
                        if (equal(&z.first, &bkpts[i]))
                                break;
                if (i < nbpts)
                        put("?There is already a breakpoint at %s:%d.%d\n",
                                z.first.file, z.first.y, z.first.x);
                else if (nbpts < sizeof bkpts/sizeof bkpts[0]) {
                        bkpts[nbpts++] = z.first;
                        _Nub_set(z.first, onbreak);
                } else
                        put("?Cannot set more than %d breakpoints\n",
                                sizeof bkpts/sizeof bkpts[0]);
                if (i < nbpts) {
                        put("To remove this breakpoint, sweep and send the command:\n");
                        put("r %s:%d.%d\n", z.first.file, z.first.y, z.first.x);
                }
        }
}

static void r_cmd(char *line) {
        Nub_coord_T src;
        int i, j, n;
        char *p = line;

        /* <skip white space>=                                                      */
        while (*p && isspace(*p))
                p++;

        line = p;
        if (*p)
                /* <parse source coordinate specification>=                                 */
                {
                        int n;
                        char *p = line;
                        static Nub_coord_T z;

                        src = z;
                        if ((p = strchr(line, ':')) != NULL) {
                                *p++ = 0;
                                strncpy(src.file, line, sizeof src.file);
                        } else
                                p = line;
                        /* <skip white space>=                                                      */
                        while (*p && isspace(*p))
                                p++;
    
                        if (isdigit(*p)) {
                                /* <n := an integer>=                                                       */
                                for (n = 0; isdigit(*p); p++)
                                        n = 10*n + (*p - '0');
;
                                src.y = n;
                        }
                        /* <skip white space>=                                                      */
                        while (*p && isspace(*p))
                                p++;

                        if (*p == '.')
                                p++;    
                        /* <skip white space>=                                                      */
                        while (*p && isspace(*p))
                                p++;

                        if (isdigit(*p)) {
                                /* <n := an integer>=                                                       */
                                for (n = 0; isdigit(*p); p++)
                                        n = 10*n + (*p - '0');
;
                                src.x = n;
                        }
                        /* <skip white space>=                                                      */
                        while (*p && isspace(*p))
                                p++;

                        if (*p)
                                put("?Unrecognized coordinate: %s", line);

                }

        else if (brkpt == NULL) {
                put("?There is no current breakpoint\n");
                return;
        } else {
                src = *brkpt;
                brkpt = NULL;
        }
        j = -1;
        for (i = n = 0; i < nbpts; i++)
                if (equal(&src, &bkpts[i])) {
                        if (j < 0)
                                j = i;
                        n++;
                }
        if (n == 0)
                put("?There is no breakpoint at %s", line);
        else if (n == 1) {
                assert(j >= 0);
                _Nub_remove(bkpts[j]);
                bkpts[j] = bkpts[nbpts-1];
                nbpts--;
        } else {
                put("Sweep and send any of the following commands:\n");
                for (i = 0; i < nbpts; i++)
                        if (equal(&src, &bkpts[i]))
                                put("r %s:%d.%d\n",
                                        bkpts[i].file, bkpts[i].y, bkpts[i].x);
        }
}

/* <cdb static functions>=                                                  */
static void tput(void *type) {
        struct stype *ty = _Sym_type(type);
        void *end = (char *)ty + ty->len;

        switch (ty->op) {
        case CONST: case VOLATILE: case CONST+VOLATILE:
                if (ty->op == CONST    || ty->op == CONST+VOLATILE)
                        put("const ");
                if (ty->op == VOLATILE || ty->op == CONST+VOLATILE)
                        put("volatile ");
                tput(ty->u.q.type);
                break;
        case POINTER: {
                struct stype *rty = _Sym_type(ty->u.p.type);
                if (rty->op == STRUCT)
                        put("struct %s ", _Sym_string(rty->u.s.tag));
                else if (rty->op == UNION)
                        put("union %s ", _Sym_string(rty->u.s.tag));
                else if (rty->op != POINTER) {
                        tput(ty->u.p.type);
                        put(" ");
                } else
                        tput(ty->u.p.type);
                put("*");
                break;
                }
        case ARRAY:
                tput(ty->u.a.type);
                if (ty->u.a.nelems > 0)
                        put("[%d]", ty->u.a.nelems);
                else
                        put("[]");
                break;
        case FUNCTION: {
                int i;
                tput(ty->u.f.type);
                put(" (");
                for (i = 0; (void *)&ty->u.f.args[i] < end; i++) {
                        if (i > 0)
                                put(",");
                        tput(ty->u.f.args[i]);
                }
                put(")");
                break;
                }
        case STRUCT:
        case UNION: {
                int i;
                put("%s %s { ", ty->op == STRUCT ? "struct" : "union",
                        _Sym_string(ty->u.s.tag));
                for (i = 0; (void *)&ty->u.s.fields[i] < end; i++) {
                        tput(ty->u.s.fields[i].type);
                        put(" %s; ", _Sym_string(ty->u.s.fields[i].name));
                }
                put("}");
                break;
                }
        case ENUM: {
                int i;
                put("enum %s {", _Sym_string(ty->u.s.tag));
                for (i = 0; (void *)&ty->u.e.enums[i] < end; i++) {
                        if (i > 0)
                                put(",");
                        put("%s=%d", _Sym_string(ty->u.e.enums[i].name), ty->u.e.enums[i].value);
                }
                put("}");
                break;
                }
        case VOID:      put("void"); break;
#define xx(t) if (ty->size == sizeof (t)) do { put(#t); return; } while (0)
        case FLOAT:
                xx(float);
                xx(double);
                xx(long double);
                assert(0);
        case INT:
                xx(char);
                xx(short);
                xx(int);
                xx(long);
                assert(0);
        case UNSIGNED:
                xx(unsigned char);
                xx(unsigned short);
                xx(unsigned int);
                xx(unsigned long);
                assert(0);
#undef xx
        default:assert(0);
        }
}

static void sput(char *address, int max) {
        int i, j;
        unsigned char buf[4];

        put("\"");
        for (i = 0; i < max; i += (int)sizeof buf) {
                getvalue(DATA, address + i, buf, sizeof buf);
                for (j = 0; j < (int)sizeof buf; j++)
                        if (buf[j] == 0) {
                                put("\"");
                                return;
                        } else if (buf[j] < ' ' || buf[j] >= 0177)
                                put("\\x%02x", buf[j]);
                        else
                                put("%c", buf[j]);
        }
        put("...");
}

/* <cdb static functions>=                                                  */
static void vput(void *type, char *address) {
        struct stype *ty = _Sym_type(type);
        void *end = (char *)ty + ty->len;

        if (address == NULL) {
                put("?");
                return;
        }
        switch (ty->op) {
        case CONST: case VOLATILE: case CONST+VOLATILE:
                vput(ty->u.q.type, address);
                break;
        case POINTER: case FUNCTION: {
                void *p;
                put("(");
                tput(type);
                put(")");
                getvalue(DATA, address, &p, ty->size);
                put("0X%x", p);
                if (p && _Sym_type(ty->u.p.type)->size == 1) {
                        put(" ");
                        sput(p, 80);
                }
                break;
                }
        case ARRAY: {
                char prev[1024], buf[1024];
                int size = _Sym_type(ty->u.p.type)->size;
                if (ty->u.a.nelems > 0 && size == 1) {
                        put("{");
                        sput(address, 80);
                        put("}");
                } else if (ty->u.a.nelems > 10 && size <= sizeof buf) {
                        int i;
                        put("{");
                        for (i = 0; i < ty->u.a.nelems - 1; ) {
                                put("\n [%d]=", i);
                                vput(ty->u.a.type, address);
                                getvalue(DATA, address, prev, size);
                                while (++i < ty->u.a.nelems - 1) {
                                        address += size;
                                        getvalue(DATA, address, buf, size);
                                        if (memcmp(prev, buf, size) != 0)
                                                break;
                                }
                        }
                        put("\n [%d]=", i);
                        vput(ty->u.a.type, address);
                        put("\n}");
                } else if (ty->u.a.nelems > 0) {
                        int i;
                        put("{");
                        vput(ty->u.a.type, address);
                        for (i = 1; i < ty->u.a.nelems; i++) {
                                put(",");
                                address = (char *)address + size;
                                vput(ty->u.a.type, address);
                        }
                        put("}");
                } else
                        put("0X%x", address);
                break;
                }
        case STRUCT:
        case UNION: {
                int i;
                for (i = 0; (void *)&ty->u.s.fields[i] < end; i++) {
                        if (i > 0)
                                put(",");
                        put("%s=", _Sym_string(ty->u.s.fields[i].name));
                        if (ty->u.s.fields[i].u.offset < 0) {
                                unsigned buf, off;
                                int size, lsb;
                                struct stype *fty = _Sym_type(ty->u.s.fields[i].type);
                                static union { int i; char endian; } little = { 1 };
                                if (little.endian) {
                                        lsb  = ty->u.s.fields[i].u.le.lsb;
                                        size = -ty->u.s.fields[i].u.le.size + 1;
                                        off  = ty->u.s.fields[i].u.le.offset;
                                } else {
                                        lsb = ty->u.s.fields[i].u.be.lsb;
                                        size = -ty->u.s.fields[i].u.be.size + 1;
                                        off  = ty->u.s.fields[i].u.be.offset;
                                }
                                getvalue(DATA, (char *)address + off, &buf, sizeof (unsigned));
                                buf >>= lsb;
                                if (fty->op == UNSIGNED || (buf&(1<<(size-1))) == 0)
                                        put("%u", size == 8*sizeof buf ? buf : buf&(~(~0UL<<size)));
                                else
                                        put("%d", size == 8*sizeof buf ? buf : (~0UL<<size)|buf);
                        } else
                                vput(ty->u.s.fields[i].type, (char *)address + ty->u.s.fields[i].u.offset);
                }
                put("}");
                break;
                }
        case ENUM: {
                int i, value;
                getvalue(DATA, address, &value, ty->size);
                for (i = 0; (void *)&ty->u.e.enums[i] < end; i++)
                        if (ty->u.e.enums[i].value == value) {
                                put("%s", _Sym_string(ty->u.e.enums[i].name));
                                return;
                        }
                put("(enum %s)%d", _Sym_string(ty->u.s.tag), value);
                break;
                }
        case VOID:      put("void"); break;
#define xx(t,fmt) if (ty->size == sizeof (t)) do { t x; \
        getvalue(DATA, address, &x, ty->size); put(fmt, x); return; } while (0)
        case FLOAT:
                xx(float,"%f");
                xx(double, "%f");
                xx(long double, "%Lg");
                assert(0);
        case INT:
                xx(short, "%d");
                xx(int, "%d");
                xx(long, "%ld");
                assert(ty->size == sizeof (char));
                {
                        char x;
                        getvalue(DATA, address, &x, ty->size);
                        if (x >= ' ' && x < 0177)
                                put("'%c'", x);
                        else
                                put("'\\%03o'", x&0377);
                }
                break;
        case UNSIGNED:
                xx(unsigned char, "'%c'");
                xx(unsigned short, "%u");
                xx(unsigned int, "%u");
                xx(unsigned long, "%lu");
                assert(ty->size == sizeof (unsigned char));
                {
                        unsigned char x;
                        getvalue(DATA, address, &x, ty->size);
                        if (x >= ' ' && x < 0177)
                                put("'%c'", x);
                        else
                                put("'\\%03o'", x&0377);
                }
                break;
#undef xx
        default:assert(0);
        }
        
}

static void v_cmd(char *line) {
        void *context = focus.context;

        while (context) {
                struct ssymbol *sym = _Sym_symbol(context);
                if (sym->name && _Sym_type(sym->type)->op != FUNCTION && sym->sclass != TYPEDEF)
                        if (sym->sclass == STATIC && sym->scope == GLOBAL && sym->file)
                                put("p %s:%s\n", sym->file, sym->name);
                        else
                                put("p %s\n", sym->name);
                context = sym->uplink;
        }
}

static void p_cmd(char *line) {
        char *p = line;

        /* <skip white space>=                                                      */
        while (*p && isspace(*p))
                p++;

        if (*p == 0)
                v_cmd(p);
        else
                while (*p) {
                        struct ssymbol *sym;
                        char *file = p, *name = p;
                        while (*p && !isspace(*p) && *p != ':')
                                p++;
                        if (*p == ':' && p[1] && !isspace(p[1])) {
                                *p++ = 0;
                                name = p;
                                while (*p && !isspace(*p))
                                        p++;
                        } else
                                file = NULL;
                        *p++ = 0;
                        if (file == NULL && (sym = _Sym_find(name, focus.context)) != NULL) {
                                Nub_state_T *frame = &focus;
                                if (/* <sym is a top-level static>=                                             */
                                    (sym->sclass == STATIC && sym->scope == GLOBAL && sym->file)
)
                                        put("%s:", sym->file);
                                /* <emit sym's name and value>=                                             */
                                {
                                        assert(sym);
                                        switch (sym->sclass) {
                                        case ENUM:
                                                put("%s=%d", sym->name, sym->offset);
                                                break;
                                        case TYPEDEF:
                                                put("%s is a typedef for ", sym->name);
                                                tput(sym->type);
                                                break;
                                        case EXTERN: case STATIC:
                                                assert(sym->address);
                                                put("%s@0x%x=", sym->name, sym->address);
                                                vput(sym->type, sym->address);
                                                break;
                                        default: {
                                                void *addr;
                                                if (sym->scope >= PARAM) {
                                                        assert(sym->offset);
                                                        addr = frame->fp + sym->offset;
                                                } else {
                                                        assert(sym->address);
                                                        addr = sym->address;
                                                }
                                                put("%s@0x%x=", sym->name, addr);
                                                vput(sym->type, addr);
                                                }
                                        }
                                }

                                put("\n");
                        } else if (file) {
                                int n = 0;
                                void *context = focus.context;
                                while (context) {
                                        sym = _Sym_symbol(context);
                                        if (/* <sym is a top-level static>=                                             */
                                            (sym->sclass == STATIC && sym->scope == GLOBAL && sym->file)

                                        && strcmp(sym->file, file) == 0
                                        && sym->name && strcmp(sym->name, name) == 0) {
                                                Nub_state_T *frame = &focus;
                                                put("%s:", file);
                                                /* <emit sym's name and value>=                                             */
                                                {
                                                        assert(sym);
                                                        switch (sym->sclass) {
                                                        case ENUM:
                                                                put("%s=%d", sym->name, sym->offset);
                                                                break;
                                                        case TYPEDEF:
                                                                put("%s is a typedef for ", sym->name);
                                                                tput(sym->type);
                                                                break;
                                                        case EXTERN: case STATIC:
                                                                assert(sym->address);
                                                                put("%s@0x%x=", sym->name, sym->address);
                                                                vput(sym->type, sym->address);
                                                                break;
                                                        default: {
                                                                void *addr;
                                                                if (sym->scope >= PARAM) {
                                                                        assert(sym->offset);
                                                                        addr = frame->fp + sym->offset;
                                                                } else {
                                                                        assert(sym->address);
                                                                        addr = sym->address;
                                                                }
                                                                put("%s@0x%x=", sym->name, addr);
                                                                vput(sym->type, addr);
                                                                }
                                                        }
                                                }

                                                put("\n");
                                                n++;
                                        }
                                        context = sym->uplink;
                                }
                                if (n == 0)
                                        put("?Unknown identifier: %s:%s\n", file, name);
                        } else
                                put("?Unknown identifier: %s\n", name);
                        /* <skip white space>=                                                      */
                        while (*p && isspace(*p))
                                p++;

                }
}
/* <cdb static functions>=                                                  */
static void w_cmd(char *line) {
        Nub_state_T tmp;
        int i;

        for (i = 0; _Nub_frame(i, &tmp) == i; i++) {
                if (i == frameno)
                        put("*");
                printframe(1, &tmp, i);
        }
}

static void f_cmd(char *line) {
        char *p = line;

        if (isdigit(*p)) {
                Nub_state_T tmp;
                int n;
                /* <n := an integer>=                                                       */
                for (n = 0; isdigit(*p); p++)
                        n = 10*n + (*p - '0');
;
                if (_Nub_frame(n, &tmp) != n)
                        put("?There is no frame %d\n", n);
                else
                        printframe(2, &tmp, n);
        } else
                printframe(2, &focus, frameno);
}

static void docmds(void) {
        char line[512];
        /* <help message>=                                                          */
        static char help[] =
        "b [file:]line[.character]\n"
        "       set a breakpoint at the specified source coordinate\n"
        "c      continue execution\n"
        "d [n]  move down the call stack 1 frame or n frames\n"
        "f [n]  print everything about the current frame or about frame n\n"
        "h      print this message\n"
        "m [n]  move to frame 0 (the top frame) or to frame n\n"
        "p      list the visible variables as p commands\n"
        "p {id} print the values of the listed identifiers\n"
        "q      quit cdb and the target\n"
        "r      remove the current breakpoint\n"
        "r [file:]line[.character]\n"
        "       remove the breakpoint at the specified source coordinate\n"
        "u [n]  move up the call stack 1 frame or n frames\n"
        "w      display the call stack\n"
        "!cmd   call the shell to execute cmd\n\n"
        "[X] means X is optional, {X} means 0 or more Xs\n";


        prompt();
        while (fgets(line, (int)sizeof line, in) != NULL) {
                char *p = line;
                int c, n;
                /* <skip white space>=                                                      */
                while (*p && isspace(*p))
                        p++;

                c = *p++;
                if (isalpha(c) && isalpha(*p))
                        put("?Unrecognized command: %s", line);
                else {
                        /* <skip white space>=                                                      */
                        while (*p && isspace(*p))
                                p++;

                        switch (c) {
                        case   0: break;
                        case 'b': b_cmd(p); break;
                        case 'f': f_cmd(p); break;
                        case 'r': r_cmd(p); break;
                        case 'p': p_cmd(p); break;
                        case 'w': w_cmd(p); break;
                        case 'm': /* <n := an integer>=                                                       */
                                  for (n = 0; isdigit(*p); p++)
                                          n = 10*n + (*p - '0');
; moveto(n); break;
                        case 'd':
                        case 'u':
                                if (isdigit(*p)) {
                                        /* <n := an integer>=                                                       */
                                        for (n = 0; isdigit(*p); p++)
                                                n = 10*n + (*p - '0');
;
                                } else
                                        n = 1;
                                moveto(frameno + (c == 'u' ? -n : n));
                                break;
                        case 'h': put("%s", help); break;
                        case '!': system(p); break;
                        case 'c': return;
                        case 'q': exit(EXIT_FAILURE);
                        default: put("?Unrecognized command: %s", line); break;
                        }
                }               
                prompt();
        }
}

/* <cdb static functions>=                                                  */
static void onbreak(Nub_state_T state) {
        put("stopped ");
        /* <print stopped location and set focus and frameno>=                      */
        {
                if (state.name[0])
                        put("in %.*s ", sizeof state.name, state.name);
                brkpt = &state.src;
                put("at %s:%d.%d\n", brkpt->file, brkpt->y, brkpt->x);
                focus = state;
                frameno = 0;
                printframe(1, &focus, frameno);
        }
        docmds();
}
/* <cdb functions>=                                                         */
void _Cdb_startup(Nub_state_T state) {
        in = fopen("/dev/tty", "r");
        if (in == NULL)
                in = stdin;
        out = stderr;
        focus = state;
        frameno = -1;
        brkpt = NULL;
        docmds();
}

void _Cdb_fault(Nub_state_T state) {
        put("fault ");
        /* <print stopped location and set focus and frameno>=                      */
        {
                if (state.name[0])
                        put("in %.*s ", sizeof state.name, state.name);
                brkpt = &state.src;
                put("at %s:%d.%d\n", brkpt->file, brkpt->y, brkpt->x);
                focus = state;
                frameno = 0;
                printframe(1, &focus, frameno);
        }
        docmds();
        exit(EXIT_FAILURE);
}
