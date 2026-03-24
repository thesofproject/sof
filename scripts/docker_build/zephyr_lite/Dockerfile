# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation. All rights reserved.

# Use zephyr-build as base image
FROM ghcr.io/zephyrproject-rtos/zephyr-build:v0.29.0 as base

# Remove additional toolchains.
# As this is not ideal solution there is a plan to build docker image without zephyr-build as the base
# and install only needed toolchains in the future.
USER root

RUN rm -rvf /opt/toolchains/zephyr-sdk-1.0.0/hosttools/sysroots && \
    rm -rvf /opt/fvps && \
    cd /opt/toolchains/zephyr-sdk-1.0.0/gnu && \
      rm -rvf arc* \
          micro* \
          mips* \
          nios* \
          risc* \
          sparc* \
          x86* \
          xtensa-espressif* \
          xtensa-sample* \
          xtensa-dc233c*

# Use ubuntu24.04 as base for zephyr-lite
FROM ubuntu:24.04 as zephyr-lite

# Copy only required files from base image to zephyr-lite
# /opt for toolchains and sdk
# /usr for binaries and libs
# /home for libs installed in .local
# /etc/ssl for ssl certs for python packages
COPY --from=base /opt /opt
COPY --from=base /usr /usr
COPY --from=base /home /home
COPY --from=base /etc/ssl /etc/ssl

USER root

# Create a user if it doesn't already exist and grant them permission to /home/user.
# Add user to dialout and sudo group
RUN useradd -ms /bin/bash user && \
    chown -R user:user /home/user && \
    chown -R user:user /opt/python && \
    usermod -a -G dialout,sudo user

USER user

# Install cmake and jsonschema in venv
RUN /opt/python/venv/bin/pip install 'cmake>=3.21' jsonschema

# Set zephyr env variables
ENV PATH="/opt/python/venv/bin/:$PATH"
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk-1.0.0
ENV ZSDK_VERSION=1.0.0
ENV ZEPHYR_TOOLCHAIN_VARIANT=zephyr

CMD ["/bin/bash", "-l"]
