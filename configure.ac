AC_PREREQ([2.69])
AC_INIT([sof],[m4_esyscmd(./version.sh)],[sound-open-firmware@alsa-project.org])
AC_CONFIG_SRCDIR([src/init/init.c])
AC_CONFIG_HEADERS([src/include/config.h])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign 1.11 -Wall -Wno-portability subdir-objects silent-rules color-tests dist-xz tar-ustar])

# Initialize maintainer mode
AM_MAINTAINER_MODE([enable])

# get version info from git
m4_define(sof_major, `cat .version | cut -dv -f2 | cut -d. -f1`)
m4_define(sof_minor, `cat .version | cut -d. -f2 | cut -d- -f1`)
m4_define(sof_micro, `cat .version | cut -d. -f3 | cut -d- -f1`)
AC_DEFINE_UNQUOTED([SOF_MAJOR], sof_major, [Sof major version])
AC_DEFINE_UNQUOTED([SOF_MINOR], sof_minor, [Sof minor version])
AC_DEFINE_UNQUOTED([SOF_MICRO], sof_micro, [Sof micro version])

AC_CANONICAL_HOST

# Macro to set flag if supported
AC_DEFUN([ADD_OPTIONAL_AM_CFLAGS],
         [SAVED_CFLAGS="$CFLAGS"
          CFLAGS="-Werror $1"
          AC_MSG_CHECKING([whether $CC supports $1])
          AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main(void) { return 0; }])],
                            [AC_MSG_RESULT([yes])]
                            [AM_CFLAGS="$AM_CFLAGS $1"],
                            [AC_MSG_RESULT([no])])
          CFLAGS="$SAVED_CFLAGS"])

# General assembler flags
ASFLAGS="-DASSEMBLY"
AC_SUBST(ASFLAGS)

# Cross compiler tool libgcc and headers
AC_ARG_WITH([root-dir],
		AS_HELP_STRING([--with-root-dir], [Specify location of cross gcc libraries and headers]),
		[], [with_root_dir=no])

# MEU location
AC_ARG_WITH([meu],
		AS_HELP_STRING([--with-meu], [Specify location of MEU tool]),
		[], [with_meu=no])
if test "x$with_meu" != "xno"; then
	MEU_PATH="$with_meu"
	AC_SUBST(MEU_PATH)

	MEU_VERSION=$($with_meu/meu -ver | grep "Version:" | cut -d" " -f6)
	AX_COMPARE_VERSION([$MEU_VERSION], [ge], [12.0.0.1035], [MEU_OFFSET=1088], [MEU_OFFSET=1152])
	AC_SUBST(MEU_OFFSET)
fi
AM_CONDITIONAL(USE_MEU, test "x$with_meu" != "xno")

# Private key location
AC_ARG_WITH([key],
		AS_HELP_STRING([--with-key], [Specify location of private key]),
		[], [with_key=no])
if test "x$with_meu" != "xno"; then
	if test "x$with_key" != "xno"; then
		PRIVATE_KEY="$with_key"
		AC_SUBST(PRIVATE_KEY)
	else
		AC_MSG_ERROR([Private key location not specified])
	fi
fi

# check if we are building FW image or library
AC_ARG_ENABLE(library, [AS_HELP_STRING([--enable-library],[build library])], have_library=$enableval, have_library=no)
if test "$have_library" = "yes"; then
	AC_DEFINE([CONFIG_LIB], [1], [Configure for Shared Library])
fi
AM_CONDITIONAL(BUILD_LIB, test "$have_library" = "yes")

# check if we are building tools
AC_ARG_ENABLE(rimage, [AS_HELP_STRING([--enable-rimage],[build rimage tool])], have_rimage=$enableval, have_rimage=no)
IMPLICIT_FALLTHROUGH_FLAG=""
if test "$have_rimage" = "yes"; then
	AC_DEFINE([CONFIG_RIMAGE], [1], [Configure to build rimage])
	AM_CFLAGS="-O2 -g -Wall -Werror -Wl,-EL -Wmissing-prototypes"
	IMPLICIT_FALLTHROUGH_FLAG="-Wimplicit-fallthrough=3"
fi
AM_CONDITIONAL(BUILD_RIMAGE, test "$have_rimage" = "yes")

# check if we are building docs
AC_ARG_ENABLE(doc, [AS_HELP_STRING([--enable-doc],[build doc])], have_doc=$enableval, have_doc=no)
if test "$have_doc" = "yes"; then
	AC_DEFINE([CONFIG_DOC], [1], [Configure to build doc])
fi
AM_CONDITIONAL(BUILD_DOC, test "$have_doc" = "yes")

# Disable DMIC driver if requested, by default build for supported platforms
AC_ARG_ENABLE([dmic], AS_HELP_STRING([--disable-dmic], [Disable DMIC driver]))
AS_IF([test "x$enable_dmic" != "xno"], [
	AC_DEFINE([CONFIG_DMIC], [1], [Configure to build DMIC driver])
])

# Architecture support
AC_ARG_WITH([arch],
        AS_HELP_STRING([--with-arch], [Specify DSP architecture]),
	[], [with_arch=no])

case "$with_arch" in
    xtensa*)

	ARCH_CFLAGS="-mtext-section-literals"

	ARCH_LDFLAGS="-nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static"

	# automake FLAGS defined here
	AM_CFLAGS="-fno-inline-functions -nostdlib -mlongcalls"
	AM_LDFLAGS=""
	AM_CCASFLAGS=""

	# GCC needs these additional flags on top of any user flags.
	CFLAGS="${CFLAGS:+$CFLAGS } -O2 -g -Wall -Werror -Wl,-EL -Wmissing-prototypes"
	LDFLAGS="${LDFLAGS:+$LDFLAGS } -nostdlib"
	CCASFLAGS="${CCASFLAGS:+$CCASFLAGS } -fno-inline-functions -nostdlib -mlongcalls"

	ARCH="xtensa"
	AC_SUBST(ARCH)

	AS_IF([test "x$with_root_dir" = xno],
	AC_MSG_ERROR([Please specify cross compiler root header directory]),
		[ROOT_DIR=$with_root_dir])
		AC_SUBST(ROOT_DIR)
    ;;
    host*)

	ARCH_CFLAGS="-g"

	# automake FLAGS defined here
	AM_CFLAGS="-O3 -Wall -Werror -Wl,-EL -Wmissing-prototypes"
	IMPLICIT_FALLTHROUGH_FLAG="-Wimplicit-fallthrough=3"
	AM_LDFLAGS="-lpthread"
	AM_CCASFLAGS="-O3 -Wall -Werror -Wl,-EL -Wmissing-prototypes"

	ARCH="host"
	AC_SUBST(ARCH)
	AC_DEFINE([CONFIG_HOST], [1], [Configure for Host])
    ;;
    *)
	if test "$have_rimage" = "no" && test "$have_doc" = "no" ; then
		AC_MSG_ERROR([DSP architecture not specified])
	fi
    ;;
esac

ADD_OPTIONAL_AM_CFLAGS([-Wimplicit-fallthrough=3])

AC_SUBST(ARCH_CFLAGS)
AC_SUBST(ARCH_LDFLAGS)

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(AM_CCASFLAGS)

AM_CONDITIONAL(BUILD_XTENSA, test "$ARCH" = "xtensa")
AM_CONDITIONAL(BUILD_HOST, test "$ARCH" = "host")

# check for xtensa extension
AM_CONDITIONAL(BUILD_XTENSA_SMP, test "$with_arch" = "xtensa-smp")

# Cmocka tests
AC_ARG_WITH([cmocka-prefix],
	AS_HELP_STRING([--with-cmocka-prefix], [Path to cmocka]),
	[], [with_cmocka_prefix="no"])

# in case of native build, cmocka may be installed
HAVE_CMOCKA_PKG=no
AC_CHECK_LIB(cmocka, _cmocka_run_group_tests, [HAVE_CMOCKA_PKG=yes])

if test "x$with_arch" != "xno"; then
	if test "x$with_cmocka_prefix" = "xno"; then
		if test "$ARCH" = "xtensa"; then
			AC_MSG_WARN([Need cmocka to run unit tests. Path to cmocka not specified. Please use --with-cmocka-prefix option.])
		elif test "x$HAVE_CMOCKA_PKG" = "xno"; then
			AC_MSG_WARN([Need cmocka to run unit tests. No cmocka library found. Please install cmocka or use --with-cmocka-prefix option.])
		fi
	else
		CMOCKA_PREFIX="$with_cmocka_prefix"
		AC_SUBST(CMOCKA_PREFIX)
	fi
fi

AM_CONDITIONAL(HAVE_CMOCKA_PREFIX, test "x$with_cmocka_prefix" != "xno")

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
   apollolake*)

	PLATFORM_LDSCRIPT="apollolake.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="apollolake"
	AC_SUBST(PLATFORM)

	FW_NAME="apl"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifi3_std"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_APOLLOLAKE], [1], [Configure for Apollolake])
	AC_DEFINE([CONFIG_BOOT_LOADER], [1], [Configure Boot Loader])
	AC_DEFINE([CONFIG_IRQ_MAP], [1], [Configure IRQ maps])
	AC_DEFINE([CONFIG_DMA_GW], [1], [Configure DMA Gateway])
	AC_DEFINE([CONFIG_MEM_WND], [1], [Configure Memory Windows])
    ;;
   kabylake*)

	PLATFORM_LDSCRIPT="apollolake.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="apollolake"
	AC_SUBST(PLATFORM)

	FW_NAME="kbl"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifi3_std"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_APOLLOLAKE], [1], [Configure for Apollolake])
	AC_DEFINE([CONFIG_BOOT_LOADER], [1], [Configure Boot Loader])
	AC_DEFINE([CONFIG_IRQ_MAP], [1], [Configure IRQ maps])
	AC_DEFINE([CONFIG_DMA_GW], [1], [Configure DMA Gateway])
	AC_DEFINE([CONFIG_MEM_WND], [1], [Configure Memory Windows])
    ;;
    skylake*)

	PLATFORM_LDSCRIPT="apollolake.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="apollolake"
	AC_SUBST(PLATFORM)

	FW_NAME="skl"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifi3_std"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_APOLLOLAKE], [1], [Configure for Apollolake])
	AC_DEFINE([CONFIG_BOOT_LOADER], [1], [Configure Boot Loader])
	AC_DEFINE([CONFIG_IRQ_MAP], [1], [Configure IRQ maps])
	AC_DEFINE([CONFIG_DMA_GW], [1], [Configure DMA Gateway])
	AC_DEFINE([CONFIG_MEM_WND], [1], [Configure Memory Windows])
    ;;
    haswell*)

	PLATFORM_LDSCRIPT="haswell.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="haswell"
	AC_SUBST(PLATFORM)

	FW_NAME="hsw"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifiep_bd5"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_HASWELL], [1], [Configure for Haswell])
	AC_DEFINE([CONFIG_HOST_PTABLE], [1], [Configure handling host page table])
    ;;
    broadwell*)

	PLATFORM_LDSCRIPT="broadwell.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="haswell"
	AC_SUBST(PLATFORM)

	FW_NAME="bdw"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifiep_bd5"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_BROADWELL], [1], [Configure for Broadwell])
	AC_DEFINE([CONFIG_HOST_PTABLE], [1], [Configure handling host page table])
    ;;
   cannonlake*)

	PLATFORM_LDSCRIPT="cannonlake.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="cannonlake"
	AC_SUBST(PLATFORM)

	FW_NAME="cnl"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifi4_std"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_CANNONLAKE], [1], [Configure for Cannonlake])
	AC_DEFINE([CONFIG_BOOT_LOADER], [1], [Configure Boot Loader])
	AC_DEFINE([CONFIG_IRQ_MAP], [1], [Configure IRQ maps])
	AC_DEFINE([CONFIG_DMA_GW], [1], [Configure DMA Gateway])
	AC_DEFINE([CONFIG_MEM_WND], [1], [Configure Memory Windows])
    ;;
   suecreek*)

	PLATFORM_LDSCRIPT="suecreek.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="suecreek"
	AC_SUBST(PLATFORM)

	FW_NAME="sue"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifi4_std"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_SUECREEK], [1], [Configure for Suecreek])
	AC_DEFINE([CONFIG_BOOT_LOADER], [1], [Configure Boot Loader])
	AC_DEFINE([CONFIG_IRQ_MAP], [1], [Configure IRQ maps])
    ;;
    icelake*)

	PLATFORM_LDSCRIPT="icelake.x"
	AC_SUBST(PLATFORM_LDSCRIPT)

	PLATFORM="icelake"
	AC_SUBST(PLATFORM)

	FW_NAME="icl"
	AC_SUBST(FW_NAME)

	XTENSA_CORE="hifi4_std"
	AC_SUBST(XTENSA_CORE)

	AC_DEFINE([CONFIG_ICELAKE], [1], [Configure for Icelake])
	AC_DEFINE([CONFIG_BOOT_LOADER], [1], [Configure Boot Loader])
	AC_DEFINE([CONFIG_IRQ_MAP], [1], [Configure IRQ maps])
	AC_DEFINE([CONFIG_DMA_GW], [1], [Configure DMA Gateway])
	AC_DEFINE([CONFIG_MEM_WND], [1], [Configure Memory Windows])
    ;;
    *)
	if test "$have_rimage" = "no" && test "$have_doc" = "no"; then
		if test "$ARCH" = "host"; then
			PLATFORM="host"
			AC_SUBST(PLATFORM)
		else
			AC_MSG_ERROR([Host platform not specified])
		fi
	fi
    ;;
esac

AM_CONDITIONAL(BUILD_BAYTRAIL, test "$FW_NAME" = "byt")
AM_CONDITIONAL(BUILD_CHERRYTRAIL,  test "$FW_NAME" = "cht")
AM_CONDITIONAL(BUILD_HASWELL,  test "$FW_NAME" = "hsw")
AM_CONDITIONAL(BUILD_BROADWELL,  test "$FW_NAME" = "bdw")
AM_CONDITIONAL(BUILD_APOLLOLAKE,  test "$FW_NAME" = "apl" -o "$FW_NAME" = "skl" -o "$FW_NAME" = "kbl")
AM_CONDITIONAL(BUILD_CANNONLAKE,  test "$FW_NAME" = "cnl")
AM_CONDITIONAL(BUILD_SUECREEK,  test "$FW_NAME" = "sue")
AM_CONDITIONAL(BUILD_ICELAKE,  test "$FW_NAME" = "icl")
AM_CONDITIONAL(BUILD_BOOTLOADER,  test "$FW_NAME" = "apl" -o "$FW_NAME" = "cnl" -o "$FW_NAME" = "icl" -o "$FW_NAME" = "sue" -o "$FW_NAME" = "skl" -o "$FW_NAME" = "kbl")
AM_CONDITIONAL(BUILD_CAVS,  test "$FW_NAME" = "apl" -o "$FW_NAME" = "cnl" -o "$FW_NAME" = "icl" -o "$FW_NAME" = "sue" -o "$FW_NAME" = "skl" -o "$FW_NAME" = "kbl")
AM_CONDITIONAL(BUILD_MODULE,  test "$FW_NAME" = "apl" -o "$FW_NAME" = "cnl" -o "$FW_NAME" = "icl" -o "$FW_NAME" = "sue" -o "$FW_NAME" = "skl" -o "$FW_NAME" = "kbl")
AM_CONDITIONAL(BUILD_APL_SSP,  test "$FW_NAME" = "apl" -o "$FW_NAME" = "cnl" -o "$FW_NAME" = "icl" -o "$FW_NAME" = "sue" -o "$FW_NAME" = "skl" -o "$FW_NAME" = "kbl")

# DSP core support (Optional)
AC_ARG_WITH([dsp-core],
        AS_HELP_STRING([--with-dsp-core], [Specify DSP Core]),
	[], [with_dsp_core=no])

case "$with_dsp_core" in
    *)
	XTENSA_CORE="$with_dsp_core"
	AC_SUBST(XTENSA_CORE)
    ;;

esac

PLATFORM_BOOT_LDR_LDSCRIPT="boot_ldr.x"
AC_SUBST(PLATFORM_BOOT_LDR_LDSCRIPT)

# Optimisation settings and checks

# SSE4_2 support
AC_ARG_ENABLE(sse42, [AS_HELP_STRING([--enable-sse42],[enable SSE42 optimizations])], have_sse42=$enableval, have_sse42=yes)
AX_CHECK_COMPILE_FLAG(-msse4.2, [SSE42_CFLAGS="-DOPS_SSE42 -msse4.2 -ffast-math -ftree-vectorizer-verbose=0"],
	[have_sse42=no])
if test "$have_sse42" = "yes"; then
	AC_DEFINE(HAVE_SSE42,1,[Define to enable SSE42 optimizations.])
fi
AM_CONDITIONAL(HAVE_SSE42, test "$have_sse42" = "yes")
AC_SUBST(SSE42_CFLAGS)

# AVX support
AC_ARG_ENABLE(avx, [AS_HELP_STRING([--enable-avx],[enable AVX optimizations])], have_avx=$enableval, have_avx=yes)
AX_CHECK_COMPILE_FLAG(-mavx, [AVX_CFLAGS="-DOPS_AVX -mavx -ffast-math -ftree-vectorizer-verbose=0"],
	[have_avx=no])
if test "$have_avx" = "yes"; then
	AC_DEFINE(HAVE_AVX,1,[Define to enable AVX optimizations.])
fi
AM_CONDITIONAL(HAVE_AVX, test "$have_avx" = "yes")
AC_SUBST(AVX_CFLAGS)


# AVX2 support
AC_ARG_ENABLE(avx2, [AS_HELP_STRING([--enable-avx2],[enable AVX2 optimizations])], have_avx2=$enableval, have_avx2=yes)
AX_CHECK_COMPILE_FLAG(-mavx2, [AVX2_CFLAGS="-DOPS_AVX2 -mavx2 -ffast-math -ftree-vectorizer-verbose=0"],
		[have_avx2=no])
if test "$have_avx2" = "yes"; then
	AC_DEFINE(HAVE_AVX2,1,[Define to enable AVX2 optimizations.])
fi
AM_CONDITIONAL(HAVE_AVX2, test "$have_avx2" = "yes")
AC_SUBST(AVX2_CFLAGS)


# FMA support
AC_ARG_ENABLE(fma, [AS_HELP_STRING([--enable-fma],[enable FMA optimizations])], have_fma=$enableval, have_fma=yes)
AX_CHECK_COMPILE_FLAG(-mfma, [FMA_CFLAGS="-DOPS_FMA -mfma -ffast-math -ftree-vectorizer-verbose=0"],
	[have_fma=no])
if test "$have_fma" = "yes"; then
	AC_DEFINE(HAVE_FMA,1,[Define to enable FMA optimizations.])
fi
AM_CONDITIONAL(HAVE_FMA, test "$have_fma" = "yes")
AC_SUBST(FMA_CFLAGS)

# Hifi2EP
AC_ARG_ENABLE(hifi2ep, [AS_HELP_STRING([--enable-hifi2ep],[enable HiFi2EP optimizations])], have_hifi2ep=$enableval, have_hifi2ep=yes)
AX_CHECK_COMPILE_FLAG(-mhifi2ep, [FMA_CFLAGS="-DOPS_HIFI2EP -mhifi2ep -ffast-math -ftree-vectorizer-verbose=0"],
	[have_hifi2ep=no])
if test "$have_hifi2ep" = "yes"; then
	AC_DEFINE(HAVE_HIFI2EP,1,[Define to enable Hifi2 EP optimizations.])
fi
AM_CONDITIONAL(HAVE_HIFI2EP, test "$have_hifi2ep" = "yes")
AC_SUBST(HIFI2EP_CFLAGS)

# Hifi3
AC_ARG_ENABLE(hifi3, [AS_HELP_STRING([--enable-hifi3],[enable HiFi3 optimizations])], have_hifi3=$enableval, have_hifi3=yes)
AX_CHECK_COMPILE_FLAG(-mhihi3, [FMA_CFLAGS="-DOPS_HIFI3 -mhifi3 -ffast-math -ftree-vectorizer-verbose=0"],
	[have_hifi3=no])
if test "$have_hifi3" = "yes"; then
	AC_DEFINE(HAVE_HIFI3,1,[Define to enable Hifi3 optimizations.])
fi
AM_CONDITIONAL(HAVE_HIFI3, test "$have_hifi3" = "yes")
AC_SUBST(HIFI3_CFLAGS)

# Test after CFLAGS set othewise test of cross compiler fails. 
AM_PROG_AS
AM_PROG_AR
AC_PROG_CC
LT_INIT
AC_CHECK_TOOL([OBJCOPY], [objcopy], [])
AC_CHECK_TOOL([OBJDUMP], [objdump], [])

# Check for openssl - used by rimage
AC_CHECK_LIB([crypto], [OPENSSL_config], , [have_openssl="no"])
if test "$have_rimage" = "yes"; then
	if test "$have_openssl" = "no"; then
		AC_MSG_ERROR([Need OpenSSL libcrypto for rimage code signing])
	fi
fi

if test "x$prefix" == "xNONE"; then
PEM_KEY_PREFIX="/usr/local/share/rimage"
else
PEM_KEY_PREFIX=$prefix"/share/rimage"
fi
AC_DEFINE_UNQUOTED([PEM_KEY_PREFIX], ["$PEM_KEY_PREFIX"], ["Path for PEM keys"])
AC_SUBST(PEM_KEY_PREFIX)

# Check for doxygen and graphviz - used by make doc
AC_CHECK_PROG(have_doxygen, doxygen, true, false)
if test "$have_doxygen" = "false"; then
	AC_MSG_WARN([Need doxygen to build documentation])
fi
AC_CHECK_PROG(have_graphviz, dot, true, false)
if test "$have_graphviz" = "false"; then
	AC_MSG_WARN([Need graphviz to build documentation])
fi

# Check for compiler type
AM_CONDITIONAL(XCC, test "$CC" = "xt-xcc")

AM_EXTRA_RECURSIVE_TARGETS([bin])

AM_EXTRA_RECURSIVE_TARGETS([vminstall])

AC_CONFIG_FILES([
	Makefile
	rimage/Makefile
	rimage/keys/Makefile
	doc/Makefile
	src/Makefile
	src/tasks/Makefile
	src/init/Makefile
	src/arch/Makefile
	src/arch/xtensa/Makefile
	src/arch/xtensa/include/Makefile
	src/arch/xtensa/include/arch/Makefile
	src/arch/xtensa/include/xtensa/Makefile
	src/arch/xtensa/include/xtensa/config/Makefile
	src/arch/xtensa/smp/Makefile
	src/arch/xtensa/smp/hal/Makefile
	src/arch/xtensa/smp/include/Makefile
	src/arch/xtensa/smp/include/arch/Makefile
	src/arch/xtensa/smp/xtos/Makefile
	src/arch/xtensa/up/Makefile
	src/arch/xtensa/up/hal/Makefile
	src/arch/xtensa/up/include/Makefile
	src/arch/xtensa/up/include/arch/Makefile
	src/arch/xtensa/up/xtos/Makefile
	src/arch/host/Makefile
	src/arch/host/include/Makefile
	src/arch/host/include/arch/Makefile
	src/audio/Makefile
	src/math/Makefile
	src/drivers/Makefile
	src/drivers/intel/Makefile
	src/drivers/intel/baytrail/Makefile
	src/drivers/intel/haswell/Makefile
	src/drivers/intel/cavs/Makefile
	src/include/Makefile
	src/include/sof/Makefile
	src/include/sof/audio/Makefile
	src/include/sof/audio/coefficients/Makefile
	src/include/sof/audio/coefficients/src/Makefile
	src/include/sof/drivers/Makefile
	src/include/sof/math/Makefile
	src/include/uapi/Makefile
	src/ipc/Makefile
	src/library/Makefile
	src/library/include/Makefile
	src/library/include/platform/Makefile
	src/lib/Makefile
	src/host/Makefile
	src/platform/Makefile
	src/platform/baytrail/Makefile
	src/platform/baytrail/include/Makefile
	src/platform/baytrail/include/arch/Makefile
	src/platform/baytrail/include/arch/xtensa/Makefile
	src/platform/baytrail/include/arch/xtensa/config/Makefile
	src/platform/baytrail/include/platform/Makefile
	src/platform/apollolake/Makefile
	src/platform/apollolake/include/Makefile
	src/platform/apollolake/include/arch/Makefile
	src/platform/apollolake/include/arch/xtensa/Makefile
	src/platform/apollolake/include/arch/xtensa/config/Makefile
	src/platform/apollolake/include/platform/Makefile
	src/platform/haswell/Makefile
	src/platform/haswell/include/Makefile
	src/platform/haswell/include/arch/Makefile
	src/platform/haswell/include/arch/xtensa/Makefile
	src/platform/haswell/include/arch/xtensa/config/Makefile
	src/platform/haswell/include/platform/Makefile
	src/platform/cannonlake/Makefile
	src/platform/cannonlake/include/Makefile
	src/platform/cannonlake/include/arch/Makefile
	src/platform/cannonlake/include/arch/xtensa/Makefile
	src/platform/cannonlake/include/arch/xtensa/config/Makefile
	src/platform/cannonlake/include/platform/Makefile
	src/platform/suecreek/Makefile
        src/platform/suecreek/include/Makefile
        src/platform/suecreek/include/arch/Makefile
        src/platform/suecreek/include/arch/xtensa/Makefile
        src/platform/suecreek/include/arch/xtensa/config/Makefile
        src/platform/suecreek/include/platform/Makefile
	src/platform/icelake/Makefile
	src/platform/icelake/include/Makefile
	src/platform/icelake/include/arch/Makefile
	src/platform/icelake/include/arch/xtensa/Makefile
	src/platform/icelake/include/arch/xtensa/config/Makefile
	src/platform/icelake/include/platform/Makefile
	src/platform/intel/Makefile
	src/platform/intel/cavs/Makefile
	test/Makefile
	test/cmocka/Makefile
])
AC_REQUIRE_AUX_FILE([tap-driver.sh])
AC_OUTPUT

echo "
---{ $PACKAGE_NAME $VERSION }---

Target Architecture:           ${ARCH}
Target Platform:               ${PLATFORM}
Target Core:                   ${XTENSA_CORE}
Install Prefix:                ${prefix}
PEM:                           ${PEM_KEY_PREFIX}

Compiler:                      ${CC}
OBJCOPY:                       ${OBJCOPY}
OBJDUMP:                       ${OBJDUMP}
CFLAGS:                        ${CFLAGS}
LDFLAGS:                       ${LDFLAGS}
ARCH_CFLAGS:                   ${ARCH_CFLAGS}
ARCH_LDFLAGS:                  ${ARCH_LDFLAGS}
A@&t@M_CFLAGS:                     ${AM_CFLAGS}
A@&t@M_LDFLAGS:                    ${AM_LDFLAGS}
A@&t@M_CCASFLAGS:                  ${AM_CCASFLAGS}
"

