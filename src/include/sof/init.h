/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_INIT_H__
#define __INCLUDE_INIT_H__

#include <platform/platform.h>

struct sof;

/* main firmware entry point - argc and argv not currently used */
int main(int argc, char *argv[]);

int master_core_init(struct sof *sof);

int slave_core_init(struct sof *sof);

int arch_init(struct sof *sof);

#endif
