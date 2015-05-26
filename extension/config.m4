PHP_ARG_ENABLE(trace, whether to enable trace support,
[  --enable-trace          Enable trace support])

if test "$PHP_TRACE" != "no"; then

  dnl check ZTS support
  if test "$PHP_THREAD_SAFETY" == "yes"; then
    AC_MSG_ERROR([currently trace does not support ZTS])
  fi

  dnl check mmap functions
  AC_CHECK_FUNCS(mmap)
  AC_CHECK_FUNCS(munmap)

  PHP_TRACE_COMMON_FILES="\
    common/trace_comm.c \
    common/trace_ctrl.c \
    common/trace_mmap.c \
    common/trace_type.c \
    common/sds/sds.c"

  dnl $ext_srcdir available after PHP_NEW_EXTENSION
  PHP_NEW_EXTENSION(trace, trace.c $PHP_TRACE_COMMON_FILES, $ext_shared)

  dnl configure can't use ".." as a source filename, so we make a link here
  ln -sf $ext_srcdir/../common $ext_srcdir

  dnl add common include path
  PHP_ADD_INCLUDE($ext_srcdir/common)

  PHP_ADD_MAKEFILE_FRAGMENT

fi

dnl vim:et:ts=2:sw=2
