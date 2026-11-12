/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2025 NXP
 *
 * Keep the IPC4 module manifest entries (struct sof_man_module_manifest,
 * see platform.c) in the final ELF and load them with the image. The table
 * serves two consumers:
 *  - rimage's i.MX ext-manifest v4 generator reads the ".module" section by
 *    name to emit the "$AE1" module table for the Linux IPC4 driver;
 *  - the firmware itself resolves IPC4 module IDs to component drivers from
 *    it at runtime (platform_ipc4_get_module()), since i.MX has no
 *    rimage/IMR manifest in memory like Intel platforms.
 * Without an explicit KEEP the linker's --gc-sections would drop it.
 */
.module : ALIGN(4)
{
	KEEP(*(.module))
} >sdram0 :sdram0_phdr
