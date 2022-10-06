#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2022 Intel Corporation. All rights reserved.

# This is a "brute force" solution to filesystem permission issues:
#
# If the current user does not own the current directory then this
# wrapper script switches to the user who does own the current directory
# before running the given command.

# If no user owns the current directory, a user who does gets created
# first!

# The main use case is to run this first thing inside a container to
# solve file ownership mismatches.

# `docker run --user=$(id -un) ...` achieves something very similar
# without any code except the resulting user many not exist inside the
# container. Some commands may not like that.
#
# To understand more about the Docker problem solved here take a look at
# https://stackoverflow.com/questions/35291520/docker-and-userns-remap-how-to-manage-volume-permissions-to-share-data-betwee
# and many other similar questions.

# TODO: replace sudo with gosu?

set -e
set -x

# TODO: rename the "sof_" bits

main()
{
    sof_uid="$(stat --printf='%u' .)"
    local current_uid; current_uid="$(id -u)"
    if test "$current_uid" = "$sof_uid"; then
        exec "$@"
    else
        exec_as_sof_uid "$@"
    fi
}

exec_as_sof_uid()
{
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

    # Double sudo to work around some funny restriction in
    # zephyr-build:/etc/sudoers: 'user' can do anything but... only as
    # root.
    sudo sudo -u "$sof_user" REAL_CC="$REAL_CC" "$@"
    exit "$?"
}

main "$@"
