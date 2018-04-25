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

FROM ubuntu
ARG UID=1000

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
		libncurses5-dev \
		libssl-dev \
		libtool \
		libtool \
		libtool-bin \
		pkg-config \
		software-properties-common \
		sudo \
		texinfo \
		udev \
		wget

# Use gcc/g++ 7.
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y
RUN apt-get -y update
RUN apt-get install -y gcc-7 g++-7
RUN apt-get clean
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 70 --slave /usr/bin/g++ g++ /usr/bin/g++-7

# Use ToT alsa utils for the latest topology patches.
RUN mkdir -p /root/alsa-build
RUN cd /root/alsa-build && git clone git://git.alsa-project.org/alsa-lib.git
RUN cd /root/alsa-build && git clone git://git.alsa-project.org/alsa-utils.git
RUN cd /root/alsa-build/alsa-lib && ./gitcompile &&  make install
RUN cd /root/alsa-build/alsa-utils && ./gitcompile &&  make install

RUN useradd --create-home -d /home/sof -u $UID -G sudo sof
RUN echo "sof:test0000" | chpasswd
RUN adduser sof sudo
ENV HOME /home/sof

# pull all sof repositories that are needed
USER sof
RUN git clone git://git.alsa-project.org/sound-open-firmware.git /home/sof/sof.git
RUN git clone git://git.alsa-project.org/sound-open-firmware-tools.git /home/sof/soft.git
RUN git clone https://github.com/01org/osadsp-crosstool-ng.git /home/sof/ct-ng.git
RUN git clone https://github.com/jcmvbkbc/newlib-xtensa.git /home/sof/newlib-xtensa.git

USER sof
RUN mkdir -p /home/sof/bin
WORKDIR /home/sof/soft.git
RUN ./autogen.sh
RUN ./configure --prefix=/home/sof/bin
RUN make
RUN make install

ENV PATH="/home/sof/bin/bin:${PATH}"

USER sof
WORKDIR /home/sof/ct-ng.git
RUN for arch in byt hsw bxt cnl; do \
	./bootstrap && \
	./configure --prefix=`pwd` && \
	make && \
	make install && \
	./ct-ng xtensa-${arch}-elf && \
	./ct-ng build && \
	make distclean; \
done

ENV PATH="/home/sof/ct-ng.git/builds/xtensa-byt-elf/bin:${PATH}"
ENV PATH="/home/sof/ct-ng.git/builds/xtensa-hsw-elf/bin:${PATH}"
ENV PATH="/home/sof/ct-ng.git/builds/xtensa-bxt-elf/bin:${PATH}"
ENV PATH="/home/sof/ct-ng.git/builds/xtensa-cnl-elf/bin:${PATH}"

USER sof
WORKDIR /home/sof/newlib-xtensa.git
RUN git checkout -b xtensa origin/xtensa
RUN for arch in byt hsw bxt cnl; do \
	./configure --target=xtensa-${arch}-elf \
		--prefix=/home/sof/work/xtensa-root && \
	make && \
	make install && \
	make distclean; \
done

USER sof
WORKDIR /home/sof/sof.git
RUN ./autogen.sh
RUN ./configure --enable-rimage
RUN make
USER root
RUN make install

# Create direcroties for the host machines sof/soft directories to be mounted.
USER sof
RUN mkdir -p /home/sof/work/sof
RUN mkdir -p /home/sof/work/soft

USER sof
WORKDIR /home/sof/work/sof.git/

# Default to building all.
CMD ./scripts/xtensa-build-all.sh
