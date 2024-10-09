/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOF_TRACE_PREPROC_H__
#define __SOF_TRACE_PREPROC_H__

#ifdef CONFIG_TRACE
#error "SOF dma-trace (CONFIG_TRACE) not supported in Zephyr builds. \
	Please use CONFIG_ZEPHYR_LOG instead."
#endif

#ifndef CONFIG_ZEPHYR_LOG

/*
 * Unlike the XTOS version of this macro, this does not silence all
 * compiler warnings about unused variables.
 */
#define SOF_TRACE_UNUSED(arg1, ...) ((void)arg1)

#endif /* CONFIG_ZEPHYR_LOG */

#endif /* __SOF_TRACE_PREPROC_H__ */
