#
# Defines a docker image that can run Xtensa Qemu
#
# Usage:
# check out sof
# build docker image:
# cd scripts/docker_build/sof_qemu
# ./docker-build.sh
#
# run docker image:
# in sof repo
# ./scripts/docker-qemu.sh scrpits-or-command-you-want-run
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
		vim \
		jed \
		dialog \
		apt-utils \
		autoconf \
		build-essential \
		git \
		python \
		zlib1g-dev \
		libglib2.0-dev \
		libpixman-1-dev \
		pkg-config \
		sudo \
		bsdmainutils \
		bison \
		flex

# Set up sof user
RUN useradd --create-home -d /home/sof -u $UID -G sudo sof && \
echo "sof:test0000" | chpasswd && adduser sof sudo
ENV HOME /home/sof

ARG GITHUB_SOF=https://github.com/thesofproject
ARG SOF_REPO=$GITHUB_SOF/qemu.git
ARG CLONE_DEFAULTS="--depth 5"
# build qemu
USER sof
RUN cd /home/sof && git clone $CLONE_DEFAULTS --branch sof-v4.2 $SOF_REPO && \
	cd qemu && \
# replace the submodule git repo to github mirror
	sed -i 's#git://git.qemu.org#https://github.com/qemu#g' .gitmodules && \
	sed -i 's#git://git.qemu-project.org#https://github.com/qemu#g' .gitmodules && \
	./configure --prefix=`pwd`/ --target-list=xtensa-softmmu --enable-coroutine-pool && \
	make

# Create direcroties for the host machines sof directories to be mounted.
RUN mkdir -p /home/sof/sof.git

WORKDIR /home/sof/qemu/
