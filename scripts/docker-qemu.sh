#!/bin/sh

# Runs a given script in the docker container you can generate from the
# docker_build directory.
# Example:
#  To build sof for baytrail:
#  ./scripts/docker-run.sh ./scripts/xtensa-build-all.sh byt
#  To build topology:
#  ./scripts/docker-run.sh ./scripts/build_soft.sh

docker run -i --privileged -v `pwd`:/home/sof/sof.git \
           -v `pwd`/../soft.git:/home/sof/soft.git \
	   --user `id -u` sofqemu $@
