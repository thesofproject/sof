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

FROM ubuntu:20.04
ARG UID=1000

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
	echo "sof:test0000" | chpasswd && adduser sof sudo
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

ARG GITHUB_SOF=https://github.com/thesofproject
ARG XT_OVERLAY_REPO=$GITHUB_SOF/xtensa-overlay.git
ARG CT_NG_REPO=$GITHUB_SOF/crosstool-ng.git
# build cross compiler
USER sof
RUN cd "$HOME" && \
	git clone $CLONE_DEFAULTS --branch sof-gcc10.2 $XT_OVERLAY_REPO && \
	git clone $CLONE_DEFAULTS --branch sof-gcc10x $CT_NG_REPO && \
	cd crosstool-ng && \
	./bootstrap && ./configure --prefix=`pwd` && make && make install && \
	for arch in byt hsw apl cnl imx imx8m imx8ulp rn rmb mt8186 mt8195; do \
		echo "$arch: ct-ng build start..." && \
		cp config-${arch}-gcc10.2-gdb9 .config && \
		# replace the build dist to save space
		sed -i 's#${CT_TOP_DIR}\/builds#\/home\/sof\/work#g' .config && \
		# gl_cv_func_getcwd_path_max=yes is used to avoid too-long confdir3/confdir3/...
		gl_cv_func_getcwd_path_max=yes ./ct-ng build && \
		./ct-ng distclean ; \
	done && \
	echo "Stage2: xtensa-overlay, crosstool-ng are done!"

ENV PATH="/home/sof/work/xtensa-byt-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-hsw-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-apl-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-cnl-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-rn-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-rmb-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-imx-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-imx8m-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-imx8ulp-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-mt8186-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-mt8195-elf/bin:${PATH}"

ARG NEWLIB_REPO=https://github.com/jcmvbkbc/newlib-xtensa.git
RUN cd "$HOME" && \
	git clone $CLONE_DEFAULTS --branch xtensa $NEWLIB_REPO && \
	cd newlib-xtensa && \
	for arch in byt hsw apl cnl imx imx8m imx8ulp rn rmb mt8186 mt8195; do \
		./configure --target=xtensa-${arch}-elf \
		--prefix=/home/sof/work/xtensa-root && \
		make && make install && \
		rm -rf etc/config.cache; \
	done && \
	echo "Stage3: newlib-xtensa is done!"

# log all hashes for each repo to a file
ARG SOF_WORK="$HOME"/work
# FIXME: this will fail when any whitespace in SOF_WORK. bash array probably good option to consider
ARG REPOS="$SOF_WORK/alsa/alsa-lib $SOF_WORK/alsa/alsa-utils $HOME/xtensa-overlay $HOME/crosstool-ng $HOME/newlib-xtensa"
ARG HASH_LIST="$SOF_WORK"/sof_docker_hash.txt
RUN echo "Build date: $(date +%Y%m%d)" > "$HASH_LIST" && set -e && \
	for repo in $REPOS; do \
		cd "$repo"; pwd; git log --oneline -5; \
	done >> "$HASH_LIST" && \
	cd "$HOME" && rm -rf xtensa-overlay crosstool-ng newlib-xtensa; \
	echo "Stage4: git versions saved and repos deleted."

# Create direcroties for the host machines sof directories to be mounted.
RUN mkdir -p "$HOME"/work/sof.git

USER sof
WORKDIR /home/sof/work/sof.git/
