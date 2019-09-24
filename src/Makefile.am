# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright(c) 2019 Intel Corporation. All rights reserved.

bin_PROGRAMS = rimage

rimage_SOURCES = \
	cse.c \
	css.c \
	elf.c \
	file_simple.c \
	hash.c \
	man_apl.c \
	man_cnl.c \
	manifest.c \
	man_kbl.c \
	man_sue.c \
	man_tgl.c \
	pkcs1_5.c \
	plat_auth.c \
	rimage.c

if USE_PROVIDED_SOF_HEADER
  PROVIDED_SOF_HEADER = -Iinclude/sof
  EXTRA_SOF_MACROS =
else
  EXTRA_SOF_MACROS = -DHAS_FILE_FORMAT_H
endif

rimage_CFLAGS = \
	$(PROVIDED_SOF_HEADER) -Iinclude/rimage \
	$(EXTRA_SOF_MACROS) \
	-O2 -g -Wall -Werror -Wl,-EL \
	-Wmissing-prototypes -Wimplicit-fallthrough=3

rimage_LDADD = -lcrypto
