
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation


.DEFAULT_GOAL := stage
.PHONY: clean stage rsync tarball
.PHONY: signed unsigned ldicts topologies tools build_tools
.PHONY: compare signed_dummies

# Override ?= variables in config.mk
-include config.mk

# List of /lib/firmware/sof/ images and symbolic links currently
# released by Intel.

# See rimage/config/*.toml
SIGNED_list  ?= tgl tgl-h

# To find aliases, try in a Linux kernel git clone:
#
#   git grep 'sof-.*\.ri' -- sound/soc/

ALIAS_OTHER_KEY_list += adl adl-s rpl rpl-s

# Not supported in the main branch anymore, go to stable-v2.3
# UNSIGNED_list += bdw byt cht
# SIGNED_list   += apl cnl icl jsl
# ALIAS_SAME_KEY_list  += glk cfl cml
# ALIAS_OTHER_KEY_list += ehl

# Much older platforms
# UNSIGNED_list += hsw sue
# SIGNED_list += kbl skl

ALIAS_list := ${ALIAS_SAME_KEY_list} ${ALIAS_OTHER_KEY_list}

$(info UNSIGNED_list = ${UNSIGNED_list} )
$(info SIGNED_list   = ${SIGNED_list}   )
$(info ALIAS_list    = ${ALIAS_list}    )

target_of_glk := apl
target_of_cfl := cnl
target_of_cml := cnl

target_of_ehl := tgl
target_of_adl   := tgl
target_of_adl-s := tgl-h
target_of_rpl   := tgl
target_of_rpl-s := tgl-h

ifeq (,${TOOLCHAIN})
  ifeq (,${XTENSA_TOOLS_ROOT})
    TOOLCHAIN := gcc
  else
    TOOLCHAIN := xcc
  endif
endif

TREE_OPTS ?= --sort=size --dirsfirst
INSTALL_OPTS ?= -D -p -m 0664

# Keep SOF_VERSION optional

SOF_VERSION ?= $(shell git describe --dirty)
ifneq (${SOF_VERSION},)
VERSION_DIR := ${SOF_VERSION}/
VERSION_SUFFIX := -${SOF_VERSION}
endif



      ################################
      ###  Top-level directories  ####
      ################################

# Our input:  build_*_?cc/ directories
BUILDS_ROOT ?= ${CURDIR}/../installer-builds
BUILD_TOOLS ?= ${BUILDS_ROOT}/build_tools

STAGING_SOF ?= staging/sof
STAGING_SOF_VERSION := ${STAGING_SOF}${VERSION_SUFFIX}

STAGING_SOF_TPLG ?= staging/sof-tplg

STAGING_TOOLS ?= staging/tools

stage: signed unsigned ldicts aliases topologies tools
ifneq (${STAGING_SOF_VERSION},${STAGING_SOF})
	ln -sfT sof${VERSION_SUFFIX} ${STAGING_SOF}
	test -e ${STAGING_SOF}
endif
	@file ${STAGING_SOF}
	@tree ${TREE_OPTS} ${STAGING_SOF_VERSION}

COMMUNITY	:= ${STAGING_SOF_VERSION}/community
INTEL_SIGNED	:= ${STAGING_SOF_VERSION}/intel-signed
${COMMUNITY} ${INTEL_SIGNED} ${BUILDS_ROOT} ${STAGING_SOF_VERSION}:
	mkdir -p $@

# The noise for incremental, do-nothing builds is incredible otherwise,
# especially for build_tools
GNUMAKEFLAGS = --no-print-directory

      #####################################
      ###    rsync to local or remote  ####
      #####################################

# Default value
FW_DESTDIR ?= /lib/firmware/intel/

# The rsync target does not depend on any other target so:
# - it's possible to deploy a staging _subset_, e.g.: only topologies
#   only,...
# - "sudo make rsync" never builds as root by accident
rsync:
# The --mkpath option is too recent, dealing with both remote and local
# would be complicated and this is also a safety against typos.
	# The destination directory must already exist
	rsync -a --info=progress2 staging/sof* "${FW_DESTDIR}"
ifneq (${USER_DESTDIR},)
	# TODO: add more user space binaries: sof-ctl, probes,...
	# absorbe scripts/sof-target-install.sh
	cd ${BUILD_TOOLS} && rsync -a ${TOOLS_RELPATHS} ${USER_DESTDIR}
endif

clean:
	${RM} -r ${STAGING_SOF}* ${STAGING_SOF_TPLG}* ${STAGING_TOOLS}*
	${RM} ${BUILDS_ROOT}/staging_sof_tree.txt

cleanall: clean
	${RM} -r "${BUILD_TOOLS}/" "${BUILDS_ROOT}"/build_*_?cc/


      #####################################
      ###           tarball            ####
      #####################################

tarball: stage
	cd staging && tar cfz sof-build${VERSION_SUFFIX}.tgz \
	  sof${VERSION_SUFFIX} sof-tplg${VERSION_SUFFIX} \
	  tools${VERSION_SUFFIX}


      #####################################
      ###           Stage tools        ####
      #####################################

# This is only for the tarball, rsync takes the RELPATHS shortcut
tools: build_tools
	mkdir -p ${STAGING_TOOLS}${VERSION_SUFFIX}
	cd ${BUILD_TOOLS} && \
	  cp -p ${TOOLS_RELPATHS} ${CURDIR}/${STAGING_TOOLS}${VERSION_SUFFIX}

   ##########################################################
   ###  Stage sof-*.ri firmware files and symbolic links ####
   ##########################################################

#
# 1. Stages all *.ri files
#
# 2. Create symbolic links, including (broken) intel-signed symbolic
#    links that must be fixed in a final release, otherwise the release
#    is incomplete. To check all symlinks: file $(find -type l)
#

# '%' is the platform name
SIGNED_FWS := ${SIGNED_list:%=${COMMUNITY}/sof-%.ri}
# $(info SIGNED_FWS = ${SIGNED_FWS})
signed: ${SIGNED_FWS}
${SIGNED_FWS}: ${COMMUNITY}/sof-%.ri:    \
               ${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ri     \
	     | ${COMMUNITY} ${INTEL_SIGNED}
	install ${INSTALL_OPTS} $< $@
	ln -sfT     intel-signed/sof-$*.ri ${STAGING_SOF_VERSION}/sof-$*.ri

# '%' is the platform name
UNSIGNED_FWS := ${UNSIGNED_list:%=${STAGING_SOF_VERSION}/sof-%.ri}
# $(info UNSIGNED_FWS = ${UNSIGNED_FWS})
unsigned: ${UNSIGNED_FWS}
${UNSIGNED_FWS}: ${STAGING_SOF_VERSION}/sof-%.ri:   \
                 ${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ri
	install ${INSTALL_OPTS} $< $@


BUILD_SOF_RIS := \
      ${UNSIGNED_list:%=${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ri} \
        ${SIGNED_list:%=${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ri}

# When the build is not deterministic use this to reduce noise when testing
# this Makefile.
# Also very useful with XCC, see next comment.
ifneq (true,${BUILD_ONLY_ONCE})
.PHONY: ${BUILD_SOF_RIS}
endif

# Incremental builds are not possible with XCC because the entire
# toolchain requires $XTENSA_SYSTEM but CMake does not support
# build-time environment variables. In the future we could move the
# XTENSA_SYSTEM values out of xtensa-build-all.sh and into some shared
# config file included here.
${BUILD_SOF_RIS}: ${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ri: | ${BUILDS_ROOT}
	cd ${BUILDS_ROOT} && bdir="$$(dirname $@)" &&  \
      if [ -d $${bdir} ] && [ xcc != "${TOOLCHAIN}" ] ; then \
        cmake --build $${bdir} -- bin; else \
      $(CURDIR)/../scripts/xtensa-build-all.sh -i "${IPC_VERSION}" $*; fi


       ########################################
       ###  Stage *.ldc logger dictionaries ###
       ########################################

# '%' is the platform name
LDICTS := ${UNSIGNED_list:%=${STAGING_SOF_VERSION}/sof-%.ldc} \
            ${SIGNED_list:%=${STAGING_SOF_VERSION}/sof-%.ldc}
# $(info LDICTS = ${LDICTS})
ldicts: ${LDICTS}
${LDICTS}: ${STAGING_SOF_VERSION}/sof-%.ldc:  \
                                ${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ri
	if test -e ${BUILDS_ROOT}/build_$*_${TOOLCHAIN}/sof.ldc; then \
	install ${INSTALL_OPTS} ${BUILDS_ROOT}/build_$*_${TOOLCHAIN}/sof.ldc $@ ; \
	else printf '# CONFIG_TRACE was disabled\n' > $@; fi


        #######################################
        ###  Platform -> platform aliases  ####
        #######################################

# '%' is the platform name
COMM_ALIASES   := ${ALIAS_list:%=${STAGING_SOF_VERSION}/community/sof-%.ri}
DICT_ALIASES   := ${ALIAS_list:%=${STAGING_SOF_VERSION}/sof-%.ldc}

${COMM_ALIASES}: ${STAGING_SOF_VERSION}/community/sof-%.ri: | ${COMMUNITY}
	ln -sfT sof-${target_of_$*}.ri $@

${DICT_ALIASES}: ${STAGING_SOF_VERSION}/sof-%.ldc: | ${STAGING_SOF_VERSION}
	ln -sfT sof-${target_of_$*}.ldc $@

# ->intel-signed/ symlinks for platform aliases. The other, identical
# looking ->intel-signed/ symlinks for the other, "normal" platforms are
# created here but by the signed: target.
#
# Some have the same key, others just the code. We don't make any
# difference, no platform re-direction here, this is merely pointing at
# ->intel-signed/same.ri
SIGNED_ALIASES := ${ALIAS_list:%=${STAGING_SOF_VERSION}/sof-%.ri}
${SIGNED_ALIASES}: ${STAGING_SOF_VERSION}/sof-%.ri: | ${STAGING_SOF_VERSION}
	ln -sfT intel-signed/sof-$*.ri $@

# Platform indirection inside the intel-signed/ directory
SIGNED_PLATFORM_ALIASES := ${ALIAS_SAME_KEY_list:%=${INTEL_SIGNED}/sof-%.ri}
${SIGNED_PLATFORM_ALIASES}: ${INTEL_SIGNED}/sof-%.ri: | ${INTEL_SIGNED}
	ln -sfT sof-${target_of_$*}.ri $@


aliases: ${COMM_ALIASES} ${DICT_ALIASES} ${SIGNED_ALIASES} \
         ${SIGNED_PLATFORM_ALIASES}

            ##################################
            ### Stage sof-tplg/ topologies ###
            ##################################

# The build is not deterministic; use this to reduce noise when testing
# this Makefile
ifneq (true,${BUILD_ONLY_ONCE})
.PHONY: ${BUILD_TOOLS}
endif

# Deploy and release only "official", top-level topology files starting
# with the 'sof-' prefix to avoid any experimental stuff.
topologies: ${BUILD_TOOLS}
	install ${INSTALL_OPTS}  -t ${STAGING_SOF_TPLG}${VERSION_SUFFIX}/ \
               ${BUILD_TOOLS}/topology/sof-*.tplg
ifneq (,${VERSION_SUFFIX})
	ln -sfT sof-tplg${VERSION_SUFFIX} ${STAGING_SOF_TPLG}
	test -e ${STAGING_SOF_TPLG}
endif
	@file ${STAGING_SOF_TPLG}
	@tree ${TREE_OPTS} ${STAGING_SOF_TPLG}${VERSION_SUFFIX} | \
            head -n 10; printf '├── ...\n..\n'


             ######################
             ### build-tools.sh ###
             ######################

TOOLS_RELPATHS  := ctl/sof-ctl  logger/sof-logger  probes/sof-probes
TOOLS_TARGETS   := sof-ctl      sof-logger         sof-probes
TOOLS_FLAGS     := -c           -l                 -p

.PHONY: build_tools
build_tools: ${BUILD_TOOLS}

# We could use more targets rather than "set -e" with a multi-lines "for" loop.
# That would be more flexible but also quite verbose.
${BUILD_TOOLS}:
	set -e; if test -d ${BUILD_TOOLS}; then \
        for i in topologies ${TOOLS_TARGETS}; do \
          cmake --build ${BUILD_TOOLS} -- $$i; done; else         \
          BUILD_TOOLS_DIR=${BUILD_TOOLS} ../scripts/build-tools.sh -T ${TOOLS_FLAGS} ; \
        fi


             ####################
             ### Self-Testing ###
             ####################

COMPARE_REFS ?= /lib/firmware/intel

checktree:
	cd ${STAGING_SOF_VERSION} && \
             tree -a -v --dirsfirst . >  ${BUILDS_ROOT}/staging_sof_tree.txt
	# Update sof-apl-nocodec.tplg when adding or removing a default platform
	diff -u tests/staging_sof${IPC_VERSION}_ref.txt ${BUILDS_ROOT}/staging_sof_tree.txt
	# Check two random topologies are there
	test -f ${STAGING_SOF_TPLG}/sof-apl-nocodec.tplg
	test -f ${STAGING_SOF_TPLG}/sof-imx8-wm8960.tplg
	for t in sof-ctl sof-logger sof-probes; do \
	   test -f ${STAGING_TOOLS}${VERSION_SUFFIX}/$${t}; done

# Useful for testing this Makefile. COMPARE_REFS can be /lib/firmware,
# sof-bin, a previous version of this Makefile,...
# As the first arguments maybe symbolic links, their trailing slash is
# critical.
compare: stage
	! diff -qr --no-dereference ${COMPARE_REFS}/sof/ \
             ${STAGING_SOF_VERSION}/ \
             | grep -v ' differ$$' # || true
	! diff -qr --no-dereference ${COMPARE_REFS}/sof-tplg/ \
             ${STAGING_SOF_TPLG}${VERSION_SUFFIX}/ \
             | grep -v ' differ$$'

# Invoke this manually to check symbolic links are correct
SIGNED_DUMMIES := ${SIGNED_list:%=${INTEL_SIGNED}/sof-%.ri} \
                   ${ALIAS_OTHER_KEY_list:%=${INTEL_SIGNED}/sof-%.ri}
signed_dummies: ${SIGNED_DUMMIES}
	! file $$(find staging -type l) | grep -i broken

${SIGNED_DUMMIES}: | ${INTEL_SIGNED}
	touch $@
