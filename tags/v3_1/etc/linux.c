/* x86s running Linux */

#include <string.h>

static char rcsid[] = "$Id$";

#ifndef LCCDIR
#define LCCDIR "/usr/local/lib/lcc/"
#endif

char *suffixes[] = { ".c", ".i", ".s", ".o", ".out", 0 };
char inputs[256] = "";
char *cpp[] = { LCCDIR "gcc/cpp",
	"-U__GNUC__", "-D_POSIX_SOURCE", "-D__STDC__=1", "-D__STRICT_ANSI__",
	"-Dunix", "-Di386", "-Dlinux",
	"-D__unix__", "-D__i386__", "-D__linux__", "-D__signed__=signed",
	"$1", "$2", "$3", 0 };
char *include[] = {"-I" LCCDIR "include", "-I" LCCDIR "gcc/include", "-I/usr/include", 0 };
char *com[] = {LCCDIR "rcc", "-target=x86/linux", "", "$1", "$2", "$3", 0 };
char *as[] = { "/usr/bin/as", "-o", "$3", "$1", "$2", 0 };
static char *ld2[] = {
	/*  0 */ "/bin/sh",
	/*  1 */ LCCDIR "prelink.sh", "-o",
	/*  3 */ ".o", "$2", "\n",
	/*  6 */ "/usr/bin/ld", "-m", "elf_i386", "-dynamic-linker",
	/* 10 */ "/lib/ld-linux.so.2", "-o", "$3",
	/* 13 */ LCCDIR "startup.o", "/usr/lib/crti.o",
	/* 15 */ LCCDIR "gcc/crtbegin.o", "$1", "$2",
	/* 18 */ ".o",
	/* 19 */ "-L" LCCDIR, "-llcc", "-lnub", "-lnsl",
	/* 23 */ "-L" LCCDIR "gcc", "-lgcc", "-lm", "-lc",
	/* 27 */ "",
	/* 28 */ LCCDIR "gcc/crtend.o", "/usr/lib/crtn.o",
	/* 30 */ "\n", "/bin/rm", "-f",
	/* 33 */ ".o",
	/* 34 */ ".c",
	/* 35 */ 0
};
char *ld[sizeof ld2/sizeof ld2[0]] = {
	/*  0 */ "/usr/bin/ld", "-m", "elf_i386", "-dynamic-linker",
	/*  4 */ "/lib/ld-linux.so.2", "-o", "$3",
	/*  7 */ "/usr/lib/crt1.o", "/usr/lib/crti.o",
	/*  9 */ LCCDIR "/gcc/crtbegin.o", 
                 "$1", "$2",
	/* 12 */ "-L" LCCDIR,
	/* 13 */ "-llcc",
	/* 14 */ "-L" LCCDIR "/gcc", "-lgcc", "-lc", "-lm",
	/* 18 */ "",
	/* 19 */ LCCDIR "/gcc/crtend.o", "/usr/lib/crtn.o",
	0 };

extern char *concat(char *, char *);
extern char *stringf(const char *, ...);

int option(char *arg) {
  	if (strncmp(arg, "-lccdir=", 8) == 0) {
		cpp[0] = concat(&arg[8], "/gcc/cpp");
		include[0] = stringf("-I%s/include", &arg[8]);
		include[1] = stringf("-I%s/gcc/include", &arg[8]);
		ld[9]  = concat(&arg[8], "/gcc/crtbegin.o");
		ld[12] = concat("-L", &arg[8]);
		ld[14] = stringf("-L%s/gcc",&arg[8]);
		ld[19] = concat(&arg[8], "/gcc/crtend.o");
		ld2[1] = concat(&arg[8], "/prelink.sh");
		ld2[13] = concat(&arg[8], "/startup.o");
		ld2[15] = concat(&arg[8], "/gcc/crtbegin.o");
		ld2[19] = concat("-L", &arg[8]);
		ld2[23] = stringf("-L%s/gcc", &arg[8]);
		ld2[28] = concat(&arg[8], "/gcc/crtend.o");
		com[0] = concat(&arg[8], "/rcc");
	} else if (strcmp(arg, "-p") == 0 || strcmp(arg, "-pg") == 0) {
		ld[7] = "/usr/lib/gcrt1.o";
		ld[18] = "-lgmon";
	} else if (strcmp(arg, "-b") == 0) 
		;
	else if (strcmp(arg, "-g") == 0)
		;
	else if (strncmp(arg, "-ld=", 4) == 0)
		ld[0] = &arg[4];
	else if (strcmp(arg, "-static") == 0) {
	        ld[3] = "-static";
	        ld[4] = "";
	} else if (strcmp(arg, "-g4") == 0) {
		extern char *tempdir;
		extern int getpid(void);
		ld2[3] = stringf("%s/%d.o", tempdir, getpid());
		ld2[18] = ld2[3];
		ld2[33] = ld2[3];
		ld2[34] = stringf("%s/%d.c", tempdir, getpid());
		memcpy(ld, ld2, sizeof ld2);
		com[2] = "-g4";
	} else
		return 0;
	return 1;
}
