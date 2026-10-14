/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016,2024 Intel Corporation.
 */

/**
 * \file include/sof/trace/trace-boot.h
 * \brief Early bootloader, architecture, and peripheral initialization trace checkpoint codes.
 * \ingroup trace_logging_api
 */

#ifndef __SOF_TRACE_TRACE_BOOT_H__
#define __SOF_TRACE_TRACE_BOOT_H__

/** \name Bootloader Phase Trace Codes
 *  Checkpoint values written during bootloader stage before OS handoff.
 *  @{
 */
#define TRACE_BOOT_LDR_ENTRY		0x100	/**< Bootloader entry point reached */
#define TRACE_BOOT_LDR_HPSRAM		0x110	/**< High-Performance SRAM powered and configured */
#define TRACE_BOOT_LDR_MANIFEST		0x120	/**< Firmware manifest signature verified */
#define TRACE_BOOT_LDR_LPSRAM		0x130	/**< Low-Power SRAM initialized */
#define TRACE_BOOT_LDR_L1DRAM		0x140	/**< L1 data RAM configured */
#define TRACE_BOOT_LDR_JUMP		0x150	/**< Branch to main firmware image entry */

#define TRACE_BOOT_LDR_PARSE_MODULE	0x210	/**< Parsing firmware module header */
#define TRACE_BOOT_LDR_PARSE_SEGMENT	0x220	/**< Loading firmware code/data segment */
/** @} */

/** \name General Subsystem Boot Trace Codes
 *  Major stage markers written to hardware registers during early bootstrap.
 *  @{
 */
#define TRACE_BOOT_START		0x1000	/**< SOF firmware execution started */
#define TRACE_BOOT_ARCH			0x2000	/**< Architecture-level setup started */
#define TRACE_BOOT_SYS			0x3000	/**< Core system services initialization */
#define TRACE_BOOT_PLATFORM		0x4000	/**< Platform and peripheral device setup */
/** @} */

/** \name System Services Boot Codes
 *  Checkpoint codes for core OS and runtime services.
 *  @{
 */
#define TRACE_BOOT_SYS_HEAP		(TRACE_BOOT_SYS + 0x100)	/**< Memory heaps initialized */
#define TRACE_BOOT_SYS_TRACES		(TRACE_BOOT_SYS + 0x200)	/**< Early trace subsystem ready */
#define TRACE_BOOT_SYS_NOTIFIER		(TRACE_BOOT_SYS + 0x300)	/**< System notification dispatcher initialized */
#define TRACE_BOOT_SYS_POWER		(TRACE_BOOT_SYS + 0x400)	/**< Power management framework initialized */
/** @} */

/** \name Platform & Hardware Driver Boot Codes
 *  Detailed peripheral driver checkpoint codes.
 *  @{
 */
#define TRACE_BOOT_PLATFORM_ENTRY	(TRACE_BOOT_PLATFORM + 0x100)	/**< Entering platform hardware init */
#define TRACE_BOOT_PLATFORM_IRQ		(TRACE_BOOT_PLATFORM + 0x110)	/**< Interrupt controller configured */
#define TRACE_BOOT_PLATFORM_MBOX	(TRACE_BOOT_PLATFORM + 0x120)	/**< Shared SRAM mailboxes mapped */
#define TRACE_BOOT_PLATFORM_SHIM	(TRACE_BOOT_PLATFORM + 0x130)	/**< Hardware SHIM registers initialized */
#define TRACE_BOOT_PLATFORM_PMC		(TRACE_BOOT_PLATFORM + 0x140)	/**< Power Management Controller linked */
#define TRACE_BOOT_PLATFORM_TIMER	(TRACE_BOOT_PLATFORM + 0x150)	/**< Hardware timers and sys ticks initialized */
#define TRACE_BOOT_PLATFORM_CLOCK	(TRACE_BOOT_PLATFORM + 0x160)	/**< DSP clocks and PLL configured */
#define TRACE_BOOT_PLATFORM_SCHED	(TRACE_BOOT_PLATFORM + 0x170)	/**< Real-time scheduler registered */
#define TRACE_BOOT_PLATFORM_AGENT	(TRACE_BOOT_PLATFORM + 0x180)	/**< System agent watchdog active */
#define TRACE_BOOT_PLATFORM_CPU_FREQ	(TRACE_BOOT_PLATFORM + 0x190)	/**< CPU frequency scaling configured */
#define TRACE_BOOT_PLATFORM_SSP_FREQ	(TRACE_BOOT_PLATFORM + 0x1A0)	/**< SSP serial port clocks configured */
#define TRACE_BOOT_PLATFORM_DMA		(TRACE_BOOT_PLATFORM + 0x1B0)	/**< DMA controllers probed */
#define TRACE_BOOT_PLATFORM_IPC		(TRACE_BOOT_PLATFORM + 0x1C0)	/**< Host IPC transport initialized */
#define TRACE_BOOT_PLATFORM_IDC		(TRACE_BOOT_PLATFORM + 0x1D0)	/**< Inter-DSP Communication initialized */
#define TRACE_BOOT_PLATFORM_DAI		(TRACE_BOOT_PLATFORM + 0x1E0)	/**< Digital audio interfaces initialized */
#define TRACE_BOOT_PLATFORM_SSP		(TRACE_BOOT_PLATFORM + 0x1F0)	/**< SSP/I2S hardware ready */
#define TRACE_BOOT_PLATFORM_SPI		(TRACE_BOOT_PLATFORM + 0x200)	/**< SPI hardware ready */
#define TRACE_BOOT_PLATFORM_DMA_TRACE	(TRACE_BOOT_PLATFORM + 0x210)	/**< Host DMA trace worker active */
/** @} */

#endif /* __SOF_TRACE_TRACE_BOOT_H__ */
