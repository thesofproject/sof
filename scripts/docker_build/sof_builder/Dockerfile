# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Defines a docker image that can build sound open firmware
#
# Usage:
# check out sof
# build docker image:
# docker build --build-arg UID=$(id -u) -t sof .
# docker run -it  -v <insert sof dir here>:/home/sof/workdir --user `id -u` sof
#
# For incremental builds:
# docker run -it  -v <insert sof dir here>:/home/sof/work/sof.git --user `id -u` sof ./incremental.sh
#

FROM ubuntu:22.04
# pick random high value to intentionally mismatch local UID
ARG UID=1999

ARG host_http_proxy
ARG host_https_proxy
ENV http_proxy $host_http_proxy
ENV https_proxy $host_https_proxy

# for non-interactive package install
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get -y update && \
	    apt-get install -y \
		dialog \
		apt-utils \
		vim \
		jed \
		autoconf \
		bison \
		build-essential \
		ninja-build \
		python3-pyelftools \
		flex \
		gawk \
		gettext \
		git \
		gperf \
		help2man \
		libncurses5-dev \
		libssl-dev \
		libtool \
		libtool-bin \
		pkg-config \
		software-properties-common \
		sudo \
		texinfo \
		tree \
		udev \
		wget \
		unzip \
		cmake \
		python3 \
		libglib2.0-dev

ARG CLONE_DEFAULTS="--depth 5"

# Set up sof user
RUN useradd --create-home -d /home/sof -u $UID -G sudo sof && \
	echo 'sof ALL = NOPASSWD: ALL' > /etc/sudoers.d/sof && \
	chmod 0440 /etc/sudoers.d/sof && \
	echo "sof:test0000" | chpasswd && adduser sof sudo
# allow others to read/traverse sof home
RUN chmod o+rx /home/sof
ENV HOME /home/sof

# Use ToT alsa utils for the latest topology patches.
# Note: For alsa-lib, set default library directory to /usr/lib/x86_64-linux-gnu.
#       By default it goes to /usr/lib64, but Ubuntu 20.04's default shared library
#       path does not have this. Typical build error is like,
# alsatplg: error while loading shared libraries: libatopology.so.2: cannot open shared object file: No such file or directory
RUN mkdir -p "$HOME"/work/alsa && cd "$HOME"/work/alsa && \
	git clone $CLONE_DEFAULTS https://github.com/thesofproject/alsa-lib.git && \
	git clone $CLONE_DEFAULTS https://github.com/thesofproject/alsa-utils.git && \
	cd "$HOME"/work/alsa/alsa-lib && ./gitcompile --prefix=/usr --libdir=/usr/lib/x86_64-linux-gnu/ &&  make install && \
	cd "$HOME"/work/alsa/alsa-utils && ./gitcompile &&  make install && \
	chown -R sof:sof "$HOME" && \
	echo "Stage1: alsa-lib, alsa-utils are done!"

USER sof

# log all hashes for each repo to a file
ARG SOF_WORK="$HOME"/work
# FIXME: this will fail when any whitespace in SOF_WORK. bash array probably good option to consider
ARG REPOS="$SOF_WORK/alsa/alsa-lib $SOF_WORK/alsa/alsa-utils"
ARG HASH_LIST="$SOF_WORK"/sof_docker_hash.txt
RUN echo "Build date: $(date +%Y%m%d)" > "$HASH_LIST" && set -e && \
	for repo in $REPOS; do \
		cd "$repo"; pwd; git log --oneline -5; \
	done >> "$HASH_LIST" && \
	echo "Stage2: git versions saved."

# Create direcroties for the host machines sof directories to be mounted.
RUN mkdir -p "$HOME"/work/sof.git

USER sof
WORKDIR /home/sof/work/sof.git/
