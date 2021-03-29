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

FROM ubuntu:18.04
ARG UID=1000

# Set up proxy from host
COPY apt.conf /etc/apt/
ARG host_http_proxy
ARG host_https_proxy
ENV http_proxy $host_http_proxy
ENV https_proxy $host_https_proxy

RUN apt-get -y update && \
	    apt-get install -y \
	    	autoconf \
		bison \
		build-essential \
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
RUN mkdir -p /home/sof/work/alsa && cd /home/sof/work/alsa && \
git clone $CLONE_DEFAULTS https://github.com/thesofproject/alsa-lib.git && \
git clone $CLONE_DEFAULTS https://github.com/thesofproject/alsa-utils.git && \
cd /home/sof/work/alsa/alsa-lib && ./gitcompile &&  make install && \
cd /home/sof/work/alsa/alsa-utils && ./gitcompile &&  make install &&\
chown -R sof:sof /home/sof

ARG GITHUB_SOF=https://github.com/thesofproject
ARG XT_OVERLAY_REPO=$GITHUB_SOF/xtensa-overlay.git
ARG CT_NG_REPO=$GITHUB_SOF/crosstool-ng.git
# build cross compiler
USER sof
RUN cd /home/sof && \
	git clone $CLONE_DEFAULTS --branch sof-gcc10.2 $XT_OVERLAY_REPO && \
	cd xtensa-overlay && cd ../ && \
	git clone $CLONE_DEFAULTS --branch sof-gcc10x $CT_NG_REPO && \
	mkdir -p /home/sof/work/ && \
	cd crosstool-ng && \
	./bootstrap && ./configure --prefix=`pwd` && make && make install && \
	for arch in byt hsw apl cnl imx imx8m; do \
		cp config-${arch}-gcc10.2-gdb9 .config && \
# replace the build dist to save space
		sed -i 's#${CT_TOP_DIR}\/builds#\/home\/sof\/work#g' .config && \
# gl_cv_func_getcwd_path_max=yes is used to avoid too-long confdir3/confdir3/...
		gl_cv_func_getcwd_path_max=yes ./ct-ng build && \
		./ct-ng distclean ; \
	done && \
	cd /home/sof/ && rm -rf xtensa-overlay && rm -rf crosstool-ng

ENV PATH="/home/sof/work/xtensa-byt-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-hsw-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-apl-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-cnl-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-imx-elf/bin:${PATH}"
ENV PATH="/home/sof/work/xtensa-imx8m-elf/bin:${PATH}"

ARG NEWLIB_REPO=https://github.com/jcmvbkbc/newlib-xtensa.git
RUN cd /home/sof && \
	git clone $CLONE_DEFAULTS --branch xtensa $NEWLIB_REPO && \
	cd newlib-xtensa && \
	for arch in byt hsw apl cnl imx imx8m; do \
		./configure --target=xtensa-${arch}-elf \
		--prefix=/home/sof/work/xtensa-root && \
		make && \
		make install && \
		rm -rf etc/config.cache; \
	done && \
	cd /home/sof/ && rm -rf newlib-xtensa.git


# Create direcroties for the host machines sof directories to be mounted.
RUN mkdir -p /home/sof/work/sof.git

USER sof
WORKDIR /home/sof/work/sof.git/
