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
		build-essential \
		git \
		python \
		zlib1g-dev \
		libglib2.0-dev \
		libpixman-1-dev \
		pkg-config \
		sudo


# Set up sof user
RUN useradd --create-home -d /home/sof -u $UID -G sudo sof && \
echo "sof:test0000" | chpasswd && adduser sof sudo
ENV HOME /home/sof

# build qemu
USER sof
RUN cd /home/sof && git clone https://github.com/thesofproject/qemu.git && \
	cd qemu && git checkout sof-stable && \
# replace the submodule git repo to github mirror
	sed -i 's#git://git.qemu.org#https://github.com/qemu#g' .gitmodules && \
	sed -i 's#git://git.qemu-project.org#https://github.com/qemu#g' .gitmodules && \
	./configure --prefix=`pwd`/ --target-list=xtensa-softmmu --enable-coroutine-pool && \
	make

# Create direcroties for the host machines sof/soft directories to be mounted.
RUN mkdir -p /home/sof/sof.git && \
    mkdir -p /home/sof/soft.git

WORKDIR /home/sof/qemu/
