PHP_ARG_ENABLE(phptrace, whether to enable phptrace support,
[  --enable-phptrace   Enable phptrace support])

if test "$PHP_PHPTRACE" != "no"; then

  dnl check ZTS support
  if test "$PHP_THREAD_SAFETY" == "yes"; then
    AC_MSG_ERROR([currently trace does not support ZTS])
  fi

  dnl check mmap functions
  AC_CHECK_FUNCS(mmap)
  AC_CHECK_FUNCS(munmap)

  dnl add common include path
  PHP_ADD_INCLUDE(../common)

  dnl configure can't use ".." as a source filename, so we make a link here
  ln -sf ../common ./

  PHPTRACE_SOURCES="common/phptrace_comm.c \
                    common/phptrace_ctrl.c \
                    common/phptrace_mmap.c \
                    common/phptrace_type.c \
                    common/sds/sds.c"

  PHP_NEW_EXTENSION(phptrace, phptrace.c $PHPTRACE_SOURCES, $ext_shared)
fi

dnl vim:et:ts=2:sw=2
