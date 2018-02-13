
AC_PREREQ([2.69])
AC_INIT([sof-tools], [1.0.1])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([rmbox/rmbox.c])
AC_CONFIG_HEADERS([config.h])
AC_CHECK_HEADER([sof/uapi/abi.h])

AC_CANONICAL_HOST

dnl Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

AC_PROG_CC

AC_OUTPUT([
	Makefile
	rmbox/Makefile
	topology/Makefile
	topology/common/Makefile
	topology/dsps/Makefile
	topology/m4/Makefile
	topology/sof/Makefile
	topology/test/Makefile
])

echo "

prefix:                        ${prefix}
Compiler:                      ${CC}
CFLAGS:                        ${CFLAGS}
"
