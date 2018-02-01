
AC_PREREQ([2.69])
AC_INIT([sof-tools], [1.0.1])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([rimage/rimage.c])
AC_CONFIG_HEADERS([config.h])
AC_CHECK_HEADER([sof/uapi/manifest.h])

AC_CANONICAL_HOST

dnl Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

AC_PROG_CC

#check for openssl
AC_CHECK_LIB([crypto], [OPENSSL_config], , [AC_MSG_ERROR([Need OpenSSL libcrypto])])

PEM_KEY_PREFIX="/usr/local/share/rimage"
AC_DEFINE_UNQUOTED([PEM_KEY_PREFIX], ["$PEM_KEY_PREFIX"], ["Path for PEM keys"])
AC_SUBST(PEM_KEY_PREFIX)

AC_OUTPUT([
	Makefile
	rimage/Makefile
	rimage/keys/Makefile
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
PEM:                           ${PEM_KEY_PREFIX}
Compiler:                      ${CC}
CFLAGS:                        ${CFLAGS}
"
