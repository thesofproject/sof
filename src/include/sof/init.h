/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_INIT_H__
#define __SOF_INIT_H__

struct sof;

/* main firmware entry point - argc and argv not currently used */
int main(int argc, char *argv[]);

#if CONFIG_MULTICORE

int secondary_core_init(struct sof *sof);

#endif /* CONFIG_MULTICORE */

int arch_init(void);

#endif /* __SOF_INIT_H__ */
