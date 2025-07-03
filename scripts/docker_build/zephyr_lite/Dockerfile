# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation. All rights reserved.

# Use zephyr-build as base image
FROM ghcr.io/zephyrproject-rtos/zephyr-build:v0.27.4 as base

# Remove additional toolchains.
# As this is not ideal solution there is a plan to build docker image without zephyr-build as the base
# and install only needeed toolchains in the future.
RUN cd /opt/toolchains/zephyr-sdk-0.17.0 && \
    sudo rm -rvf arc* \
        micro* \
        mips* \
        nios* \
        risc* \
        sparc* \
        x86* \
        xtensa-espressif* \
        xtensa-sample* \
        xtensa-dc233c*

# Some of tests require python 3.12 - instll it from source
RUN cd /tmp && wget -q --show-progress --progress=bar:force:noscroll --no-check-certificate https://www.python.org/ftp/python/3.12.9/Python-3.12.9.tgz && \
    tar -xf Python-3.12.9.tgz && \
    cd Python-3.12.9 && \
    ./configure && \
    sudo make -j$(nproc) && \
    sudo make install && \
    sudo rm -rf /tmp/Python-3*

# Reinstall python3.10 packages with python3.12
RUN python3.10 -m pip freeze > /tmp/python3.10.pip.txt && \
    cat /tmp/python3.10.pip.txt | xargs -n 1 python3.12 -m pip install || true

# Use ubuntu24.04 as base for zephyr-lite
FROM ubuntu:24.04 as zephyr-lite

# Copy needed files from base to zephyr-lite
# /opt for toolchains and sdk
# /usr for binaries and libs
# /home for libs installed in .local
COPY --from=base /opt /opt
COPY --from=base /usr /usr
COPY --from=base /home /home

USER root

# Create a user if it doesn't already exist and grant them permission to /home/user.
# Add user to dialout and sudo group
RUN useradd -ms /bin/bash user && \
    chown -R user:user /home/user && \
    usermod -a -G dialout,sudo user

USER user

# Set zephyr env variables
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk-0.17.0
ENV ZEPHYR_TOOLCHAIN_VARIANT=zephyr
