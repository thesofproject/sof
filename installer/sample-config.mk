# To customize the installation, copy this file to config.mk and edit
# it. Leave undefined to use default values.

# As usual with Make, all these can also be passed as either CLI
# arguments or environment variables.  Warning: undefined is NOT the
# same as blank!

# You MUST "make cleanall" after changing anything here

# Everything is installed by default. To install and deploy fewer
# patforms override the default lists like this:
# UNSIGNED_list :=
# SIGNED_list := apl tgl

# The default FW_DESTDIR is the local /lib/firmware/intel directory
# _remote := test-system.local
# FW_DESTDIR     := root@${_remote}:/lib/firmware/intel
# USER_DESTDIR   := ${_remote}:bin/

# Passed to ./scripts/xtensa-build-all.sh -i
# Ignored by incremental builds, MUST cleanall after changing this
# IPC_VERSION    := IPC4

# Define this empty for a plain sof/ directory and no sof -> sof-v1.2.3
# symbolic links. This is _only_ to override the top-level directory
# name; for sof_versions.h see version.cmake and try sof/.tarball-version
# SOF_VERSION :=
#
# SOF_VERSION := $(shell git describe --tags )
# SOF_VERSION := v1.6.14

# Uncomment this to have the build_*_?cc/ directories at the top-level
# BUILDS_ROOT := ${CURDIR}/..

# The build is not deterministic; use this to reduce noise when testing
# the installer/ itself
# BUILD_ONLY_ONCE := true
