/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016,2024 Intel Corporation.
 */

#ifndef __SOF_INIT_H__
#define __SOF_INIT_H__

/* main firmware entry point - argc and argv not currently used */
#ifndef CONFIG_ARCH_POSIX
int main(int argc, char *argv[]);
#endif

#if CONFIG_MULTICORE

struct sof;

int secondary_core_init(struct sof *sof);

#endif /* CONFIG_MULTICORE */

#endif /* __SOF_INIT_H__ */
