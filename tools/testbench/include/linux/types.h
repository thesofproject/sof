/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __TESTBENCH_LINUX_TYPES_H__
#define __TESTBENCH_LINUX_TYPES_H__

#include <stdint.h>

/*
 * This header files allows to include asoc.h for topology parsing to
 * non-gcc builds with other C library, e.g. the one that is used by
 * xt-xcc compiler. The kernel linux/types.h cannot be used because
 * the other definitions there are not compatible with the toolchain.
 */

/* There are minimum types needed for alsa/sound/uapi/asoc.h */

typedef int64_t __le64;
typedef int32_t __le32;
typedef int16_t __le16;
typedef uint8_t __u8;

#endif /* __TESTBENCH_LINUX_TYPES_H__ */
