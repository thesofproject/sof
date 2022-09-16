#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2021 Intel Corporation. All rights reserved.

# "All problems can be solved by another level of indirection"
# Ideally, this script would not be needed.
#
# Minor adjustments to the docker image provided by the Zephyr project.

set -e
set -x

unset ZEPHYR_BASE

# Make sure we're in the right place
test -e ./sof/scripts/xtensa-build-zephyr.py

# See .github/workflows/zephyr.yml
PATH="$PATH":/opt/sparse/bin
command -v sparse  || true
: REAL_CC="$REAL_CC"

# See https://stackoverflow.com/questions/35291520/docker-and-userns-remap-how-to-manage-volume-permissions-to-share-data-betwee + many others
exec_as_sof_uid()
{
    local sof_uid; sof_uid="$(stat --printf='%u' .)"
    local current_uid; current_uid="$(id -u)"
    if test "$current_uid" = "$sof_uid"; then
        return 0
    fi

    # Add new container user matching the host user owning the SOF
    # checkout
    local sof_user; sof_user="$(id "$sof_uid")" || {
        sof_user=sof_zephyr_docker_builder

        local sof_guid; sof_guid="$(stat --printf='%g' .)"

        getent group "$sof_guid" ||
            sudo groupadd -g  "$sof_guid" sof_zephyr_docker_group

        sudo useradd -m -u "$sof_uid" -g "$sof_guid" "$sof_user"

        local current_user; current_user="$(id -un)"

        # Copy sudo permissions just in case the build needs it
        sudo sed -e "s/$current_user/$sof_user/" /etc/sudoers.d/"$current_user" |
            sudo tee -a /etc/sudoers.d/"$sof_user"
        sudo chmod --reference=/etc/sudoers.d/"$current_user" \
             /etc/sudoers.d/"$sof_user"
    }

    # Safety delay: slower infinite loops are much better
    sleep 0.5

    # Double sudo to work around some funny restriction in
    # zephyr-build:/etc/sudoers: 'user' can do anything but... only as
    # root.
    sudo sudo -u "$sof_user" REAL_CC="$REAL_CC" "$0" "$@"
    exit "$?"
}

exec_as_sof_uid "$@"

# Work in progress: move more code to a function
# https://github.com/thesofproject/sof-test/issues/740

# As of container version 0.18.4,
# https://github.com/zephyrproject-rtos/docker-image/blob/master/Dockerfile
# installs two SDKs: ZSDK_VERSION=0.12.4 and ZSDK_ALT_VERSION=0.13.1
# ZEPHYR_SDK_INSTALL_DIR points at ZSDK_VERSION but we want the latest.
unset ZEPHYR_SDK_INSTALL_DIR

# Zephyr's CMake does not look in /opt but it searches $HOME
ls -ld /opt/toolchains/zephyr-sdk-*
ln -s  /opt/toolchains/zephyr-sdk-*  ~/ || true

if test -e .west || test -e zephyr; then
    init_update=''
else
    init_update='-u'
fi

# To investigate what went wrong enable the trailing comment.
# This cannot be enabled by default for automation reasons.
./sof/scripts/xtensa-build-zephyr.py $init_update --no-interactive "$@" # || /bin/bash
