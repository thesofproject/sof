
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation

# The default target of this installation Makefile copies firmware and
# topology files produced by ./scripts/xtensa-build-all.sh into a tree
# structure (staging area) identical to /lib/firmware/intel/{sof,sof-tplg}/
#
# After inspecting the staging area, you can run "[sudo] make rsync" to
# rsync it into either a local or remote /lib/firmware/
# directory, or to a sof-bin clone or to any other destination of your
# choice configured in the config.mk file.


.DEFAULT_GOAL := stage
.PHONY: clean stage rsync
.PHONY: signed unsigned ldicts topologies
.PHONY: compare signed_dummies

# Local configuration
-include config.mk


       ###    platforms   ###

# To build only a couple, override these in config.mk

UNSIGNED_list ?= bdw byt cht
SIGNED_list  ?= apl cnl icl jsl tgl
# older SOF versions
# SIGNED_list += hsw kbl skl sue
ALIAS_list ?= glk cfl cml ehl

$(info UNSIGNED_list = ${UNSIGNED_list} )
$(info SIGNED_list   = ${SIGNED_list}   )
$(info ALIAS_list    = ${ALIAS_list}    )

target_of_glk := apl
target_of_cfl := cnl
target_of_cml := cnl
target_of_ehl := tgl


# Try to keep SOF_VERSION optional if possible

SOF_VERSION ?= $(shell git describe --dirty)
ifneq (${SOF_VERSION},)
VERSION_DIR := ${SOF_VERSION}/
VERSION_SUFFIX := -${SOF_VERSION}
endif

TOOLCHAIN ?= gcc


      ###       directories       ####

# Our input:  build_*_?cc/ directories
BUILDS_ROOT ?= ../

STAGING_SOF ?= intel/sof

stage: signed unsigned ldicts aliases topologies
	@tree ${STAGING_SOF}

COMMUNITY	:= ${STAGING_SOF}/community
INTEL_SIGNED	:= ${STAGING_SOF}/${VERSION_DIR}intel-signed
PUBLIC_SIGNED	:= ${STAGING_SOF}/${VERSION_DIR}public-signed
${COMMUNITY} ${INTEL_SIGNED} ${PUBLIC_SIGNED}:
	mkdir -p $@


      ###    rsync to local or remote  ####

RSYNC_TO ?= /lib/firmware/intel/
rsync: stage
	rsync -a --info=progress2 ${STAGING_SOF} ${STAGING_SOF}-tplg* "${FW_DESTDIR}"
ifneq (${USER_DESTDIR},)
	# Requires ./scripts/build-tools.sh -l ...
	rsync -a ${BUILDS_ROOT}/tools/build_tools/logger/sof-logger ${USER_DESTDIR}
endif

clean:
	${RM} -r ${STAGING_SOF} ${STAGING_SOF}-tplg*


   ###    sof-*.ri firmware files and symbolic links ####

INSTALL_OPTS := -D -p -m 0664

#
# 1. Stages all *.ri files, examples:
#
# intel/sof
#   └── v1.6.1
#       ├── intel-signed  # left empty
#       ├── public-signed
#       │   ├── sof-apl-v1.6.1.ri
#       │   ├── sof-icl-v1.6.1.ri
#       │   └── sof-tgl-v1.6.1.ri
#       ├── sof-bdw-v1.6.1.ri
#
# 2. Create symbolic links, including (broken) intel-signed symbolic
#    links that must be fixed in a final release, otherwise the release
#    is incomplete. To check all symlinks: file $(find -type l)
#
#  Examples:
#
#  intel/sof/community/
#    │   │   ├── sof-apl.ri -> ../v1.6.1/public-signed/sof-apl-v1.6.1.ri
#    │   │   ├── sof-cfl.ri -> ../v1.6.1/public-signed/sof-cnl-v1.6.1.ri
#
#  intel/sof/
#    │   ├── sof-apl.ri -> v1.6.1/intel-signed/sof-apl-v1.6.1.ri  # broken links
#    │   ├── sof-cfl.ri -> v1.6.1/intel-signed/sof-cnl-v1.6.1.ri
#    │   ├── sof-bdw.ri -> v1.6.1/sof-bdw-v1.6.1.ri
#    │   ├── sof-byt.ri -> v1.6.1/sof-byt-v1.6.1.ri

SIGNED_FWS := ${SIGNED_list:%=${PUBLIC_SIGNED}/sof-%${VERSION_SUFFIX}.ri}
# $(info SIGNED_FWS = ${SIGNED_FWS})
signed: ${SIGNED_FWS}
${SIGNED_FWS}: ${PUBLIC_SIGNED}/sof-%${VERSION_SUFFIX}.ri:    \
               ${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ri     \
	     | ${COMMUNITY} ${INTEL_SIGNED}
	install ${INSTALL_OPTS} $< $@
	ln -sf ../${VERSION_DIR}public-signed/sof-$*${VERSION_SUFFIX}.ri ${STAGING_SOF}/community/sof-$*.ri
	ln -sf     ${VERSION_DIR}intel-signed/sof-$*${VERSION_SUFFIX}.ri ${STAGING_SOF}/sof-$*.ri


UNSIGNED_FWS := ${UNSIGNED_list:%=${STAGING_SOF}/${VERSION_DIR}sof-%${VERSION_SUFFIX}.ri}
# $(info UNSIGNED_FWS = ${UNSIGNED_FWS})
unsigned: ${UNSIGNED_FWS}
${UNSIGNED_FWS}: ${STAGING_SOF}/${VERSION_DIR}sof-%${VERSION_SUFFIX}.ri:   \
                 ${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ri
	install ${INSTALL_OPTS} $< $@
	ln -sf ${VERSION_DIR}sof-$*${VERSION_SUFFIX}.ri ${STAGING_SOF}/sof-$*.ri



       ###  *.ldc logger dictionaries ###

#
# intel/sof
#   │   └── v1.6.1
#   │       ├── sof-apl-v1.6.1.ldc
#   │       ├── sof-bdw-v1.6.1.ldc
#   │       ├── sof-byt-v1.6.1.ldc

LDICTS := ${UNSIGNED_list:%=${STAGING_SOF}/${VERSION_DIR}sof-%${VERSION_SUFFIX}.ldc} \
            ${SIGNED_list:%=${STAGING_SOF}/${VERSION_DIR}sof-%${VERSION_SUFFIX}.ldc}
# $(info LDICTS = ${LDICTS})
ldicts: ${LDICTS}
${LDICTS}: ${STAGING_SOF}/${VERSION_DIR}sof-%${VERSION_SUFFIX}.ldc:  \
             ${BUILDS_ROOT}/build_%_${TOOLCHAIN}/sof.ldc
	install ${INSTALL_OPTS} $< $@



            ###  Platform aliases  ####

#
# Examples:
#
# /intel/sof/
#  ├── sof-cml.ri -> v1.6/intel-signed/sof-cnl-v1.6.ri
#  ├── sof-cnl.ri -> v1.6/intel-signed/sof-cnl-v1.6.ri
#  ├── sof-glk.ri -> v1.6/intel-signed/sof-apl-v1.6.ri
#  └── v1.6
#      ├── sof-cfl-v1.6.ldc -> sof-cnl-v1.6.ldc
#      ├── sof-cml-v1.6.ldc -> sof-cnl-v1.6.ldc
#      ├── sof-glk-v1.6.ldc -> sof-apl-v1.6.ldc
#
ALIASES := ${ALIAS_list:%=${STAGING_SOF}/sof-%.ri}
aliases: ${ALIASES}
${ALIASES}: ${STAGING_SOF}/sof-%.ri:
	ln -sf ${VERSION_DIR}intel-signed/sof-${target_of_$*}${VERSION_SUFFIX}.ri $@
	ln -sf    sof-${target_of_$*}${VERSION_SUFFIX}.ldc \
              ${STAGING_SOF}/${VERSION_DIR}sof-$*${VERSION_SUFFIX}.ldc


               ### sof-tplg/ topologies ###

topologies:
	# This requires ./scripts/build-tools.sh -T ...
	install ${INSTALL_OPTS}  -t ${STAGING_SOF}-tplg${VERSION_SUFFIX}/ \
               ${BUILDS_ROOT}/tools/build_tools/topology/sof-*.tplg
ifneq (${SOF_VERSION},)
	ln -sf sof-tplg${VERSION_SUFFIX} ${STAGING_SOF}-tplg
	test -e ${STAGING_SOF}-tplg
endif
	@file ${STAGING_SOF}-tplg
	@tree ${STAGING_SOF}-tplg* | head -n 10


               ### Testing ###

COMPARE_REFS ?= /lib/firmware/intel

# Useful for testing this Makefile. Can be used against /lib/firmware,
# sof-bin, a previous version of this Makefile,...
compare: stage
	! diff -qr --no-dereference ${COMPARE_REFS}/sof/ ${STAGING_SOF}/ \
             | grep -v ' differ$$' # || true
	! diff -qr --no-dereference ${COMPARE_REFS}/sof-tplg/ ${STAGING_SOF}-tplg/ \
             | grep -v ' differ$$'

# Invoke this manually to check symbolic links are correct
SIGNED_DUMMIES := ${SIGNED_list:%=${INTEL_SIGNED}/sof-%${VERSION_SUFFIX}.ri}
signed_dummies: ${SIGNED_DUMMIES}
	! file $$(find . -type l) | grep -i broken

${SIGNED_DUMMIES}: | ${INTEL_SIGNED}
	touch $@
