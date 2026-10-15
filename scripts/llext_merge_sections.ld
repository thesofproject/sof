/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Linker script for the partial (-r) pre-link of relocatable LLEXT modules,
 * used by llext_link_helper.py together with --force-group-allocation.
 *
 * A partial link keeps every per-function section and every C++ COMDAT
 * group (inline functions, template instances, vtables) as a separate
 * output section, so a C++ module can end up with thousands of sections.
 * Fold them into one section per type, so the module gets one .text, one
 * .rodata, one .data and one .bss, the same as after a final link.
 *
 * Specially named sections (.cold, .coldrodata, .module, ...) don't match
 * these patterns and are kept as they are.
 */
SECTIONS
{
	.text 0 : { *(.literal .text .literal.* .text.*) }
	.rodata 0 : { *(.rodata .rodata.*) }
	.data 0 : { *(.data .data.* .data.rel.ro .data.rel.ro.*) }
	.bss 0 : { *(.bss .bss.* COMMON) }
}
