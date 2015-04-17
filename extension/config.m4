dnl $Id$
dnl config.m4 for extension phptrace

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(phptrace, for phptrace support,
dnl Make sure that the comment is aligned:
dnl [  --with-phptrace             Include phptrace support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(phptrace, whether to enable phptrace support,
dnl Make sure that the comment is aligned:
dnl [  --enable-phptrace           Enable phptrace support])

if test "$PHP_PHPTRACE" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-phptrace -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/phptrace.h"  # you most likely want to change this
  dnl if test -r $PHP_PHPTRACE/$SEARCH_FOR; then # path given as parameter
  dnl   PHPTRACE_DIR=$PHP_PHPTRACE
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for phptrace files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       PHPTRACE_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$PHPTRACE_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the phptrace distribution])
  dnl fi

  dnl # --with-phptrace -> add include path
  dnl PHP_ADD_INCLUDE($PHPTRACE_DIR/include)

  dnl # --with-phptrace -> check for lib and symbol presence
  dnl LIBNAME=phptrace # you may want to change this
  dnl LIBSYMBOL=phptrace # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $PHPTRACE_DIR/$PHP_LIBDIR, PHPTRACE_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_PHPTRACELIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong phptrace lib version or lib not found])
  dnl ],[
  dnl   -L$PHPTRACE_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(PHPTRACE_SHARED_LIBADD)

  PHP_NEW_EXTENSION(phptrace, phptrace.c, $ext_shared)
fi
