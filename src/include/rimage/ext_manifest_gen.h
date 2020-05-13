/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

/*
 * Extended manifest is a place to store metadata about firmware, known during
 * compilation time - for example firmware version or used compiler.
 * Given information are read on host side before firmware startup.
 * This part of output binary is not signed.
 *
 * To add new content to ext_man, in firmware code define struct which starts
 * with ext_man_elem_head followed by usage dependent content and place whole
 * struct in "fw_metadata" section. Moreover kernel code should be modified to
 * properly read new packet.
 *
 * Extended manifest designed to be extensible. In header there is a field which
 * describe header length, so after appending some data to header then it can be
 * easily skipped by device with older version of this header.
 * From other side, unknown ext_man elements should be just skipped by host,
 * to be backward compatible. Field ext_man_elem_header.elem_size should be
 * used in such a situation.
 */

#ifndef __EXT_MAN_H__
#define __EXT_MAN_H__

#include <rimage/rimage.h>

#define EXT_MAN_DATA_SECTION ".fw_metadata"

int ext_man_write(struct image *image);

#endif /* __EXT_MAN_H__ */
