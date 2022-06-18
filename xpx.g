gcc=g++
src=xpx.c
out=xpx.e
cairoinc="-I/usr/include/cairo"
xlibinc="-I/usr/include"
xlibdir="-L/usr/lib"
include="$xlibinc $xlibdir $cairoinc"
libs="-lX11 -lcairo"
# include="$cairoinc"
cmd="$gcc $include -o $out $src $libs"
echo "$cmd"
$cmd

