dnl $Id$
dnl config.m4 for extension trace

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(trace, for trace support,
dnl Make sure that the comment is aligned:
dnl [  --with-trace             Include trace support])

dnl Otherwise use enable:

PHP_ARG_ENABLE(trace, whether to enable trace support,
[  --enable-trace           Enable trace support])

if test "$PHP_TRACE" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-trace -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/trace.h"  # you most likely want to change this
  dnl if test -r $PHP_TRACE/$SEARCH_FOR; then # path given as parameter
  dnl   TRACE_DIR=$PHP_TRACE
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for trace files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       TRACE_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$TRACE_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the trace distribution])
  dnl fi

  dnl # --with-trace -> add include path
  dnl PHP_ADD_INCLUDE($TRACE_DIR/include)

  dnl # --with-trace -> check for lib and symbol presence
  dnl LIBNAME=trace # you may want to change this
  dnl LIBSYMBOL=trace # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $TRACE_DIR/lib, TRACE_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_TRACELIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong trace lib version or lib not found])
  dnl ],[
  dnl   -L$TRACE_DIR/lib -lm
  dnl ])
  dnl
  dnl PHP_SUBST(TRACE_SHARED_LIBADD)

  PHP_NEW_EXTENSION(trace, phptrace.c util/phptrace_mmap.c util/phptrace_string.c util/phptrace_protocol.c, $ext_shared)
fi
