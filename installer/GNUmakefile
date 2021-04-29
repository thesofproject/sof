
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation


.DEFAULT_GOAL := stage
.PHONY: clean stage rsync
.PHONY: signed unsigned ldicts topologies
.PHONY: compare signed_dummies

# Override ?= variables in config.mk
-include config.mk

UNSIGNED_list ?= bdw byt cht
SIGNED_list  ?= apl cnl icl jsl tgl tgl-h
# older SOF versions
# SIGNED_list += hsw kbl skl sue

# In a Linux kernel git clone, try:
#   git grep 'sof-.*\.ri' -- sound/soc/
ALIAS_list ?= glk cfl cml ehl

$(info UNSIGNED_list = ${UNSIGNED_list} )
$(info SIGNED_list   = ${SIGNED_list}   )
$(info ALIAS_list    = ${ALIAS_list}    )

# Same code, same Intel key
target_of_glk := apl
target_of_cfl := cnl
target_of_cml := cnl

# Same code, different Intel key
target_of_ehl := tgl

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

stage: signed unsigned ldicts aliases topologies
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
# - sudo never builds by accident
rsync:
# The --mkpath option is too recent, dealing with both remote and local
# would be complicated and this is also a safety against typos.
	# The destination directory must already exist
	rsync -a --info=progress2 staging/sof* "${FW_DESTDIR}"
ifneq (${USER_DESTDIR},)
	# TODO: add more user space binaries: sof-ctl, probes,...
	# absorbe scripts/sof-target-install.sh
	rsync -a ${BUILD_TOOLS}/logger/sof-logger ${USER_DESTDIR}
endif

clean:
	${RM} -r staging/sof*
	${RM} ${BUILDS_ROOT}/staging_sof_tree.txt

cleanall: clean
	${RM} -r "${BUILD_TOOLS}/" "${BUILDS_ROOT}"/build_*_?cc/

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

# The build is not deterministic; use this to reduce noise when testing
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
      $(CURDIR)/../scripts/xtensa-build-all.sh $*; fi


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
	install ${INSTALL_OPTS} ${BUILDS_ROOT}/build_$*_${TOOLCHAIN}/sof.ldc $@


        #######################################
        ###  Platform -> platform aliases  ####
        #######################################

# '%' is the platform name
COMM_ALIASES   := ${ALIAS_list:%=${STAGING_SOF_VERSION}/community/sof-%.ri}
DICT_ALIASES   := ${ALIAS_list:%=${STAGING_SOF_VERSION}/sof-%.ldc}
SIGNED_ALIASES := ${ALIAS_list:%=${STAGING_SOF_VERSION}/sof-%.ri}
aliases: ${SIGNED_ALIASES} ${COMM_ALIASES} ${DICT_ALIASES}

${COMM_ALIASES}: ${STAGING_SOF_VERSION}/community/sof-%.ri: | ${COMMUNITY}
	ln -sfT sof-${target_of_$*}.ri $@

${DICT_ALIASES}: ${STAGING_SOF_VERSION}/sof-%.ldc: | ${STAGING_SOF_VERSION}
	ln -sfT sof-${target_of_$*}.ldc $@

# Some have the same key, others just the code. We don't make any
# difference here and let the intel-signed/ directory handle this.
${SIGNED_ALIASES}: ${STAGING_SOF_VERSION}/sof-%.ri: | ${STAGING_SOF_VERSION}
	ln -sfT intel-signed/sof-$*.ri $@


            ##################################
            ### Stage sof-tplg/ topologies ###
            ##################################

# The build is not deterministic; use this to reduce noise when testing
# this Makefile
ifneq (true,${BUILD_ONLY_ONCE})
.PHONY: ${BUILD_TOOLS}
endif

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

# We should use more targets rather than set -e and a multi-lines script
# but that would be verbose.
${BUILD_TOOLS}:
	set -e; if test -d ${BUILD_TOOLS}; then \
        for i in topologies sof-logger; do \
          cmake --build ${BUILD_TOOLS} -- $$i; done; else         \
          BUILD_TOOLS_DIR=${BUILD_TOOLS} ../scripts/build-tools.sh -T -l ; \
        fi


             ####################
             ### Self-Testing ###
             ####################

COMPARE_REFS ?= /lib/firmware/intel

checktree:
	cd ${STAGING_SOF_VERSION} && \
             tree -a --dirsfirst . >  ${BUILDS_ROOT}/staging_sof_tree.txt
	# Update sof-apl-nocodec.tplg when adding or removing a default platform
	diff -u tests/staging_sof_ref.txt ${BUILDS_ROOT}/staging_sof_tree.txt
	# Check two random topologies are there
	test -f ${STAGING_SOF_TPLG}/sof-apl-nocodec.tplg
	test -f ${STAGING_SOF_TPLG}/sof-imx8-wm8960.tplg

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
                   ${ALIAS_list:%=${INTEL_SIGNED}/sof-%.ri}
signed_dummies: ${SIGNED_DUMMIES}
	! file $$(find . -type l) | grep -i broken

${SIGNED_DUMMIES}: | ${INTEL_SIGNED}
	touch $@
