#!/bin/sh

# $Id$

# prelink -o file files...
# generates list of module pointers in the given object files
# on the standard output as a C program

# set -x
case "$1" in
-o)     ofile=$2; shift; shift ;;
*)      ofile=/tmp/$$.o ;;
esac
cfile=`expr "$ofile" : '\(.*\.\)'`c
for i
do
	case "$i" in
	-*|*.a|*.lib)	;;
	*)	files="$files $i" ;;
	esac
done
nm $files | awk '
/_module_V/    {
        if (match($0, /_module_V[0-9a-f]*/)) {
                m=substr($0, RSTART, RLENGTH)
                if (!seen[m]) {
                        printf "extern struct module %s;\n", m
			seen[m] = 1
		} }
	}
/_spoints_V/	{
	if (match($0, /_[0-9]+/)) {
		n = substr($0, RSTART+1, RLENGTH-1)
		if (max < n) max = n
	} }
END     {
print "struct module *_Nub_modules[] = {"
for (m in seen) printf "\t&%s,\n", m
printf "\t0\n};\n"
printf "char _Nub_bpflags[%d];\n", max + 1
}' >$cfile
lcc -c -o "$ofile" $cfile
