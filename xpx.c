#if 0
gen=xpx.g
out=xpx.e
if [ "$1" != "-" ] ; then
  rm ./$out
  ./$gen
fi
if [ -e ./$out ] ; then
  ./$out
else
  echo "compile failed"
fi
#endif

#include "xpx.h"
