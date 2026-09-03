#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation. All rights reserved.

docker build --build-arg UID=$(id -u) --build-arg host_http_proxy="$http_proxy" \
	--build-arg host_https_proxy="$https_proxy" -t sofqemu .
