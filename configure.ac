AC_PREREQ([2.69])
AC_INIT([Reef],[m4_esyscmd(./git-version.sh)],[sound-open-firmware@alsa-project.org])
AC_CONFIG_SRCDIR([src/init/init.c])
AC_CONFIG_HEADERS([src/include/config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign 1.11 -Wall -Wno-portability subdir-objects silent-rules color-tests dist-xz tar-ustar])

# Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

# get version info from git
m4_define(reef_major, `cat .version | cut -dv -f2 | cut -d. -f1`)
m4_define(reef_minor, `cat .version | cut -d. -f2 | cut -d- -f1`)
AC_DEFINE_UNQUOTED([REEF_MAJOR], reef_major, [Reef major version])
AC_DEFINE_UNQUOTED([REEF_MINOR], reef_minor, [Reef minor version])

AC_CANONICAL_HOST

# General compiler flags
CFLAGS="${CFLAGS:+$CFLAGS } -O2 -g -Wall -Werror -Wl,-EL -fno-inline-functions -nostdlib -Wmissing-prototypes"

# Cross compiler tool libgcc and headers
AC_ARG_WITH([root-dir],
        AS_HELP_STRING([--with-root-dir], [Specify location of cross gcc libraries and headers]),
        [], [with_root_dir=no])
AS_IF([test "x$with_root_dir" = xno],
	AC_MSG_ERROR([Please specify cross compiler root header directory]),
	[ROOT_DIR=$with_root_dir])
AC_SUBST(ROOT_DIR)

# Architecture support
AC_ARG_WITH([arch],
        AS_HELP_STRING([--with-arch], [Specify DSP architecture]),
	[], [with_arch=no])

case "$with_arch" in
    xtensa*)

	ARCH_CFLAGS="-mtext-section-literals"
	AC_SUBST(ARCH_CFLAGS)

	ARCH_LDFLAGS="-nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static"
	AC_SUBST(XTENSA_LDFLAGS)

	# extra CFLAGS defined here otherwise configure working gcc tests fails.
	CFLAGS="${CFLAGS:+$CFLAGS }-mlongcalls"
	LDFLAGS="${LDFLAGS:+$LDFLAGS }-nostdlib"

	#ARCH_ASFLAGS=""
	AC_SUBST(ARCH_ASFLAGS)

	ARCH="xtensa"
	AC_SUBST(ARCH)
    ;;
    *)
        AC_MSG_ERROR([DSP architecture not specified])
    ;;
esac

AM_CONDITIONAL(BUILD_XTENSA, test "$ARCH" = "xtensa")


# Platform support
AC_ARG_WITH([platform],
        AS_HELP_STRING([--with-platform], [Specify Host Platform]),
	[], [with_platform=no])

case "$with_platform" in
    baytrail*)

	PLATFORM_LDSCRIPT="baytrail.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="baytrail"
	AC_SUBST(PLATFORM)

	FW_NAME="byt"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifiep_bd5"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_BAYTRAIL], [1], [Configure for Baytrail])
	AC_DEFINE([CONFIG_HOST_PTABLE], [1], [Configure handling host page table])
    ;;
    cherrytrail*)

	PLATFORM_LDSCRIPT="baytrail.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="baytrail"
	AC_SUBST(PLATFORM)

	FW_NAME="cht"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifiep_bd5"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_CHERRYTRAIL], [1], [Configure for Cherrytrail])
	AC_DEFINE([CONFIG_HOST_PTABLE], [1], [Configure handling host page table])
    ;;
    *)
        AC_MSG_ERROR([Host platform not specified])
    ;;
esac

AM_CONDITIONAL(BUILD_BAYTRAIL, test "$FW_NAME" = "byt")
AM_CONDITIONAL(BUILD_CHERRYTRAIL,  test "$FW_NAME" = "cht")
AM_CONDITIONAL(BUILD_HASWELL,  test "$FW_NAME" = "hsw")
AM_CONDITIONAL(BUILD_BROADWELL,  test "$FW_NAME" = "bdw")
AM_CONDITIONAL(BUILD_APOLLOLAKE,  test "$FW_NAME" = "apl")
# DSP core support (Optional)
AC_ARG_WITH([dsp-core],
        AS_HELP_STRING([--with-dsp-core], [Specify DSP Core]),
	[], [with_dsp_core=no])

case "$with_dsp_core" in
    CHT_audio_hifiep*)
	# BXT/CHT DSP Core
	XTENSA_CORE="CHT_audio_hifiep"
	AC_SUBST(XTENSA_CORE)
    ;;

esac

# dma trace support (Optional), dma trace by default
AC_ARG_ENABLE([dma-trace],
        AS_HELP_STRING([--disable-dma-trace], [Disabled dma trace and use fallback mailbox trace]))

AS_IF([test "x$enable_dma_trace" != "xno"], [
	AC_DEFINE([CONFIG_DMA_TRACE], [1], [Configure DMA trace])
	])

# Test after CFLAGS set othewise test of cross compiler fails. 
AM_PROG_AS
AM_PROG_AR
AC_PROG_CC
LT_INIT
AC_CHECK_TOOL([OBJCOPY], [objcopy], [])
AC_CHECK_TOOL([OBJDUMP], [objdump], [])

AM_EXTRA_RECURSIVE_TARGETS([bin])

AM_EXTRA_RECURSIVE_TARGETS([vminstall])

AC_CONFIG_FILES([
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
        src/math/Makefile
	src/drivers/Makefile
	src/include/Makefile
	src/include/reef/Makefile
	src/include/reef/audio/Makefile
	src/include/uapi/Makefile
	src/ipc/Makefile
	src/lib/Makefile
	src/platform/Makefile
	src/platform/baytrail/Makefile
	src/platform/baytrail/include/Makefile
	src/platform/baytrail/include/platform/Makefile
	src/platform/baytrail/include/xtensa/Makefile
	src/platform/baytrail/include/xtensa/config/Makefile
])
AC_OUTPUT

echo "
---{ $PACKAGE_NAME $VERSION }---

Target Architecture:           ${ARCH}
Target Platform:               ${PLATFORM}
Target Core:                   ${XTENSA_CORE}

Compiler:                      ${CC}
CFLAGS:                        ${CFLAGS}
LDFLAGS:                       ${LDFLAGS}
ARCH_CFLAGS:                   ${ARCH_CFLAGS}
ARCH_LDFLAGS:                  ${ARCH_LDFLAGS}
"

