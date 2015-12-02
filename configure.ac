
AC_PREREQ([2.69])
AC_INIT([reef], [0.1])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_SRCDIR([src/init/init.c])
AC_CONFIG_HEADERS([src/include/config.h])

AC_CANONICAL_HOST

dnl Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

# General compiler flags
CFLAGS="${CFLAGS:+$CFLAGS }-Os -O2 -g -Wpointer-arith -Werror -Wl,-EL -fno-inline-functions -nostdlib"

# Cross compiler tool libgcc and headers
AC_ARG_WITH([tool-dir],
        AS_HELP_STRING([--with-tool-dir], [Specify location of cross gcc libraries and headers]),
        [], [with_tool_dir=no])
AS_IF([test "x$with_tool_dir" = xno],
	AC_MSG_ERROR([Please specify cross compiler tool directory]),
	[TOOL_DIR=$with_tool_dir])
AC_SUBST(TOOL_DIR)

# Architecture support
AC_ARG_WITH([arch],
        AS_HELP_STRING([--with-arch], [Specify DSP architecture]),
	[], [with_arch=no])

case "$with_arch" in
    xtensa*)
	AM_CONDITIONAL(HAVE_XTENSA, true)

	ARCH_CFLAGS="-mtext-section-literals  -D__XTENSA_CALL0_ABI__"
	AC_SUBST(ARCH_CFLAGS)

	ARCH_LDFLAGS="-nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static"
	AC_SUBST(XTENSA_LDFLAGS)

	# extra CFLAGS defined here otherwise configure working gcc tests fails.
	CFLAGS="${CFLAGS:+$CFLAGS }-mlongcalls"
	LDFLAGS="${LDFLAGS:+$LDFLAGS }-nostdlib"

	ARCH_ASFLAGS="-D__XTENSA_CALL0_ABI__"
	AC_SUBST(ARCH_ASFLAGS)

	ARCH="xtensa"
	AC_SUBST(ARCH)
    ;;
    *)
        AC_MSG_ERROR([DSP architecture not specified])
    ;;
esac


# Platform support
AC_ARG_WITH([platform],
        AS_HELP_STRING([--with-platform], [Specify Host Platform]),
	[], [with_platform=no])

case "$with_platform" in
    baytrail*)
	AM_CONDITIONAL(HAVE_BAYTRAIL, true)

       	PLATFORM_LDSCRIPT="elf32xtensa.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="baytrail"
	AC_SUBST(PLATFORM)

	XTENSA_CORE="hifi2_std"
	AC_SUBST(XTENSA_CORE)
    ;;
    *)
        AC_MSG_ERROR([Host platform not specified])
    ;;
esac

# Test after CFLAGS set othewise test of cross compiler fails. 
AM_PROG_AS
AM_PROG_AR
AC_PROG_CC
AC_PROG_RANLIB
AC_CHECK_TOOL([OBJCOPY], [objcopy], [])
AC_CHECK_TOOL([OBJDUMP], [objdump], [])

AM_EXTRA_RECURSIVE_TARGETS([bin])

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
	src/audio/Makefile
	src/drivers/Makefile
	src/include/Makefile
	src/ipc/Makefile
	src/lib/Makefile
	src/platform/Makefile
	src/platform/baytrail/Makefile
	src/platform/baytrail/include/Makefile
	src/platform/baytrail/include/platform/Makefile
	src/platform/baytrail/include/xtensa/Makefile
	src/platform/baytrail/include/xtensa/config/Makefile
])

echo "
---{ $PACKAGE_NAME $VERSION }---

Target Architecture:           ${ARCH}
Target Platform:               ${PLATFORM}

Compiler:                      ${CC}
CFLAGS:                        ${CFLAGS}
LDFLAGS:                       ${LDFLAGS}
ARCH_CFLAGS:                   ${ARCH_CFLAGS}
ARCH_LDFLAGS:                  ${ARCH_LDFLAGS}
"

