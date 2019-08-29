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

docker run -i -t -v `pwd`:/home/sof/work/sof.git \
	   --user `id -u` sof $@
