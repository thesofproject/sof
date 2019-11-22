/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
 */

#ifndef __SOF_TRACE_TRACE_VERBOSE_PRIVATE_H__
#define __SOF_TRACE_TRACE_VERBOSE_PRIVATE_H__

#include <config.h>

#if CONFIG_TRACE_VERBOSE_IRQ
#define tracev_TRACE_CLASS_IRQ(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_IRQ(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_IRQ(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_IRQ(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_IRQ(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_IRQ(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_IRQ(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_IRQ(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_IRQ */

#if CONFIG_TRACE_VERBOSE_IPC
#define tracev_TRACE_CLASS_IPC(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_IPC(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_IPC(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_IPC(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_IPC(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_IPC(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_IPC(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_IPC(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_IPC */

#if CONFIG_TRACE_VERBOSE_PIPE
#define tracev_TRACE_CLASS_PIPE(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_PIPE(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_PIPE(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_PIPE(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_PIPE(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_PIPE(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_PIPE(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_PIPE(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_PIPE */

#if CONFIG_TRACE_VERBOSE_HOST
#define tracev_TRACE_CLASS_HOST(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_HOST(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_HOST(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_HOST(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_HOST(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_HOST(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_HOST(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_HOST(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_HOST */

#if CONFIG_TRACE_VERBOSE_DAI
#define tracev_TRACE_CLASS_DAI(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_DAI(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_DAI(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_DAI(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_DAI(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_DAI(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_DAI(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_DAI(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_DAI */

#if CONFIG_TRACE_VERBOSE_DMA
#define tracev_TRACE_CLASS_DMA(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_DMA(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_DMA(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_DMA(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_DMA(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_DMA(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_DMA(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_DMA(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_DMA */

#if CONFIG_TRACE_VERBOSE_SSP
#define tracev_TRACE_CLASS_SSP(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SSP(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SSP(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SSP(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_SSP(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SSP(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SSP(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SSP(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_SSP */

#if CONFIG_TRACE_VERBOSE_COMP
#define tracev_TRACE_CLASS_COMP(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_COMP(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_COMP(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_COMP(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_COMP(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_COMP(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_COMP(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_COMP(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_COMP */

#if CONFIG_TRACE_VERBOSE_WAIT
#define tracev_TRACE_CLASS_WAIT(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_WAIT(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_WAIT(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_WAIT(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_WAIT(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_WAIT(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_WAIT(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_WAIT(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_WAIT */

#if CONFIG_TRACE_VERBOSE_LOCK
#define tracev_TRACE_CLASS_LOCK(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_LOCK(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_LOCK(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_LOCK(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_LOCK(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_LOCK(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_LOCK(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_LOCK(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_LOCK */

#if CONFIG_TRACE_VERBOSE_MEM
#define tracev_TRACE_CLASS_MEM(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_MEM(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_MEM(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_MEM(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_MEM(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_MEM(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_MEM(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_MEM(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_MEM */

#if CONFIG_TRACE_VERBOSE_MIXER
#define tracev_TRACE_CLASS_MIXER(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_MIXER(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_MIXER(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_MIXER(...)\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_MIXER(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_MIXER(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_MIXER(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_MIXER(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_MIXER */

#if CONFIG_TRACE_VERBOSE_BUFFER
#define tracev_TRACE_CLASS_BUFFER(...)			\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_BUFFER(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_BUFFER(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_BUFFER(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_BUFFER(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_BUFFER(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_BUFFER(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_BUFFER(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_BUFFER */

#if CONFIG_TRACE_VERBOSE_VOLUME
#define tracev_TRACE_CLASS_VOLUME(...)			\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_VOLUME(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_VOLUME(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_VOLUME(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_VOLUME(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_VOLUME(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_VOLUME(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_VOLUME(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_VOLUME */

#if CONFIG_TRACE_VERBOSE_SWITCH
#define tracev_TRACE_CLASS_SWITCH(...)			\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SWITCH(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SWITCH(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SWITCH(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_SWITCH(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SWITCH(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SWITCH(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SWITCH(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_SWITCH */

#if CONFIG_TRACE_VERBOSE_MUX
#define tracev_TRACE_CLASS_MUX(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_MUX(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_MUX(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_MUX(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_MUX(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_MUX(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_MUX(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_MUX(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_MUX */

#if CONFIG_TRACE_VERBOSE_SRC
#define tracev_TRACE_CLASS_SRC(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SRC(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SRC(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SRC(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_SRC(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SRC(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SRC(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SRC(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_SRC */

#if CONFIG_TRACE_VERBOSE_TONE
#define tracev_TRACE_CLASS_TONE(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_TONE(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_TONE(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_TONE(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_TONE(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_TONE(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_TONE(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_TONE(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_TONE */

#if CONFIG_TRACE_VERBOSE_EQ_FIR
#define tracev_TRACE_CLASS_EQ_FIR(...)			\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_EQ_FIR(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_EQ_FIR(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_EQ_FIR(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_EQ_FIR(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_EQ_FIR(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_EQ_FIR(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_EQ_FIR(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_EQ_FIR */

#if CONFIG_TRACE_VERBOSE_EQ_IIR
#define tracev_TRACE_CLASS_EQ_IIR(...)			\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_EQ_IIR(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_EQ_IIR(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_EQ_IIR(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_EQ_IIR(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_EQ_IIR(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_EQ_IIR(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_EQ_IIR(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_EQ_IIR */

#if CONFIG_TRACE_VERBOSE_SA
#define tracev_TRACE_CLASS_SA(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SA(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SA(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SA(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_SA(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SA(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SA(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SA(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_SA */

#if CONFIG_TRACE_VERBOSE_DMIC
#define tracev_TRACE_CLASS_DMIC(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_DMIC(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_DMIC(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_DMIC(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_DMIC(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_DMIC(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_DMIC(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_DMIC(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_DMIC */

#if CONFIG_TRACE_VERBOSE_POWER
#define tracev_TRACE_CLASS_POWER(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_POWER(...)	\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_POWER(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_POWER(...)\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_POWER(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_POWER(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_POWER(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_POWER(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_POWER */

#if CONFIG_TRACE_VERBOSE_IDC
#define tracev_TRACE_CLASS_IDC(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_IDC(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_IDC(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_IDC(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_IDC(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_IDC(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_IDC(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_IDC(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_IDC */

#if CONFIG_TRACE_VERBOSE_CPU
#define tracev_TRACE_CLASS_CPU(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_CPU(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_CPU(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_CPU(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_CPU(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_CPU(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_CPU(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_CPU(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_CPU */

#if CONFIG_TRACE_VERBOSE_CLK
#define tracev_TRACE_CLASS_CLK(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_CLK(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_CLK(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_CLK(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_CLK(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_CLK(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_CLK(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_CLK(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_CLK */

#if CONFIG_TRACE_VERBOSE_EDF
#define tracev_TRACE_CLASS_EDF(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_EDF(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_EDF(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_EDF(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_EDF(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_EDF(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_EDF(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_EDF(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_EDF */

#if CONFIG_TRACE_VERBOSE_KPB
#define tracev_TRACE_CLASS_KPB(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_KPB(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_KPB(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_KPB(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_KPB(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_KPB(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_KPB(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_KPB(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_KPB */

#if CONFIG_TRACE_VERBOSE_SELECTOR
#define tracev_TRACE_CLASS_SELECTOR(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SELECTOR(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SELECTOR(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SELECTOR(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_SELECTOR(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SELECTOR(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SELECTOR(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SELECTOR(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_SELECTOR */

#if CONFIG_TRACE_VERBOSE_SCHEDULE
#define tracev_TRACE_CLASS_SCHEDULE(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SCHEDULE(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SCHEDULE(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SCHEDULE(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_SCHEDULE(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SCHEDULE(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SCHEDULE(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SCHEDULE(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_SCHEDULE */

#if CONFIG_TRACE_VERBOSE_SCHEDULE_LL
#define tracev_TRACE_CLASS_SCHEDULE_LL(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SCHEDULE_LL(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SCHEDULE_LL(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SCHEDULE_LL(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_SCHEDULE_LL(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_SCHEDULE_LL(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_SCHEDULE_LL(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_SCHEDULE_LL(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_SCHEDULE_LL */

#if CONFIG_TRACE_VERBOSE_ALH
#define tracev_TRACE_CLASS_ALH(...)		\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_ALH(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_ALH(...)	\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_ALH(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_ALH(...)		UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_ALH(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_ALH(...)	UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_ALH(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_ALH */

#if CONFIG_TRACE_VERBOSE_KEYWORD
#define tracev_TRACE_CLASS_KEYWORD(...)			\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_KEYWORD(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_KEYWORD(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_KEYWORD(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_KEYWORD(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_KEYWORD(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_KEYWORD(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_KEYWORD(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_KEYWORD */

#if CONFIG_TRACE_VERBOSE_CHMAP
#define tracev_TRACE_CLASS_CHMAP(...)			\
	trace_event(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_CHMAP(...)		\
	trace_event_with_ids(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_CHMAP(...)		\
	trace_event_atomic(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_CHMAP(...)	\
	trace_event_atomic_with_ids(__VA_ARGS__)
#else
#define tracev_TRACE_CLASS_CHMAP(...)			UNUSED(__VA_ARGS__)
#define tracev_ids_TRACE_CLASS_CHMAP(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_TRACE_CLASS_CHMAP(...)		UNUSED(__VA_ARGS__)
#define tracev_atomic_ids_TRACE_CLASS_CHMAP(...)	UNUSED(__VA_ARGS__)
#endif /* CONFIG_TRACE_VERBOSE_CHMAP */

#endif /* __SOF_TRACE_TRACE_VERBOSE_PRIVATE_H__ */
