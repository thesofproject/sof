# To customize the installation, copy this file to config.mk and edit
# it. Leave undefined to use default values.

# As usual with Make, all these can also be passed as either CLI
# arguments or environment variables.  Warning: undefined is NOT the
# same as blank!

# Everything is installed by default. To install and deploy fewer
# patforms override the default lists like this:
# UNSIGNED_list :=
# SIGNED_list := apl tgl

# The default FW_DESTDIR is the local /lib/firmware/intel directory
# _remote := up2.local
# FW_DESTDIR     := root@${_remote}:/lib/firmware/intel
# USER_DESTDIR   := ${_remote}:bin/

# SOF_VERSION := $(shell git describe --tags )
# SOF_VERSION := v1.6.14
