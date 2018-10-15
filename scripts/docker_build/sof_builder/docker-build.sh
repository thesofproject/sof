#!/bin/bash
if [ -f "/etc/apt/apt.conf" ]; then
	cp /etc/apt/apt.conf ./
else
	touch apt.conf
fi
docker build --build-arg UID=$(id -u) --build-arg host_http_proxy=$http_proxy --build-arg host_https_proxy=$https_proxy -t sof .
