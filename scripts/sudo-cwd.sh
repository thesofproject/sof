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

main()
{
    cwd_uid="$(stat --printf='%u' .)"
    local current_uid; current_uid="$(id -u)"
    if test "$current_uid" = "$cwd_uid"; then
        exec "$@"
    else
        exec_as_cwd_uid "$@"
    fi
}

exec_as_cwd_uid()
{
    # If missing, add new user owning the current directory
    local cwd_user; cwd_user="$(id "$cwd_uid")" || {
        cwd_user='cwd_user'

        local cwd_guid; cwd_guid="$(stat --printf='%g' .)"

        getent group "$cwd_guid" ||
            sudo groupadd -g  "$cwd_guid" 'cwd_group'

        sudo useradd -m -u "$cwd_uid" -g "$cwd_guid" "$cwd_user"

        # Add cwd_user to the sof group for group write access
        sudo usermod -aG sof "$cwd_user"

        local current_user; current_user="$(id -un)"

        # Copy sudo permissions just in case the build needs it
        if test -e /etc/sudoers.d/"$current_user"; then
          sudo sed -e "s/$current_user/$cwd_user/" /etc/sudoers.d/"$current_user" |
            sudo tee -a /etc/sudoers.d/"$cwd_user"
          sudo chmod --reference=/etc/sudoers.d/"$current_user" \
             /etc/sudoers.d/"$cwd_user"
        fi
    }

    # Double sudo to work around some funny restriction in
    # zephyr-build:/etc/sudoers: 'user' can do anything but... only as
    # root.
    # Passing empty http[s]_proxy is OK
    # shellcheck disable=SC2154
    sudo sudo -u "$cwd_user" REAL_CC="$REAL_CC" \
         http_proxy="$http_proxy"  https_proxy="$https_proxy" \
         "$@"

    exit "$?"
}

main "$@"
