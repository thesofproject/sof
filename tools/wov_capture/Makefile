# SPDX-License-Identifier: BSD-3-Clause
# Makefile for wov tools (wov_capture_app, wov_blocking_read, wov_blocking_read_tinyalsa)

CC       ?= gcc
CFLAGS   += -Wall -Wextra -O2 -g -D_GNU_SOURCE

.PHONY: all clean install wov_capture_app wov_blocking_read wov_blocking_read_tinyalsa

all: wov_capture_app

wov_capture_app: wov_capture_app.c
	$(CC) $(CFLAGS) $^ -lm -o $@

wov_blocking_read: wov_blocking_read.c
	$(CC) $(CFLAGS) $^ -lasound -o $@

wov_blocking_read_tinyalsa: wov_blocking_read_tinyalsa.c
	$(CC) $(CFLAGS) $^ -ltinyalsa -o $@

clean:
	rm -f *.o wov_capture_app wov_blocking_read wov_blocking_read_tinyalsa

# Install to a DUT via scp: make install DUT=root@aphid
install: all
	scp wov_capture_app $(DUT):/usr/local/bin/
