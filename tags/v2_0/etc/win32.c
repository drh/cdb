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
char *ld[] = { "link", "", "-nologo",
	"-align:0x1000", "-subsystem:console", "-entry:mainCRTStartup",
	"$2", "", "-OUT:$3", "$1", LCCDIR "liblcc.lib", "libc.lib", "kernel32.lib", "", 0 };

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
		ld[10] = stringf("%s\\liblcc.lib", lccdir);
	} else if (strcmp(arg, "-b") == 0)
		;
	else if (strncmp(arg, "-ld=", 4) == 0)
		ld[0] = &arg[4];
	else if (strcmp(arg, "-g4") == 0) {
		extern char *tempdir;
		char *tmp = stringf("%s\\%d", tempdir, getpid());
		if (lccdir[strlen(lccdir)-1] == '\\')
			lccdir[strlen(lccdir)-1] = '\0';
		ld[0] = stringf("sh %s\\prelink.sh -o %s.obj", lccdir, tmp);
		ld[1] = "$2";
		ld[2] = "\nlink -nologo";
		ld[7] = stringf("%s.obj %s\\startup.obj %s\\libnub.lib ws2_32.lib",
			tmp, lccdir, lccdir);
		ld[13] = stringf("\nrm %s.obj %s.c", tmp, tmp);
		com[2] = "-g4";
	} else
		return 0;
	return 1;
}
