/* x86s running MS Windows NT 4.0 */

#include <string.h>
#include <assert.h>

static char rcsid[] = "$Id$";

#ifndef LCCDIR
#define LCCDIR "\\pkg\\lcc-4.0\\"
#endif

static char lccdir[512] = LCCDIR;
char *suffixes[] = { ".c;.C", ".i;.I", ".asm;.ASM;.s;.S", ".obj;.OBJ", ".exe", 0 };
char inputs[256] = "";
char *cpp[] = { LCCDIR "cpp", "-D__STDC__=1", "-Dwin32", "-D_WIN32", "-D_M_IX86",
	"$1", "$2", "$3", 0 };
char *include[] = {
	"-I" LCCDIR "include",
	"-I\"/program files/devstudio/vc/include\"",
	"-I/msdev/include", 0 };
char *com[] = { LCCDIR "rcc", "-target=x86/win32", "", "$1", "$2", "$3", 0 };
char *as[] = { "ml", "-nologo", "-c", "-Cp", "-coff", "-Fo$3", "$1", "$2", 0 };
static char *ld2[] = {
	/*  0 */ "sh",
	/*  1 */ LCCDIR "prelink.sh", "-o",
	/*  3 */ ".obj", "$2", "\n",
	/*  6 */ "link", "-nologo", "-align:0x1000", "-subsystem:console", "-entry:mainCRTStartup",
	/* 11 */ "-OUT:$3", "$1", "$2",
	/* 14 */ ".obj",
	/* 15 */ LCCDIR "startup.obj",
	/* 16 */ LCCDIR "libnub.lib",
	/* 17 */ "ws2_32.lib",
	/* 18 */ LCCDIR "liblcc.lib", "libc.lib", "kernel32.lib", "\n",
	/* 22 */ "rm", "-f",
	/* 24 */ ".obj",
	/* 25 */ ".c",
	0
	};
char *ld[sizeof ld2/sizeof ld2[0]] = {
	/*  0 */ "link", "-nologo", "-align:0x1000", "-subsystem:console", "-entry:mainCRTStartup",
	/*  5 */ "$2", "-OUT:$3", "$1",
	/*  8 */ LCCDIR "liblcc.lib", "libc.lib", "kernel32.lib",
	/* 11 */ 0
	};

extern char *stringf(const char *, ...);
extern char *replace(const char *, int, int);

int option(char *arg) {
	if (strncmp(arg, "-lccdir=", 8) == 0) {
		strcpy(lccdir, replace(arg + 8, '/', '\\'));
		if (lccdir[strlen(lccdir)-1] == '\\')
			lccdir[strlen(lccdir)-1] = '\0';
		cpp[0] = stringf("%s\\cpp.exe", lccdir);
		include[0] = stringf("-I%s\\include", lccdir);
		com[0] = stringf("%s\\rcc.exe", lccdir);
		ld[8] = stringf("%s\\liblcc.lib", lccdir);
		ld2[1] = stringf("%s\\prelink.sh", lccdir);
		ld2[15] = stringf("%s\\startup.obj", lccdir);
		ld2[16] = stringf("%s\\libnub.lib", lccdir);
		ld2[18] = ld[8];
	} else if (strcmp(arg, "-b") == 0)
		;
	else if (strncmp(arg, "-ld=", 4) == 0)
		ld[0] = &arg[4];
	else if (strcmp(arg, "-g4") == 0) {
		extern char *tempdir;
		ld2[3] = stringf("%s\\%d.obj", tempdir, getpid());
		ld2[14] = ld2[3];
		ld2[24] = ld2[3];
		ld2[25] = stringf("%s\\%d.c", tempdir, getpid());
		memcpy(ld, ld2, sizeof ld2);
		com[2] = "-g4";
	} else
		return 0;
	return 1;
}
