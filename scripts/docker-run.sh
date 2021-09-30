#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Runs a given script in the docker container you can generate from the
# docker_build directory.
# Example:
#  To build sof for baytrail:
#  ./scripts/docker-run.sh ./scripts/xtensa-build-all.sh byt
#  To build topology:
#  ./scripts/docker-run.sh ./scripts/build-tools.sh

# set -x

set -e

SOF_TOP="$(cd "$(dirname "$0")"/.. && /bin/pwd)"

# Log the container version. Especially useful when forgetting to update
# the 'sof' shortcut.
docker images sof

if tty --quiet; then
    SOF_DOCKER_RUN="$SOF_DOCKER_RUN --tty"
fi

docker run -i -v "${SOF_TOP}":/home/sof/work/sof.git \
	--env CMAKE_BUILD_TYPE \
	--env PRIVATE_KEY_OPTION \
	--env http_proxy="$http_proxy" \
	--env https_proxy="$https_proxy" \
	--user "$(id -u)" \
	$SOF_DOCKER_RUN \
	sof "$@"
