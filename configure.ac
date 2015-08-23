
AC_PREREQ([2.69])
AC_INIT([reef], [0.1])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/init/init.c])
AC_CONFIG_HEADERS([src/include/config.h])

AC_CANONICAL_HOST

dnl Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

# Architecture support
AC_ARG_ENABLE(xtensa, [AS_HELP_STRING([--enable-xtensa],[enable xtensa architecture])], have_xtensa=$enableval, have_xtensa=yes)
if test "$have_xtensa" = "yes"; then
        AC_DEFINE(HAVE_XTENSA,1,[Define to enable xtensa architecture.])
	XTENSA_CFLAGS="-std=c99 -Os -g -O2 -Wpointer-arith -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__XTENSA_CALL0_ABI__"
	AC_SUBST(XTENSA_CFLAGS)

	XTENSA_LDFLAGS="-nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static"
	AC_SUBST(XTENSA_LDFLAGS)

	CFLAGS="${CFLAGS:+$CFLAGS }-mlongcalls"
	LDFLAGS="${LDFLAGS:+$LDFLAGS }-nostdlib"

	XTENSA_ASFLAGS="-D__XTENSA_CALL0_ABI__"
	AC_SUBST(XTENSA_ASFLAGS)

fi
AM_CONDITIONAL(HAVE_XTENSA, test "$have_xtensa" = "yes")

# Platform support
AC_ARG_ENABLE(baytrail, [AS_HELP_STRING([--enable-baytrail],[enable baytrail platform])], have_baytrail=$enableval, have_baytrail=yes)
if test "$have_baytrail" = "yes"; then
       
	XTENSA_LDSCRIPT="elf32xtensa.x"
	AC_SUBST(XTENSA_LDSCRIPT)

	PLATFORM_DIR="baytrail"
	AC_SUBST(PLATFORM_DIR)

	XTENSA_CORE = hifi2_std
	AC_SUBST(XTENSA_CORE)
fi
AM_CONDITIONAL(HAVE_BAYTRAIL, test "$have_baytrail" = "yes")

AM_PROG_AS
AM_PROG_AR
AC_PROG_CC
AC_PROG_RANLIB
AC_CHECK_TOOL([OBJCOPY], [objcopy], [])

AM_EXTRA_RECURSIVE_TARGETS([bin])

REEF_LIBDIR="-L ../../init -L ../../tasks"
AC_SUBST(REEF_LIBDIR)

REEF_LIBS="-linit -ltasks"
AC_SUBST(REEF_LIBS)

AC_OUTPUT([
	Makefile
	src/Makefile
	src/tasks/Makefile
	src/init/Makefile
	src/arch/Makefile
	src/arch/xtensa/Makefile
	src/arch/xtensa/include/Makefile
	src/arch/xtensa/hal/Makefile
	src/arch/xtensa/xtos/Makefile
])
