/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_INIT_H__
#define __SOF_INIT_H__

#include <arch/init.h>

struct sof;

/* main firmware entry point - argc and argv not currently used */
int main(int argc, char *argv[]);

int master_core_init(int argc, char *argv[], struct sof *sof);

int arch_init(void);

#endif /* __SOF_INIT_H__ */
