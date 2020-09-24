#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

set -e
set -x

uid="$1"; shift

unset https_proxy http_proxy
rm /etc/apt/apt.conf

apt-get install -y python3-pyelftools
stat "$(pwd)"
sudo -u "#$uid" "$@"
