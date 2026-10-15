/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * LLEXT linker fragment for the ffmpeg_dec hot/cold split
 * (CONFIG_FFMPEG_DEC_COLD_SPLIT). Collects the FFmpeg code (renamed to .cold in
 * the archives by ffmpeg.cmake) plus any av_cold leftovers into an explicit
 * .cold output section so the LLEXT loader (llext_manager) relocates it to DRAM.
 *
 * Placed with INSERT AFTER .bss so .cold gets the highest link-time address and
 * cannot overlap the fixed-LMA .text/.rodata/.hash of the ace "mirrored" -shared
 * layout (forcing it before/after .text overran those budgets). Its link address
 * is irrelevant at runtime: the loader reads the .cold section and copies it to
 * the cold/DRAM region. Cross-region .cold<->.text calls are handled by
 * -mlongcalls (FFmpeg and module code).
 */
SECTIONS
{
	.cold : {
		*(.cold .cold.*)
		*(.literal.unlikely .literal.unlikely.*)
		*(.text.unlikely .text.unlikely.*)
	}
} INSERT AFTER .bss;
