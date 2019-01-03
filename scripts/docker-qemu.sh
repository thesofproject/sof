#!/bin/sh

# Runs a given script in the docker container you can generate from the
# docker_build directory.

docker run -i --privileged -v `pwd`:/home/sof/sof.git \
	   --user `id -u` sofqemu $@
