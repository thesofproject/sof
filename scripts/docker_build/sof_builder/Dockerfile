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
# docker run -it  -v <insert sof dir here>:/home/sof/workdir -v <soft git dir>:/home/sof/work/soft.git --user `id -u` sof
#
# For incremental builds:
# docker run -it  -v <insert sof dir here>:/home/sof/work/sof.git -v <soft git dir>:/home/sof/work/soft.git --user `id -u` sof ./incremental.sh
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
		udev \
		wget \
		unzip


# Use ToT alsa utils for the latest topology patches.
RUN mkdir -p /root/alsa-build && cd /root/alsa-build && \
	if [ "x$http_proxy" = "x" ]; then \
		git clone git://git.alsa-project.org/alsa-lib.git && \
		git clone git://git.alsa-project.org/alsa-utils.git ; \
	else \
		git clone http://git.alsa-project.org/http/alsa-lib.git && \
		git clone http://git.alsa-project.org/http/alsa-utils.git; \
	fi && \
cd /root/alsa-build/alsa-lib && ./gitcompile &&  make install && \
cd /root/alsa-build/alsa-utils && ./gitcompile &&  make install && \
cd /root/ && rm -rf alsa-build

# Set up sof user
RUN useradd --create-home -d /home/sof -u $UID -G sudo sof && \
echo "sof:test0000" | chpasswd && adduser sof sudo
ENV HOME /home/sof

# build cross compiler
USER sof
RUN cd /home/sof && git clone https://github.com/thesofproject/xtensa-overlay.git && \
	cd xtensa-overlay && git checkout sof-gcc8.1 && cd ../ && \
	git clone https://github.com/thesofproject/crosstool-ng.git && \
	mkdir -p /home/sof/work/ && \
	cd crosstool-ng && git checkout sof-gcc8.1 && \
	./bootstrap && ./configure --prefix=`pwd` && make && make install && \
	for arch in byt hsw apl cnl; do \
		cp config-${arch}-gcc8.1-gdb8.1 .config && \
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

RUN cd /home/sof && git clone https://github.com/jcmvbkbc/newlib-xtensa.git newlib-xtensa.git && \
	cd newlib-xtensa.git && git checkout -b xtensa origin/xtensa && \
	for arch in byt hsw apl cnl; do \
		./configure --target=xtensa-${arch}-elf \
		--prefix=/home/sof/work/xtensa-root && \
		make && \
		make install && \
		make distclean; \
	done && \
	cd /home/sof/ && rm -rf newlib-xtensa.git


# Create direcroties for the host machines sof/soft directories to be mounted.
RUN mkdir -p /home/sof/work/sof.git


USER sof
WORKDIR /home/sof/work/sof.git/
