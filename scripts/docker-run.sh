#!/bin/sh
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

# Runs a given script in the docker container you can generate from the
# docker_build directory.
# Example:
#  To build sof for tigerlake:
#  ./scripts/docker-run.sh ./scripts/xtensa-build-all.sh tgl
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

# The --user option below can cause the command to run as a user who
# does not exist in the container. So far so good but in case something
# ever goes wrong try replacing --user with the newer
# scripts/sudo-cwd.sh script.
test "$(id -u)" = 1000 ||
  >&2 printf "Warning: this script should be run as user ID 1000 to match the container's account\n"

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
	$SOF_DOCKER_RUN \
	thesofproject/sof:20250410_no-alsa ./scripts/sudo-cwd.sh "$@"
