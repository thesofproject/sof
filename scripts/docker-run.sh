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

# Log container versions
for rep in sof thesofproject/sof; do
    docker images --digests "$rep"
done

if tty --quiet; then
    SOF_DOCKER_RUN="$SOF_DOCKER_RUN --tty"
fi

# Not fatal, just a warning to allow other "creative" solutions.
# TODO: fix this with 'adduser' like in zephyr/docker-build.sh
test "$(id -u)" = 1001 ||
    >&2 printf "Warning: this script should be run as user ID 1001 to match the container\n"

set -x
docker run -i -v "${SOF_TOP}":/home/sof/work/sof.git \
	-v "${SOF_TOP}":/home/sof/work/sof-bind-mount-DO-NOT-DELETE \
	--env CMAKE_BUILD_TYPE \
	--env PRIVATE_KEY_OPTION \
	--env USE_XARGS \
	--env NO_PROCESSORS \
	--env VERBOSE \
	--env http_proxy="$http_proxy" \
	--env https_proxy="$https_proxy" \
	--user "$(id -u)" \
	$SOF_DOCKER_RUN \
	thesofproject/sof "$@"
