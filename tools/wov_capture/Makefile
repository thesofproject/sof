# SPDX-License-Identifier: BSD-3-Clause
# Makefile for wov_capture_app
#
# Requires tinycompress headers and shared library.
# Point TINYCOMPRESS_DIR at a tinycompress source tree that has been built
# (i.e. ./configure && make already ran), or set TINYCOMPRESS_INC / TINYCOMPRESS_LIB
# separately.
#
# Examples:
#   make                                  # auto-detect from ../../../tinycompress
#   make TINYCOMPRESS_DIR=/home/lrg/work/tinycompress
#   make TINYCOMPRESS_INC=/usr/include/tinycompress TINYCOMPRESS_LIB=/usr/lib

BINARY   := wov_capture_app
SRCS     := wov_capture_app.c
OBJS     := $(SRCS:.c=.o)

CC       ?= gcc
CFLAGS   += -Wall -Wextra -O2 -g
LDFLAGS  +=

# -------------------------------------------------------------------------
# tinycompress location
# -------------------------------------------------------------------------
# Try to find tinycompress relative to this Makefile (up 3 levels = sof-wov root,
# then sibling tinycompress repo), unless the caller overrides.
SOF_WOV_ROOT := $(realpath $(dir $(lastword $(MAKEFILE_LIST)))../../..)
TINYCOMPRESS_DIR_DEFAULT := $(realpath $(SOF_WOV_ROOT)/../tinycompress)

TINYCOMPRESS_DIR ?= $(TINYCOMPRESS_DIR_DEFAULT)

ifdef TINYCOMPRESS_INC
  TC_INC := $(TINYCOMPRESS_INC)
else
  TC_INC := $(TINYCOMPRESS_DIR)/include
endif

ifdef TINYCOMPRESS_LIB
  TC_LIB := $(TINYCOMPRESS_LIB)
else
  TC_LIB := $(TINYCOMPRESS_DIR)/src/lib/.libs
endif

CFLAGS  += -I$(TC_INC)
LDFLAGS += -L$(TC_LIB) -ltinycompress -Wl,-rpath,$(TC_LIB)

# -------------------------------------------------------------------------
# Targets
# -------------------------------------------------------------------------
.PHONY: all clean install

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BINARY)

# Install to a DUT via scp: make install DUT=root@spider
install: $(BINARY)
	scp $(BINARY) $(DUT):/usr/local/bin/$(BINARY)
