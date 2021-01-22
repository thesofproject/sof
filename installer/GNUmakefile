
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation


.DEFAULT_GOAL := stage
.PHONY: clean stage rsync
.PHONY: signed unsigned ldicts topologies
.PHONY: compare signed_dummies

# Override ?= variables in config.mk
-include config.mk

UNSIGNED_list ?= bdw byt cht
SIGNED_list  ?= apl cnl icl jsl tgl
# older SOF versions
# SIGNED_list += hsw kbl skl sue
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

TOOLCHAIN ?= gcc

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
BUILDS_ROOT ?= ../

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
${COMMUNITY} ${INTEL_SIGNED}:
	mkdir -p $@


      #####################################
      ###    rsync to local or remote  ####
      #####################################

# Default value
FW_DESTDIR ?= /lib/firmware/intel/
# We don't depend on any other target so:
# - it's possible to deploy a staging _subset_,
# - sudo never builds by accident
rsync:
	rsync -a --info=progress2 staging/sof* "${FW_DESTDIR}"
ifneq (${USER_DESTDIR},)
	# TODO: add more user space binaries: sof-ctl, probes,...
	# absorbe scripts/sof-target-install.sh
	# Requires ./scripts/build-tools.sh -l ...
	rsync -a ${BUILDS_ROOT}/tools/build_tools/logger/sof-logger ${USER_DESTDIR}
endif

clean:
	${RM} -r staging/sof*


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


       ########################################
       ###  Stage *.ldc logger dictionaries ###
       ########################################

# '%' is the platform name
LDICTS := ${UNSIGNED_list:%=${STAGING_SOF_VERSION}/sof-%.ldc} \
            ${SIGNED_list:%=${STAGING_SOF_VERSION}/sof-%.ldc}
# $(info LDICTS = ${LDICTS})
ldicts: ${LDICTS}
${LDICTS}: ${STAGING_SOF_VERSION}/sof-%.ldc:  \
             ${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ldc
	install ${INSTALL_OPTS} $< $@


        #######################################
        ###  Platform -> platform aliases  ####
        #######################################

# '%' is the platform name
COMM_ALIASES   := ${ALIAS_list:%=${STAGING_SOF_VERSION}/community/sof-%.ri}
DICT_ALIASES   := ${ALIAS_list:%=${STAGING_SOF_VERSION}/sof-%.ldc}
SIGNED_ALIASES := ${ALIAS_list:%=${STAGING_SOF_VERSION}/sof-%.ri}
aliases: ${SIGNED_ALIASES} ${COMM_ALIASES} ${DICT_ALIASES}

${COMM_ALIASES}: ${STAGING_SOF_VERSION}/community/sof-%.ri:
	ln -sfT sof-${target_of_$*}.ri $@

${DICT_ALIASES}: ${STAGING_SOF_VERSION}/sof-%.ldc:
	ln -sfT sof-${target_of_$*}.ldc $@

# Some have the same key, others just the code. We don't make any
# difference here and let the intel-signed/ directory handle this.
${SIGNED_ALIASES}: ${STAGING_SOF_VERSION}/sof-%.ri:
	ln -sfT intel-signed/sof-$*.ri $@


            ##################################
            ### Stage sof-tplg/ topologies ###
            ##################################

topologies:
	# This requires ./scripts/build-tools.sh -T ...
	install ${INSTALL_OPTS}  -t ${STAGING_SOF_TPLG}${VERSION_SUFFIX}/ \
               ${BUILDS_ROOT}/tools/build_tools/topology/sof-*.tplg
ifneq (,${VERSION_SUFFIX})
	ln -sfT sof-tplg${VERSION_SUFFIX} ${STAGING_SOF_TPLG}
	test -e ${STAGING_SOF_TPLG}
endif
	@file ${STAGING_SOF_TPLG}
	@tree ${TREE_OPTS} ${STAGING_SOF_TPLG}${VERSION_SUFFIX} | \
            head -n 10; printf '├── ...\n..\n'


             ####################
             ### Self-Testing ###
             ####################

COMPARE_REFS ?= /lib/firmware/intel

# Useful for testing this Makefile. Can be used against /lib/firmware,
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
