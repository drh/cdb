/* SPARCs running Solaris 2.5.1 at CS Dept., Princeton University */

#include <string.h>

static char rcsid[] = "$Id: solaris.c,v 1.4 1997/08/01 16:50:35 drh Exp $";

#ifndef LCCDIR
#define LCCDIR "/usr/local/lib/lcc/"
#endif
#ifndef SUNDIR
#define SUNDIR "/opt/SUNWspro/SC4.0/lib/"
#endif

static char lccdir[512] = LCCDIR;
char *suffixes[] = { ".c", ".i", ".s", ".o", ".out", 0 };
char inputs[256] = "";
char *cpp[] = { LCCDIR "cpp",
	"-D__STDC__=1", "-Dsparc", "-D__sparc__", "-Dsun", "-D__sun__", "-Dunix",
	"$1", "$2", "$3", 0 };
char *include[] = { "-I" LCCDIR "include", "-I/usr/local/include",
	"-I/usr/include", 0 };
char *com[] = { LCCDIR "rcc", "-target=sparc/solaris", "",
	"$1", "$2", "$3", 0 };
char *as[] = { "/usr/ccs/bin/as", "-Qy", "-s", "-o", "$3", "$1", "$2", 0 };
static char *ld2[] = {
	/*  0 */ "/bin/sh",
	/*  1 */ LCCDIR "prelink.sh", "-o",
	/*  3 */ ".o", "$2", "\n",
	/*  6 */ "/usr/ccs/bin/ld", "-o", "$3", "$1", SUNDIR "crti.o",
	/* 11 */ SUNDIR "startup.o", SUNDIR "values-xa.o", "$2",
	/* 14 */ ".o", "-Y",
	/* 16 */ "P," SUNDIR ":/usr/ccs/lib:/usr/lib", "-Qy",
	/* 18 */ "-L" LCCDIR, "-llcc", "-lnub", "-lsocket", "-lnsl", "-lm", "-lc", SUNDIR "crtn.o",
	/* 26 */ "\n", "/bin/rm", "-f",
	/* 29 */ ".o",
	/* 30 */ ".c",
	/* 31 */ 0
};
char *ld[sizeof ld2/sizeof ld2[0]] = {
	/*  0 */ "/usr/ccs/bin/ld", "-o", "$3", "$1", SUNDIR "crti.o",
	/*  5 */ SUNDIR "crt1.o", SUNDIR "values-xa.o", "$2", "-Y",
	/*  9 */ "P," SUNDIR ":/usr/ccs/lib:/usr/lib", "-Qy",
	/* 11 */ "-L" LCCDIR, "-llcc", "-lm", "-lc", SUNDIR "crtn.o",
	/* 16 */ 0
	};

extern char *stringf(const char *, ...);

int option(char *arg) {
	if (strncmp(arg, "-lccdir=", 8) == 0) {
		strcpy(lccdir, &arg[8]);
		if (lccdir[strlen(lccdir)-1] == '/')
			lccdir[strlen(lccdir)-1] = '\0';
		cpp[0] = stringf("%s/cpp", lccdir);
		include[0] = stringf("-I%s/include", lccdir);
		ld[11] = stringf("-L%s", lccdir);
		com[0] = stringf("%s/rcc", lccdir);
		ld2[1] = stringf("%s/prelink.sh", lccdir);
		ld2[11] = stringf("%s/startup.o", lccdir);
		ld2[18] = ld[11];
	} else if (strcmp(arg, "-g") == 0)
		;
	else if (strcmp(arg, "-p") == 0) {
		ld[5] = SUNDIR "mcrt1.o";
		ld[9] = "P," SUNDIR "libp:/usr/ccs/lib/libp:/usr/lib/libp:"
			 SUNDIR ":/usr/ccs/lib:/usr/lib";
	} else if (strcmp(arg, "-b") == 0)
		;
	else if (strncmp(arg, "-ld=", 4) == 0) {
		ld[0] = &arg[4];
		ld2[0] = &arg[4];
	}  else if (strcmp(arg, "-g4") == 0) {
		extern char *tempdir;
		extern int getpid(void);
		ld2[3] = stringf("%s/%d.o", tempdir, getpid());
		ld2[14] = ld2[3];
		ld2[29] = ld2[3];
		ld2[30] = stringf("%s/%d.c", tempdir, getpid());
		memcpy(ld, ld2, sizeof ld2);
		com[2] = "-g4";
	} else
		return 0;
	return 1;
}
