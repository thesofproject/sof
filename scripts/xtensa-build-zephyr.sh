#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# stop on most errors
set -e

SOF_TOP=$(cd "$(dirname "$0")" && cd .. && pwd)

SUPPORTED_PLATFORMS=()

# Intel
SUPPORTED_PLATFORMS+=(apl cnl icl tgl-h tgl)

# NXP
SUPPORTED_PLATFORMS+=(imx8 imx8x imx8m)

# -a
DEFAULT_PLATFORMS=("${SUPPORTED_PLATFORMS[@]}")

# How to exclude one platform from the default builds while leaving it
# possible to build individually:
# unset DEFAULT_PLATFORMS[2]

BUILD_JOBS=$(nproc --all)
PLATFORMS=()

die()
{
	>&2 printf '%s ERROR: ' "$0"
	# We want die() to be usable exactly like printf
	# shellcheck disable=SC2059
	>&2 printf "$@"
	>&2 printf '\n'
	exit 1
}

print_usage()
{
    cat <<EOF
Re-configures and re-builds SOF with Zephyr using a pre-installed Zephyr toolchain and
the _defconfig file for that platform.

Outputs in \$(west topdir)/build-*/ directories

usage: $0 [options] [ platform(s) ] [ -- cmake arguments ]

       -a Build all platforms.

       -j n Set number of make build jobs for rimage. Jobs=#cores by default.
           Ignored by "west build".

       -k Path to a non-default rimage signing key.

       -c Using west, downloads inside this sof clone a new Zephyr
          project with the required git repos. Creates a
          sof/zephyrproject/modules/audio/sof symbolic link pointing
          back at this sof clone.
          Incompatible with -p. To stop after downloading Zephyr, do not
          pass any platform or cmake argument.

       -z Initial Zephyr git ref for the -c option. Can be a branch, tag,
          full SHA1 in a fork, magic pull/12345/merge,... anything as long as
          it is fetchable from https://github.com/zephyrproject-rtos/zephyr/

       -p Existing Zephyr project directory. Incompatible with -c.  If
          zephyr-project/modules/audio/sof is missing then a
          symbolic link pointing to ${SOF_TOP} will automatically be
          created and west will recognize it as its new sof module.
          If zephyr-project/modules/audio/sof already exists and is a
          different copy than where this script is run from, then the
          behavior is undefined.
          This -p option is always _required_ if the real (not symbolic)
          sof/ and zephyr-project/ directories are not nested in one
          another.

This script supports XtensaTools but only when installed in a specific
directory structure, example:

myXtensa/
└── install/
    ├── builds/
    │   ├── RD-2012.5-linux/
    │   │   └── Intel_HiFiEP/
    │   └── RG-2017.8-linux/
    │       ├── LX4_langwell_audio_17_8/
    │       └── X4H3I16w2D48w3a_2017_8/
    └── tools/
        ├── RD-2012.5-linux/
        │   └── XtensaTools/
        └── RG-2017.8-linux/
            └── XtensaTools/

$ XTENSA_TOOLS_ROOT=/path/to/myXtensa $0 ...

Supported platforms ${SUPPORTED_PLATFORMS[*]}

EOF
}

zephyr_fetch_and_switch()
{
	( cd "$WEST_TOP"/zephyr
	  set -x
	  git fetch --depth=5 "$1" "$2"
	  git checkout FETCH_HEAD
	)
}

# Downloads zephyrproject inside sof/ and create a ../../.. symbolic
# link back to sof/
west_init_update()
{
	local init_ref="$1"

	# Or, we could have a default value:
	# init_ref=${1:-811a09bd8305}

	git clone --depth=5 https://github.com/zephyrproject-rtos/zephyr \
	    "$WEST_TOP"/zephyr

	# To keep things simple, this moves to a detached HEAD even when
	# init_ref is a (remote) branch.
	test -z "$init_ref" ||
	    zephyr_fetch_and_switch     origin   "${init_ref}"

	# This shows how to point CI at any Zephyr commit from anywhere
	# and to run all tests on it. Simply edit remote and reference,
	# uncomment one of these lines and submit as an SOF Pull
	# Request.  Unlike many git servers, github allows direct
	# fetching of (full 40-digits) SHAs; even SHAs not in origin but
	# in forks!

	# zephyr_fetch_and_switch    origin   pull/38374/head
	# zephyr_fetch_and_switch    origin   19d5448ec117fde8076bec4d0e61da53147f3315

	# SECURITY WARNING for reviewers: never allow unknown code from
	# unknown submitters on any CI system.

	git -C "$WEST_TOP"/zephyr --no-pager log --oneline --graph \
	    --decorate --max-count=20
	west init -l "${WEST_TOP}"/zephyr
	( cd "${WEST_TOP}"
	  mkdir -p modules/audio
	  ln -s ../../.. modules/audio/sof
	  # Do NOT "west update sof"!!
	  west update zephyr hal_xtensa
	)
}

install_opts()
{
    install -D -p "$@"
}

assert_west_topdir()
{
	if west topdir >/dev/null; then return 0; fi

	printf 'SOF_TOP=%s\nWEST_TOP=%s\n' "$SOF_TOP" "$WEST_TOP"
	die 'try the -p or -c option?'
	# Also note west can get confused by symbolic links, see
	# https://github.com/zephyrproject-rtos/west/issues/419
}

build_platforms()
{
	cd "$WEST_TOP"
	assert_west_topdir

	local STAGING=build-sof-staging
	mkdir -p ${STAGING}/sof/ # smex does not use 'install -D'

	for platform in "${PLATFORMS[@]}"; do
		# TODO: extract a new build_platform() function for
		# cleaner error handling and saving a lot of tabs.

		# default value
		local RIMAGE_KEY=modules/audio/sof/keys/otc_private_key.pem

		case "$platform" in
			apl)
				PLAT_CONFIG='intel_adsp_cavs15'
				XTENSA_CORE="X4H3I16w2D48w3a_2017_8"
				XTENSA_TOOLS_VERSION="RG-2017.8-linux"
				;;
			cnl)
				PLAT_CONFIG='intel_adsp_cavs18'
				XTENSA_CORE="X6H3CNL_2017_8"
				XTENSA_TOOLS_VERSION="RG-2017.8-linux"
				;;
			icl)
				PLAT_CONFIG='intel_adsp_cavs20'
				XTENSA_CORE="X6H3CNL_2017_8"
				XTENSA_TOOLS_VERSION="RG-2017.8-linux"
				;;
			tgl-h|tgl)
				PLAT_CONFIG='intel_adsp_cavs25'
				XTENSA_CORE="cavs2x_LX6HiFi3_2017_8"
				XTENSA_TOOLS_VERSION="RG-2017.8-linux"
				RIMAGE_KEY=modules/audio/sof/keys/otc_private_key_3k.pem
				;;
			imx8)
				PLAT_CONFIG='nxp_adsp_imx8'
				RIMAGE_KEY='' # no key needed for imx8
				;;
			imx8x)
				PLAT_CONFIG='nxp_adsp_imx8x'
				RIMAGE_KEY='ignored for imx8x'
				;;
			imx8m)
				PLAT_CONFIG='nxp_adsp_imx8m'
				RIMAGE_KEY='ignored for imx8m'
				;;

			*)
				echo "Unsupported platform: $platform"
				exit 1
				;;
		esac

		if [ -n "$XTENSA_TOOLS_ROOT" ]
		then
			# set variables expected by zephyr/cmake/toolchain/xcc/generic.cmake
			export ZEPHYR_TOOLCHAIN_VARIANT=xcc
			export XTENSA_TOOLCHAIN_PATH="$XTENSA_TOOLS_ROOT/install/tools"
			export TOOLCHAIN_VER="$XTENSA_TOOLS_VERSION"
			printf 'XTENSA_TOOLCHAIN_PATH=%s\n' "${XTENSA_TOOLCHAIN_PATH}"
			printf 'TOOLCHAIN_VER=%s\n' "${TOOLCHAIN_VER}"

			# set variables expected by xcc toolchain
			XTENSA_BUILDS_DIR="$XTENSA_TOOLS_ROOT/install/builds/$XTENSA_TOOLS_VERSION"
			export XTENSA_SYSTEM=$XTENSA_BUILDS_DIR/$XTENSA_CORE/config
			printf 'XTENSA_SYSTEM=%s\n' "${XTENSA_SYSTEM}"
		fi

		local bdir=build-"$platform"

		(
			 # west can get lost in symbolic links and then
			 # show a confusing error.
			/bin/pwd

			if test -e  "$bdir/build.ninja" || test -e  "$bdir/Makefile"; then
			    test -z "${CMAKE_ARGS+defined}" ||
				die 'Cannot re-define CMAKE_ARGS, you must delete %s first\n' \
				    "$(pwd)/$bdir"
			    # --board is cached and not required again either but unlike
			    # CMAKE_ARGS this _not_ does force CMake to re-run and passing
			    # a different board by mistake is very nicely caught by west.
			    set -x
			    west build --build-dir "$bdir" --board "$PLAT_CONFIG"
			else
			    set -x
			    west build --build-dir "$bdir" --board "$PLAT_CONFIG" \
				zephyr/samples/subsys/audio/sof \
				-- "${CMAKE_ARGS[@]}"
			fi

			# This should ideally be part of
			# sof/zephyr/CMakeLists.txt but due to the way
			# Zephyr works, the SOF library build there does
			# not seem like it can go further than a
			# "libmodules_sof.a" file that smex does not
			# know how to use.
			"$bdir"/zephyr/smex_ep/build/smex \
			       -l "$STAGING"/sof/sof-"$platform".ldc \
			       "$bdir"/zephyr/zephyr.elf

			download_missing_submodules

			# Build rimage
			RIMAGE_DIR=build-rimage
			cmake -B "$RIMAGE_DIR" -S modules/audio/sof/rimage
			make -C "$RIMAGE_DIR" -j"$BUILD_JOBS"

			test -z "${RIMAGE_KEY_OPT}" || RIMAGE_KEY="${RIMAGE_KEY_OPT}"

			west sign  --build-dir "$bdir" \
				--tool rimage --tool-path "$RIMAGE_DIR"/rimage \
				--tool-data modules/audio/sof/rimage/config -- -k "$RIMAGE_KEY"
		)

		# A bit of a hack but it's very simple and saves a lot of duplication
		grep -q "UNSIGNED_RI.*${platform}" "${SOF_TOP}"/src/arch/xtensa/CMakeLists.txt ||
		    # This could use a -q(uiet) option...
		    "${SOF_TOP}"/tools/sof_ri_info/sof_ri_info.py \
			--no_headers --no_modules --no_memory \
			--erase_vars "$bdir"/zephyr/reproducible.ri "$bdir"/zephyr/zephyr.ri

		install_opts -m 0644 "$bdir"/zephyr/zephyr.ri \
			     "$STAGING"/sof/community/sof-"$platform".ri
	done

	install_opts -m 0755 "$bdir"/zephyr/sof-logger_ep/build/logger/sof-logger \
		     "$STAGING"/tools/sof-logger

	(cd "$STAGING"; pwd)
	tree "$STAGING" || ( cd "$STAGING" && ls -R1 )
}

parse_args()
{
	local zeproj
	unset zephyr_ref
	local OPTIND=1

	# Parse -options
	while getopts "acz:j:k:p:" OPTION; do
		case "$OPTION" in
			a) PLATFORMS=("${DEFAULT_PLATFORMS[@]}") ;;
			c) DO_CLONE=yes ;;
			z) zephyr_ref="$OPTARG" ;;
			j) BUILD_JOBS="$OPTARG" ;;
			k) RIMAGE_KEY_OPT="$OPTARG" ;;
			p) zeproj="$OPTARG" ;;
			*) print_usage; exit 1 ;;
		esac
	done
	# This also drops any _leading_ '--'. Note this parser is
	# compatible with using '--' twice (undocumented feature), as
	# in:
	#   build-zephyr.sh -c -k somekey -- apl imx -- -DEXTRA_CFLAGS='-Werror'
	shift $((OPTIND-1))

	if [ -n "$zeproj" ] && [ "$DO_CLONE" = yes ]; then
	    die 'Cannot use -p with -c, -c supports %s only' "${SOF_TOP}/zephyrproject"
	fi

	if [ -n "$zephyr_ref" ] && [ -z "$DO_CLONE" ]; then
	   die '%s' '-z without -c makes no sense'
	fi

	if [ -n "$zeproj" ]; then

	    [ -d "$zeproj" ] ||
		die "$zeproj is not a directory, try -c instead of -p?"

	    ( cd "$zeproj"
	      test "$(realpath "$(west topdir)")" = "$(/bin/pwd)"
	    ) || die '%s is not the top of a zephyr project' "$zeproj"

	    WEST_TOP="$zeproj"
	fi

	# Find platform arguments if '-a' was not used
	test "${#PLATFORMS[@]}" -ne 0 || while test -n "$1"; do
		local arg="$1"

		# '--' ends platforms
		if [ "$arg" = '--' ]; then
		    shift || true; break
		fi

		platform=none
		for i in "${SUPPORTED_PLATFORMS[@]}"; do
			if [ "$i" = "$arg" ]; then
				PLATFORMS=("${PLATFORMS[@]}" "$i")
				platform="$i"
				shift || true
				break
			fi
		done
		if [ "$platform" == "none" ]; then
			echo "Error: Unknown platform specified: $arg"
			echo "Supported platforms are: ${SUPPORTED_PLATFORMS[*]}"
			exit 1
		fi
	done

	# Check some target platform(s) have been passed in one way or
	# the other
	if [ "${#PLATFORMS[@]}" -eq 0 ]; then
		printf 'No platform build requested\n'
	else
		printf 'Building platforms:'
		printf ' %s' "${PLATFORMS[@]}"; printf '\n'
	fi

	CMAKE_ARGS=("$@")
	# For debugging quoting and whitespace
	#declare -p CMAKE_ARGS
}

main()
{
	parse_args "$@"

	type -p west ||
	    die "Install west and a west toolchain, \
see https://docs.zephyrproject.org/latest/getting_started/index.html"

	if [ "$DO_CLONE" == "yes" ]; then
		 # Supposedly no Zephyr yet
		test -z "$WEST_TOP" ||
		    die 'Cannot use -p with -c, -c supports %s only' "${SOF_TOP}/zephyrproject"

		if west topdir 2>/dev/null ||
			( cd "$SOF_TOP" && west topdir 2>/dev/null ); then
		    die 'Zephyr found already! Not downloading it again\n'
		fi

		local zep="${SOF_TOP}"/zephyrproject
		test -e "$zep" && die "failed -c: $zep already exists"

		# Resolve symlinks
		mkdir "$zep"; WEST_TOP=$( cd "$zep" && /bin/pwd )

		west_init_update "${zephyr_ref}"

	else
		 # Look for Zephyr and define WEST_TOP

		if test -z "${WEST_TOP}"; then
		    # no '-p' user input, so try a couple other things
		    if test -e "${SOF_TOP}"/zephyrproject; then
			WEST_TOP=$( cd "${SOF_TOP}"/zephyrproject/ && /bin/pwd )
		    else # most simple: nesting
			WEST_TOP=$(cd "${SOF_TOP}" && west topdir) || {
			    # This was the last chance, abort:
			    cd "${SOF_TOP}"; assert_west_topdir
			}
		    fi
		fi

		( cd "${WEST_TOP}" && assert_west_topdir )
	fi


	# Symlink zephyr-project to our SOF selves if no sof west module yet
	test -e "${WEST_TOP}"/modules/audio/sof || {
		mkdir -p "${WEST_TOP}"/modules/audio
		ln -s "$SOF_TOP" "${WEST_TOP}"/modules/audio/sof
	}

	test "${#PLATFORMS[@]}" -eq 0 || build_platforms
}


download_missing_submodules()
{
	# FIXME: remove this hack. Downloading and building should be
	# kept separate but support for submodules in west is too
	# recent, cannot rely on it yet.
	# https://docs.zephyrproject.org/latest/guides/west/release-notes.html#v0-9-0
	test -e "${WEST_TOP}"/modules/audio/sof/rimage/CMakeLists.txt || (

		cd "${WEST_TOP}"/modules/audio/sof

		    # Support starting with sof/ coming from "west
		    # update sof".
		    # "origin" is the default value expected by git
		    # submodule.  Don't overwrite any existing "origin";
		    # it could be some user-defined mirror.
		    git remote | grep -q '^origin$' ||
			git remote add origin https://github.com/thesofproject/sof

		git submodule update --init --recursive
	)
}


main "$@"
