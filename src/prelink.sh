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
nm $* | awk '
/_module__V/    {
        if (match($0, /_module__V[0-9a-f]*/)) {
                m=substr($0, RSTART, RLENGTH)
                if (!seen[m]) {
                        printf "extern struct module %s;\n", m
                        seen[m] = 1
                } }
        }
END     {
print "struct module *_Nub_modules[] = {"
for (m in seen) printf "\t&%s,\n", m
printf "\t0\n};\n"
}' >$cfile
lcc -c -o "$ofile" $cfile