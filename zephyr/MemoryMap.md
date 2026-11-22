# SOF Zephyr target memory maps

This document describes the memory map of each SOF Zephyr target: the physical
memory regions, where the firmware image is linked, where the SOF heaps are
placed, and (for Intel ACE) how the L2 SRAM left after the image is split at
run time between the virtual heap, the vpage allocator and loadable modules.
For every region it names the Devicetree node, linker script region, Kconfig
option or C macro that defines it.

Audience: SOF firmware developers and reviewers.

## Contents

- [Baseline and method](#baseline-and-method)
- [Conventions](#conventions)
- [Where regions come from](#where-regions-come-from)
- [Intel ACE (MTL/ARL, LNL, PTL, WCL, NVL, NVL-S)](#intel-ace)
- [Intel cAVS 2.5 (TGL, TGL-H)](#intel-cavs-25)
- [NXP i.MX](#nxp-imx)
- [AMD ACP (derived, not linked)](#amd-acp-derived-not-linked)
- [MediaTek (derived, not linked)](#mediatek-derived-not-linked)
- [QEMU Xtensa](#qemu-xtensa)
- [Not built: imx93 A55, native_sim](#not-built-imx93-a55-native_sim)
- [Summary](#summary)
- [System heap users](#system-heap-users)
- [Notes and observations](#notes-and-observations)

## Baseline and method

The numbers reflect:

- SOF: `main` with the vregion changes of thesofproject/sof#11518
  (vregions on ACE 1.5/2.0, `CONFIG_SOF_VPAGE_LLEXT_SIZE`, single
  physical L2 budget in `vpage_init()`, and one memory configuration for all
  Intel ACE targets: 384 K system heap, VMH off, 256 K LLEXT size).
- Zephyr: `25851fa15cd`.

The 16 targets marked "built" below were built with the default
configuration for each platform (the six ACE targets again after the common
ACE memory configuration change, which does not affect the other targets):

```
CCACHE_DISABLE=1 ./sof/scripts/xtensa-build-zephyr.py -p <platform>
```

and the data was extracted from each `build-<platform>/zephyr/` directory:

- Devicetree memory nodes and `chosen` from `zephyr.dts`.
- Linker memory regions from the "Memory Configuration" table in `zephyr.map`.
- Allocated section addresses and sizes from `size -A -x zephyr.elf`.
- Marker symbols (`_image_ram_start`, `__bss_end`, `heapmem`,
  `_unused_ram_start_marker`, `_heap_sentry`, ...) from `nm zephyr.elf`.
- Memory related Kconfig values from `.config`.

To regenerate for one target:

```
./sof/scripts/xtensa-build-zephyr.py -p mtl
B=build-mtl/zephyr
awk '/^Memory Configuration/{f=1;next} /^Linker script and memory map/{f=0} f&&NF' $B/zephyr.map
<cross>-size -A -x $B/zephyr.elf
<cross>-nm $B/zephyr.elf | grep -E ' (_image_ram_start|__bss_end|heapmem|shared_heapmem|_unused_ram_start_marker|_heap_sentry|_image_ram_end|_imr_end)$'
grep -E '^CONFIG_(SRAM_|SOF_ZEPHYR_|SOF_VPAGE_|VIRTUAL_HEAP|LIBRARY_|L3_HEAP|MM_DRV_PAGE_SIZE|HP_SRAM_RESERVE)' $B/.config
```

The ACE vpage region size is not visible in the link: `vpage_init()` computes
it at boot. The values marked "computed" in this document apply the formula
from `sof/zephyr/lib/vpage.c` (see [vpage region](#vpage-region-size)) to the
linked `_unused_ram_start_marker` and the Kconfig values; the firmware logs the
same value at boot as `region <addr> size <size> pages <n>`.

The AMD and MediaTek targets do not build at this Zephyr pin, so their maps
are derived from the generated `zephyr.dts`, `.config`, the SoC linker scripts
and the SOF platform headers, not from a link. imx93 (A55) and native_sim
were not built at all; they are described from board configuration and
Devicetree sources only.

## Conventions

- Diagrams put the highest address at the top.
- `K` = 1024 bytes, `M` = 1024 K. Hex values are bytes.
- "computed" marks a value derived by arithmetic from linked symbols, DTS or
  Kconfig, not read directly from a build artifact.
- File references are relative to the SOF (`sof/`) or Zephyr (`zephyr/`)
  tree at the commits above, as `path:line`. Short forms used throughout:
  `alloc.c` = `sof/zephyr/lib/alloc.c`, `vpage.c` = `sof/zephyr/lib/vpage.c`,
  `adsp_memory.h` = `zephyr/soc/intel/intel_adsp/ace/include/adsp_memory.h`,
  `ace-link-mirrored.ld` =
  `zephyr/soc/intel/intel_adsp/ace/include/linker/ace-link-mirrored.ld`.

## Where regions come from

### Devicetree nodes

| Node (label) | Targets | Meaning | Consumed by |
|---|---|---|---|
| `sram0` | all Xtensa DSPs | Main DSP SRAM. On Intel this is HP/L2 SRAM. `chosen { zephyr,sram = &sram0; }` on every Xtensa board here, which sets `CONFIG_SRAM_BASE_ADDRESS`/`CONFIG_SRAM_SIZE` | Intel: `L2_SRAM_BASE`/`L2_SRAM_SIZE` (`zephyr/soc/intel/intel_adsp/ace/include/adsp_memory.h:14-15`, `zephyr/soc/intel/intel_adsp/cavs/include/cavs25/adsp_memory.h:11-12`) |
| `sram0virtual` | ACE only | Virtual address window used by the TLB driver, 8 M at `0xa0020000` | `L2_VIRTUAL_SRAM_BASE`/`L2_VIRTUAL_SRAM_SIZE` (`adsp_memory.h:17-18`); bounds check in `adsp_add_virtual_memory_region()` (`zephyr/drivers/mm/mm_drv_intel_adsp_regions.c:25-35`) |
| `sram1` | Intel, i.MX, MT8365 | Intel: LP SRAM, 64 K (`LP_SRAM_BASE`/`LP_SRAM_SIZE`, `adsp_memory.h:20-21`), linker region `lpram`. i.MX: second SRAM node, see the i.MX section | Intel linker `lpram`; MT8365 heap (see MediaTek) |
| `IMR1` (token `imr1`) | Intel | IMR (L3) in host DRAM, 16 M, `block-size = <0x1000>` | `L3_MEM_BASE_ADDR`/`L3_MEM_SIZE`/`L3_MEM_PAGE_SIZE` (`adsp_memory.h:37-39`); IMR boot loader macros (`adsp_memory.h:49-73`) |
| `mem_window0..3` | Intel | Host memory windows. `memory = <&sram0>`, window 0 `offset = <0x4000>`, the others follow (`zephyr/soc/intel/intel_adsp/common/include/mem_window.h:15-35`), sizes from `CONFIG_MEMORY_WIN_n_SIZE` | SOF `mailbox_layout.h` (`sof/src/platform/<plat>/include/platform/lib/mailbox_layout.h`) |
| `tlb` (`intel,adsp-mtl-tlb`) | ACE | TLB hardware; table size `TLB_SIZE` (`zephyr/soc/intel/intel_adsp/ace/include/adsp_memory_regions.h:23`) | `zephyr/drivers/mm/mm_drv_intel_adsp_mtl_tlb.c` |
| `dram0`/`dram1`, `sram`, `ddr`, `ocram`... | AMD, MTK, NXP Cortex-M | per SoC, see the target sections | SoC linker scripts |

### SoC linker scripts

| Family | Linker script | `MEMORY` regions | Region origins |
|---|---|---|---|
| Intel ACE | `zephyr/soc/intel/intel_adsp/ace/linker.ld`, which includes `zephyr/soc/intel/intel_adsp/ace/include/linker/ace-link-mirrored.ld` when `CONFIG_INTEL_ADSP_MEMORY_IS_MIRRORED=y` (`linker.ld:7-9`) | `vectors`, `imr`, `ram`, `ucram`, `IDT_LIST`, `lpram`, `noload` (`ace-link-mirrored.ld:81-107`) | `vectors` = `VECBASE_RESET_PADDR_SRAM` = `L2_SRAM_BASE + CONFIG_HP_SRAM_RESERVE`, len `VECTOR_TBL_SIZE` (0x1000); `imr` = `IMR_BOOT_LDR_TEXT_ENTRY_BASE`, len 0x100000; `ram` = `RAM_BASE`/`RAM_SIZE` = rest of `sram0` after the reserve and vectors (`adsp_memory.h:26-33`); `ucram` = `RPO_SET(RAM_BASE, CONFIG_INTEL_ADSP_UNCACHED_REGION)`; `lpram` = `sram1`; `noload` = 0x20000 (rimage manifest data only) |
| Intel cAVS 2.5 | `zephyr/soc/intel/intel_adsp/cavs/include/xtensa-cavs-linker.ld` | `vectors`, `imr`, `RAM`, `ucram`, `IDT_LIST`, `lpram`, `noload` (`xtensa-cavs-linker.ld:58-84`) | same macros from `zephyr/soc/intel/intel_adsp/cavs/include/cavs25/adsp_memory.h:11-27`; `IMR_BOOT_LDR_MANIFEST_BASE` is hard coded 0xB0032000 (`cavs25/adsp_memory.h:49`) |
| NXP i.MX DSP | `zephyr/soc/nxp/imx/{imx8,imx8x,imx8m,imx8ulp}/adsp/linker.ld` | vector sub-regions, `iram_text_start`, `sdram0`, `sdram1` | hard-coded `SDRAM0/1_*` macros in the SoC `memory.h`, see i.MX section |
| NXP Cortex-M7 | `zephyr/soc/nxp/imx/imx8m/m7/linker.ld`, `zephyr/soc/nxp/imx/imx9/imx95/m7/linker.ld` | `FLASH`, `RAM`, ... | Devicetree `chosen` |
| AMD ACP | `zephyr/soc/amd/acp_6_0/adsp/linker.ld`, `zephyr/soc/amd/acp_7_0/linker.ld`, `zephyr/soc/amd/acp_7_x/linker.ld` | vector sub-regions, `iram_text_start`, `sdram0`, `sdram1`/`sram1`, `IDT_LIST` | hard-coded `#define`s (in the script or in `zephyr/soc/amd/acp_6_0/adsp/memory.h`), no Devicetree |
| MediaTek | `zephyr/soc/mediatek/mt8xxx/common/adsp/linker.ld` (per-SoC wrappers include it) | `sram`, `dram`, `IDT_LIST` (`:13-18`) | DT `sram0` and `dram0` (`:8-11`) |
| QEMU dc233c | `zephyr/soc/cdns/dc233c/include/xtensa-dc233c.ld` | `vectors`, `RAM` (+ `vec_helpers` with MMU), `rom0_seg` | see QEMU section |

Intel ACE and cAVS DSP SRAM is visible at two aliases (mirrored memory). The
512 MB region number is bits 31:29 of the address (Xtensa Region Protection
Option). `RPO_SET(addr, reg)` (`ace-link-mirrored.ld:37`) rewrites those bits:

| | `CONFIG_INTEL_ADSP_CACHED_REGION` | `CONFIG_INTEL_ADSP_UNCACHED_REGION` | Example (same byte of L2) |
|---|---|---|---|
| ACE | 5 (`0xa0000000`) | 2 (`0x40000000`) | `0xa0031000` cached = `0x40031000` uncached |
| cAVS 2.5 | 5 (`0xa0000000`) | 4 (`0x80000000`) | `0xbe011000` cached = `0x9e011000` uncached |

Defaults: `zephyr/soc/intel/intel_adsp/ace/Kconfig.defconfig.series:12-16`,
`zephyr/soc/intel/intel_adsp/cavs/Kconfig.defconfig.series:12-16`, option
definitions `zephyr/soc/intel/intel_adsp/Kconfig:155-178`. Zephyr tests and
converts aliases in `zephyr/soc/intel/intel_adsp/common/include/soc_cache.h`
(`sys_cache_is_ptr_cached()` and friends compare `addr >> 29` with these
values). There is no `CONFIG_XTENSA_CACHED_REGION` in this tree; the Intel
symbols are `CONFIG_INTEL_ADSP_(UN)CACHED_REGION`.

With `CONFIG_KERNEL_COHERENCE=y` the linker script puts sections in either
alias: `.text`, `.rodata`, `.cached` are linked at `0xa0...`/`0xbe...`
(`SEGSTART_CACHED`), while `.noinit`, `.data`, `.bss` and `.heap_mem` are
linked at the uncached alias `0x40...`/`0x9e...` (`SEGSTART_UNCACHED`). This
is why the MTL build has `.bss` at `0x400d7000` and `_unused_ram_start_marker`
at `0xa013c000` on MTL: both are addresses of the same physical L2, and the
section addresses in this document are given in the alias they are linked at.
`_heap_sentry` is the uncached alias of the end of `sram0`, `_image_ram_end`
the cached alias (`ace-link-mirrored.ld:466-471`).

### Kconfig options

| Option | Defined in | Default(s) at this tree | Effect |
|---|---|---|---|
| `CONFIG_SOF_ZEPHYR_HEAP_SIZE` | `sof/zephyr/Kconfig:96-102` | 0x40000 if `VIRTUAL_HEAP`; else 0x60000 if `SOF_VREGIONS` (all ACE builds here: 384 K); the per-SoC 0xF0000 (ACE1.5/2.0) and 0x100000 (ACE3.0/4.0) defaults only apply with vregions disabled; else 0 | ACE system heap size: `HEAPMEM_SIZE` (`sof/src/kernel/platform/{meteorlake,lunarlake}/include/platform/lib/memory.h:37`, `{ace30,novalake}/.../memory.h:35`). 0 on non-ACE targets, where the heap is sized differently |
| `CONFIG_SOF_USERSPACE_USE_SHARED_HEAP` | `sof/zephyr/Kconfig:112` | n; y on PTL, selected by `SOF_USERSPACE_PROXY` (`sof/zephyr/Kconfig:219-222`), which its board conf sets | Carves `shared_heapmem` out of `HEAPMEM_SIZE` |
| `CONFIG_SOF_ZEPHYR_SHARED_BUFFER_HEAP_SIZE` | `sof/zephyr/Kconfig:119` | 0x1E000 ACE1.5/2.0, 0x10000 ACE3.0, 0 if not using the shared heap | `SHARED_BUFFER_HEAP_MEM_SIZE` (`sof/zephyr/lib/alloc.c:115`) |
| `CONFIG_SOF_ZEPHYR_HEAP_CACHED` | `sof/zephyr/Kconfig:72` | y on CAVS/ACE | System heap hands out cached aliases for non-shared allocations |
| `CONFIG_VIRTUAL_HEAP` | `sof/zephyr/Kconfig:398` | no default (n); not set by any board conf or overlay in `sof/app`, so off on every ACE build here | Enables the optional virtual memory heap (VMH) |
| `CONFIG_VIRTUAL_HEAP_EXTENDED` | `sof/zephyr/Kconfig:133` | y if `VIRTUAL_HEAP` | Adds 8 x 32 K buffers to the VMH config (`alloc.c:396-411`) |
| `CONFIG_SOF_ZEPHYR_VIRTUAL_HEAP_REGION_SIZE` | `sof/zephyr/Kconfig:143` | 0x140000 if extended, 0x100000 if `VIRTUAL_HEAP`, else 0; 0 on every ACE build here | VMH virtual region size (`alloc.c:421-423`); also offsets the vpage region start (`sof/zephyr/lib/vpage.c:358-359`) |
| `CONFIG_SOF_VREGIONS` | `sof/zephyr/Kconfig:387` | y on ACE | Builds `sof/zephyr/lib/vpage.c` and `sof/zephyr/lib/vregion.c` |
| `CONFIG_SOF_VPAGE_MAX_ALLOCS` | `sof/zephyr/Kconfig:251` | 128 | Max tracked vpage allocations (not a size) |
| `CONFIG_SOF_VPAGE_LLEXT_SIZE` | `sof/zephyr/Kconfig:264-268` | 0 without `LIBRARY_MANAGER`, else 0x40000 (256 K on every ACE build here) | Physical L2 the vpage allocator leaves for LLEXT pages (`vpage.c:390-393`) |
| `CONFIG_LIBRARY_MANAGER` | `sof/src/kernel/library_manager/Kconfig:7` | n (y in the Intel ACE board confs) | Registers the LLEXT library region |
| `CONFIG_LIBRARY_BASE_ADDRESS` | `sof/src/kernel/library_manager/Kconfig:52` | 0; `0xa0688000` in every ACE board conf (e.g. `sof/app/boards/intel_adsp_ace15_mtpm.conf:45`) | Base of the LLEXT virtual region (`sof/src/kernel/library_manager/llext_manager.c:1228-1230`) |
| `CONFIG_LIBRARY_REGION_SIZE` | `sof/src/kernel/library_manager/Kconfig:61` | 0x100000 | Size of that region |
| `CONFIG_L3_HEAP` | `sof/src/kernel/platform/Kconfig:430` | `default y if ACE` in `sof/app/boards/intel_adsp/Kconfig.defconfig:84` | Heap in IMR after the preloaded image (`alloc.c:251-279`) |
| `CONFIG_COLD_STORE_EXECUTE_DRAM` | `sof/Kconfig.sof:25` | y in the ACE board confs | `__cold`/`__cold_rodata` code and data linked to IMR (`.cold`, `.coldrodata`) |
| `CONFIG_MM_DRV_PAGE_SIZE` | `zephyr/drivers/mm/Kconfig:14` | 0x1000 on all MM_DRV targets here | TLB/vpage page size |
| `CONFIG_MM_DRV_INTEL_ADSP_TLB_REMAP_UNUSED_RAM` | `zephyr/drivers/mm/Kconfig:20` | y on ACE builds here | Unmaps L2 after the image at boot (`mm_drv_intel_adsp_mtl_tlb.c:842-871`) |
| `CONFIG_HP_SRAM_RESERVE` | `zephyr/soc/intel/intel_adsp/Kconfig:89` | 65536 | Start of `sram0` kept out of the link; holds the host windows |
| `CONFIG_MEMORY_WIN_{0..3}_SIZE` | `zephyr/soc/intel/intel_adsp/Kconfig:42-66` | 8 K, 8 K, 12 K, 8 K in all Intel builds here | Host window sizes |
| `CONFIG_KERNEL_VM_BASE`/`CONFIG_KERNEL_VM_SIZE` | `zephyr/kernel/Kconfig.vm:16,65`; Intel size default `zephyr/soc/intel/intel_adsp/ace/Kconfig.defconfig.series:66` | `0xa0020000`/`0xfe0000` (ACE), `0xbe000000`/`0xfe0000` (cAVS) | TLB index base and the range unmapped at boot |
| `CONFIG_XTENSA_MMU_NUM_L2_TABLES` | Zephyr arch; ACE3.0/4.0 defaults `zephyr/soc/intel/intel_adsp/ace/Kconfig.defconfig.ace30:27`, `.ace40:29` | PTL 50 (`sof/app/boards/intel_adsp_ace30_ptl.conf:66`), WCL/NVL/NVL-S 64 | Size of `l2_page_tables` in `.bss` on MMU targets |

### C macros and code

| Symbol | Location | Role |
|---|---|---|
| `L2_SRAM_BASE`, `L2_SRAM_SIZE`, `L2_VIRTUAL_SRAM_BASE`, `L2_VIRTUAL_SRAM_SIZE`, `LP_SRAM_BASE`, `RAM_BASE`, `RAM_SIZE`, `VECBASE_RESET_PADDR_SRAM`, `VECTOR_TBL_SIZE` | `zephyr/soc/intel/intel_adsp/ace/include/adsp_memory.h:14-33` | DT derived L2 layout used by the linker script and drivers |
| `L3_MEM_BASE_ADDR`, `IMR_BOOT_LDR_MANIFEST_BASE` (IMR + 0x42000), `IMR_BOOT_LDR_TEXT_ENTRY_BASE` (+0x6000), `IMR_BOOT_LDR_DATA/BSS_*` | `adsp_memory.h:37-73` | IMR layout; `imr` linker region starts at `IMR_BOOT_LDR_TEXT_ENTRY_BASE` = `0xa1048000` |
| `HP_SRAM_WINn_BASE`, `WIN_SIZE(n)`, `WINn_OFFSET` | `zephyr/soc/intel/intel_adsp/common/include/mem_window.h:13-35` | Host windows inside `CONFIG_HP_SRAM_RESERVE` |
| `ADSP_DW_*` | `zephyr/soc/intel/intel_adsp/common/include/adsp_debug_window.h:62-66` | Slot layout of the debug window (window 2) |
| `SRAM_SW_REG_BASE`, `SRAM_OUTBOX_BASE`, `SRAM_INBOX_BASE`, `SRAM_DEBUG_BASE`, `SRAM_EXCEPT_BASE`, `SRAM_STREAM_BASE` | `sof/src/platform/{meteorlake,lunarlake,ace30,novalake}/include/platform/lib/mailbox_layout.h:22-43` (identical apart from the copyright line); TGL `sof/src/platform/tigerlake/include/platform/lib/mailbox_layout.h:21-46` | SOF mailbox layout in windows 0-2; `MAILBOX_*` aliases in `sof/src/platform/intel/ace/include/ace/lib/mailbox.h:21-38` |
| `HEAPMEM_SIZE` | ACE: `sof/src/kernel/platform/meteorlake/include/platform/lib/memory.h:37` (= `CONFIG_SOF_ZEPHYR_HEAP_SIZE`), same in `lunarlake` `:37`, `ace30` `:35`, `novalake` `:35`; i.MX/AMD: sum of `HEAP_*_SIZE` in the platform `memory.h`; generic: `alloc.c:157` | System heap size |
| `heapmem`, `shared_heapmem`, `SHARED_BUFFER_HEAP_MEM_SIZE` | `sof/zephyr/lib/alloc.c:80-159` | Heap backing store per family: `.heap_mem` array (i.MX Xtensa, AMD, ACE), `.bss` array (Arm), host array (POSIX), from a linker end symbol to the end of SRAM (MediaTek, generic) |
| `heap_init()` | `sof/zephyr/lib/alloc.c:1047` (`SYS_INIT` at `:1192`) | `sys_heap_init(&sof_heap.heap, heapmem, HEAPMEM_SIZE - SHARED_BUFFER_HEAP_MEM_SIZE)`, then the shared buffer heap and the L3 heap |
| `calc_l3_heap_start()`, `calc_l3_heap_size()` | `sof/zephyr/lib/alloc.c:251,269` | L3 heap = IMR from the end of the preloaded image (manifest `preload_page_count`) to `L3_MEM_BASE_ADDR + ace_imr_get_mem_size()`, all at run time |
| `virtual_heap_init()` | `sof/zephyr/lib/alloc.c:413` | Only with `CONFIG_VIRTUAL_HEAP` (off on all ACE builds here). Registers the VMH region at `adsp_mm_get_unused_l2_start_aligned()`, size `CONFIG_SOF_ZEPHYR_VIRTUAL_HEAP_REGION_SIZE`, attr `VIRTUAL_REGION_SHARED_HEAP_ATTR` |
| `vpage_init()` | `sof/zephyr/lib/vpage.c:348-459` | Registers the vpage region, attr `VIRTUAL_REGION_VPAGES_ATTR` |
| `llext_memory_region_init()` | `sof/src/kernel/library_manager/llext_manager.c:1223-1235` | Registers `[LIBRARY_BASE_ADDRESS, +LIBRARY_REGION_SIZE)`, attr `VIRTUAL_REGION_LLEXT_LIBRARIES_ATTR` |
| `VIRTUAL_REGION_*_ATTR` | `sof/src/kernel/include/sof/kernel/lib/regions_mm.h:38-42` | 1 = VMH, 2 = LLEXT, 3 = vpages |
| `adsp_add_virtual_memory_region()` | `zephyr/drivers/mm/mm_drv_intel_adsp_regions.c:25` | Rejects regions outside `sram0virtual` or overlapping an existing region |
| `adsp_mm_get_unused_l2_start_aligned()` | `zephyr/drivers/mm/mm_drv_intel_adsp_mtl_tlb.c:65` | `ROUND_UP(&unused_l2_sram_start_marker, CONFIG_MM_DRV_PAGE_SIZE)`; the variable is placed in `.unused_ram_start_marker` (`:52-56`), i.e. at `_unused_ram_start_marker` |
| `sys_mm_drv_mm_init()` | `zephyr/drivers/mm/mm_drv_intel_adsp_mtl_tlb.c:800` | Sizes the physical page pool from `ace_hpsram_get_bank_count()` (hardware register), marks all pages used, then unmaps `[unused_l2_start, KERNEL_VM_BASE + KERNEL_VM_SIZE)`, returning those L2 pages to the pool |

## Intel ACE

Targets: `mtl` (board `intel_adsp/ace15_mtpm`, also used for ARL/ARL-S),
`lnl` (`intel_adsp/ace20_lnl`), `ptl` (`intel_adsp/ace30/ptl`), `wcl`
(`intel_adsp/ace30/wcl`), `nvl` (`intel_adsp/ace40/nvl`), `nvl-s`
(`intel_adsp/ace40/nvls`), see `sof/scripts/xtensa-build-zephyr.py:176-213`.
All six were built and linked. The `*_sim` boards and `ptl-sim`/`wcl-sim`
platforms were not built.

### Common ACE layout

These regions are identical on all six ACE targets (from each `zephyr.dts` and
`zephyr.map`):

| Region | Address range | Size | Defined by |
|---|---|---|---|
| IMR1 (L3, host DRAM) | `0xa1000000`-`0xa1ffffff` | 16 M | DTS `IMR1: memory@a1000000` (`zephyr/dts/xtensa/intel/intel_adsp_ace*.dtsi`) |
| `imr` linker region | `0xa1048000`-`0xa1147fff` | 1 M | `ace-link-mirrored.ld:85`, origin `IMR_BOOT_LDR_TEXT_ENTRY_BASE` (`adsp_memory.h:54`) |
| L2 virtual window | `0xa0020000`-`0xa081ffff` | 8 M | DTS `sram0virtual: virtualmemory@a0020000` |
| L2 physical start | `0xa0020000` | per target | DTS `sram0` (size overridden per SoC, see tables) |
| HP SRAM reserve | `0xa0020000`-`0xa002ffff` | 64 K | `CONFIG_HP_SRAM_RESERVE` |
| Host windows 0-3 | `0xa0024000`-`0xa002cfff` | 8+8+12+8 K | DTS `mem_window0..3`, `mem_window.h`, `CONFIG_MEMORY_WIN_n_SIZE` |
| `vectors` | `0xa0030000`-`0xa0030fff` | 4 K | `VECBASE_RESET_PADDR_SRAM`, `VECTOR_TBL_SIZE` |
| `ram` / `ucram` | from `0xa0031000` / `0x40031000` to end of `sram0` | `sram0` - 68 K | `RAM_BASE`/`RAM_SIZE` |
| LP SRAM (`lpram`) | `0xa0000000`-`0xa000ffff` | 64 K | DTS `sram1: memory@a0000000`; `.lpbuf` (DMA buffers) is empty in all six builds |
| LLEXT library region (virtual) | `0xa0688000`-`0xa0787fff` | 1 M | `CONFIG_LIBRARY_BASE_ADDRESS`, `CONFIG_LIBRARY_REGION_SIZE`, `llext_manager.c:1228` |

SOF mailbox in the windows (computed from `mailbox_layout.h`, `WIN_BASE(n)` =
`sram0` base `0xa0020000`, window offsets from `mem_window.h`):

| Area | Address | Size | Window |
|---|---|---|---|
| `SRAM_SW_REG_BASE` | `0xa0024000` | 4 K | 0 |
| `SRAM_OUTBOX_BASE` (DSP box) | `0xa0025000` | 4 K | 0 |
| `SRAM_INBOX_BASE` (host box) | `0xa0026000` | 8 K (`WIN_SIZE(1)`) | 1 |
| `SRAM_DEBUG_BASE` | `0xa0028000` | 2 K | 2 (debug window, slot layout in `adsp_debug_window.h`) |
| `SRAM_EXCEPT_BASE` | `0xa0028800` | 2 K | 2 |
| `SRAM_STREAM_BASE` | `0xa0029000` | 4 K | 2 |
| window 3 | `0xa002b000` | 8 K | 3 |

`0xa0020000`-`0xa0023fff` and `0xa002d000`-`0xa002ffff` are part of the
reserve but not covered by a window.

The IMR (L3) contents after the link: `.imr`, `.imrdata` and, with
`CONFIG_COLD_STORE_EXECUTE_DRAM=y`, `.cold`/`.coldrodata` (`__cold` code and
rodata) from `0xa1048000` to `_imr_end`. With `CONFIG_L3_HEAP=y` the L3 heap
starts after the preloaded image in IMR and ends at the IMR size reported by
hardware (`ace_imr_get_mem_size()`); both ends are run time values.

### Runtime L2 split after the image

The link fills `sram0` from `0xa0030000` with the vectors, the image
(`.text`, `.rodata`, init/iterable sections, `.cached`, then at the uncached
alias `.noinit`, `.data`, `.bss`) and finally `.heap_mem`, which holds
`shared_heapmem` (only with `CONFIG_SOF_USERSPACE_USE_SHARED_HEAP`) followed
by `heapmem` (`alloc.c:115-123`, linker `ace-link-mirrored.ld:443-450`).
`.unused_ram_start_marker` is then aligned to 4 K (`:452-459`); its address is
what `adsp_mm_get_unused_l2_start_aligned()` returns.

At boot `sys_mm_drv_mm_init()` unmaps everything from that address to
`CONFIG_KERNEL_VM_BASE + CONFIG_KERNEL_VM_SIZE` (`0xa1000000`), so the L2 left
after the image becomes a pool of free physical 4 K pages. Up to three
virtual regions are then registered, all inside `sram0virtual`:

1. VMH, optional: only with `CONFIG_VIRTUAL_HEAP`, which no ACE build here
   enables. It would take `[unused_l2_start, unused_l2_start +
   CONFIG_SOF_ZEPHYR_VIRTUAL_HEAP_REGION_SIZE)`, `alloc.c:421`.
2. vpages: from the end of the VMH region, which without VMH is
   `unused_l2_start` itself; size from the formula below, `vpage.c:401`.
3. LLEXT libraries: `[CONFIG_LIBRARY_BASE_ADDRESS, +CONFIG_LIBRARY_REGION_SIZE)`,
   `llext_manager.c:1228`.

None of these regions is backed at registration. Pages are mapped on demand
by `sys_mm_drv_map_region_safe()`/`sys_mm_drv_map_region()` with physical
address 0, which makes the TLB driver take any free page from the
`L2_PHYS_SRAM_REGION` pool (`mm_drv_intel_adsp_mtl_tlb.c:270`). So vpages,
LLEXT module pages and (if enabled) VMH buffers all compete for the same
physical L2 left after the image. Virtual addresses below the end of `sram0` coincide
numerically with physical L2 addresses but are not identity mapped after boot.

#### vpage region size

`vpage_init()` (`sof/zephyr/lib/vpage.c:348-399`):

```
l2_end    = L2_SRAM_BASE + L2_SRAM_SIZE
start     = adsp_mm_get_unused_l2_start_aligned() + CONFIG_SOF_ZEPHYR_VIRTUAL_HEAP_REGION_SIZE
end       = L2_VIRTUAL_SRAM_BASE + L2_VIRTUAL_SRAM_SIZE
if LIBRARY_MANAGER && LIBRARY_BASE_ADDRESS < end:  end = LIBRARY_BASE_ADDRESS
phys_left = max(l2_end - start, 0)
phys_left = max(phys_left - CONFIG_SOF_VPAGE_LLEXT_SIZE, 0)
size      = min(end - start, phys_left)      # fails with -ENOMEM below one page
```

On every ACE target here the physical term is the smaller one, so the vpage
region ends at `l2_end - CONFIG_SOF_VPAGE_LLEXT_SIZE`. The physical page
budget after the image is therefore:

```
L2 after image = VMH region size + vpage region size + SOF_VPAGE_LLEXT_SIZE
```

With VMH off on every ACE build here this is `vpage region size + 256 K`.
It is an accounting identity on page counts, not a guarantee: the vpage
region and the LLEXT region can each map up to their virtual size, and they
draw from one physical pool.

### Per-target diagrams

All six ACE targets have the same configuration at this tree: a 384 K system
heap (`CONFIG_SOF_ZEPHYR_HEAP_SIZE=0x60000`), no VMH
(`CONFIG_SOF_ZEPHYR_VIRTUAL_HEAP_REGION_SIZE=0x0`), a 256 K LLEXT size
(`CONFIG_SOF_VPAGE_LLEXT_SIZE=0x40000`) and the 1 M LLEXT library region at
`0xa0688000`. They differ only in the `sram0` size and the image size, so they
share one shape. The diagram below has the `mtl` addresses; each target has
its own table; `ptl` (shared buffer heap) and `nvl-s` (smallest L2) also get
a diagram of the lower part of the map.

### mtl (ACE 1.5, also ARL)

```
                 virtual / cached alias view of L2           defined by
 0xa0820000 +-------------------------------------------+ sram0virtual end
            | unassigned virtual space                  |
 0xa0788000 +-------------------------------------------+
            | LLEXT library region, 1 M (virtual)       | LIBRARY_BASE_ADDRESS,
 0xa0688000 +-------------------------------------------+ LIBRARY_REGION_SIZE
            | unassigned virtual space                  |
 0xa02e0000 +===========================================+ sram0 end (2816 K)
            | unassigned VA; 256 K of physical pages    | SOF_VPAGE_LLEXT_SIZE
            | are left for LLEXT (budget, not a range)  |
 0xa02a0000 +-------------------------------------------+
            | vpage region, 356 pages = 1424 K          | vpage_init() (computed)
 0xa013c000 +-------------------------------------------+ _unused_ram_start_marker
            | heapmem, 384 K  (system heap)             | SOF_ZEPHYR_HEAP_SIZE=0x60000
 0xa00dc000 |   (0xb40 alignment pad before heapmem)    |
 0xa00db4c0 +-------------------------------------------+ .heap_mem / __bss_end
            | .bss, .cached, .data, .noinit, ...        | linker (ucram/ram)
            | iterable sections, .rodata, .text         |
 0xa0031000 +-------------------------------------------+ RAM_BASE
            | vectors 4 K                               | VECBASE_RESET_PADDR_SRAM
 0xa0030000 +-------------------------------------------+
            | HP_SRAM_RESERVE 64 K, host windows 0-3    | CONFIG_HP_SRAM_RESERVE
 0xa0020000 +===========================================+ sram0 base
            | (gap)                                     |
 0xa0010000 +-------------------------------------------+
            | LP SRAM 64 K (lpram)                      | sram1
 0xa0000000 +-------------------------------------------+
```

| Item | Value | Source |
|---|---|---|
| `sram0` | `0xa0020000`, 0x2c0000 (2816 K) | `intel_adsp_ace15_mtpm.dtsi:780` |
| `ram` region | `0xa0031000`, 0x2af000 | zephyr.map |
| `.text` | `0xa0031000`, 0x60bc0 | size -A |
| `.rodata` | `0xa0092000`, 0x20e38 | size -A |
| `.cached` | `0xa00c6d40`, 0xf8e0 | size -A |
| `.data` | `0x400ba200`, 0xc1f6 | size -A |
| `.bss` | `0x400d7000`-`0x400db4c0`, 0x44c0 | size -A |
| Image (vectors to `__bss_end`) | 0xab4c0 (685.2 K) | computed |
| `.heap_mem` | `0x400db4c0`, 0x60b40; `heapmem` at `0x400dc000`, 0x60000 | size -A, nm |
| `_unused_ram_start_marker` | `0xa013c000` | nm |
| L2 after image | 0x1a4000 (1680 K) | computed |
| VMH | none | .config |
| vpages | `0xa013c000`-`0xa02a0000`, 0x164000, 356 pages | computed; 356 also in the boot log on ARL-S |
| `SOF_VPAGE_LLEXT_SIZE` | 0x40000 (256 K) | .config |
| IMR `.imr` .. `_imr_end` | `0xa1048000`-`0xa10b0890` (`.cold` 0x11bda, `.coldrodata` 0x54890) | size -A, nm |

### lnl (ACE 2.0)

Same shape as the `mtl` diagram above.

| Item | Value | Source |
|---|---|---|
| `sram0` | `0xa0020000`, 0x2c0000 (2816 K) | `intel_adsp_ace20_lnl.dtsi:684` |
| `ram` region | `0xa0031000`, 0x2af000 | zephyr.map |
| `.text` | `0xa0031000`, 0x59754 | size -A |
| `.rodata` | `0xa008b000`, 0x20268 | size -A |
| `.cached` | `0xa00b4600`, 0x170d0 | size -A |
| `.data` | `0x400b21c0`, 0x1ace | size -A |
| `.bss` | `0x400cc000`-`0x400d0a08`, 0x4a08 | size -A, nm |
| Image (vectors to `__bss_end`) | 0xa0a08 (642.5 K) | computed |
| `.heap_mem` | `0x400d0a40`, 0x605c0; `heapmem` at `0x400d1000`, 0x60000 | size -A, nm |
| `_unused_ram_start_marker` | `0xa0131000` | nm |
| L2 after image | 0x1af000 (1724 K) | computed |
| VMH | none | .config |
| vpages | `0xa0131000`-`0xa02a0000`, 0x16f000, 367 pages | computed |
| `SOF_VPAGE_LLEXT_SIZE` | 0x40000 (256 K) | .config |
| IMR | `0xa1048000`-`0xa10b0890` (`.cold` 0x116ee, `.coldrodata` 0x54890) | size -A, nm |

### ptl (ACE 3.0)

Same shape as `mtl`; PTL also has the shared buffer heap at the start of
`.heap_mem`. Lower part of the map:

```
 0xa04a0000 +===========================================+ sram0 end (4608 K)
            | unassigned VA; 256 K physical for LLEXT   | SOF_VPAGE_LLEXT_SIZE
 0xa0460000 +-------------------------------------------+
            | vpage region, 739 pages = 2956 K          | vpage_init() (computed,
            |                                           |   and boot log)
 0xa017d000 +-------------------------------------------+ _unused_ram_start_marker
            | heapmem, 320 K (system heap)              | SOF_ZEPHYR_HEAP_SIZE - shared
 0xa012d000 +-------------------------------------------+
            | shared_heapmem, 64 K                      | SOF_ZEPHYR_SHARED_BUFFER_HEAP_SIZE
 0xa011d000 +-------------------------------------------+ .heap_mem / __bss_end
            | .bss (244 K, incl. 200 K l2_page_tables)  |
            | .cached, app_smem, .data, .noinit, ...    |
            | .rodata, .text                            |
 0xa0031000 +-------------------------------------------+ RAM_BASE
            | vectors, HP_SRAM_RESERVE (windows)        |
 0xa0020000 +===========================================+ sram0 base
```

| Item | Value | Source |
|---|---|---|
| `sram0` | `0xa0020000`, 0x480000 (4608 K) | `intel_adsp_ace30_ptl.dtsi:28` (overrides `intel_adsp_ace30.dtsi`) |
| `ram` region | `0xa0031000`, 0x46f000 | zephyr.map |
| `.text` | `0xa0031000`, 0x60000 | size -A |
| `.rodata` | `0xa0091000`, 0x1e000 | size -A |
| `.cached` | `0xa00bf000`, 0x21000 | size -A |
| `.data` | `0x400b7000`, 0x3000 | size -A |
| `.bss` | `0x400e0000`-`0x4011d000`, 0x3d000; contains `l2_page_tables` 0x32000 (`CONFIG_XTENSA_MMU_NUM_L2_TABLES=50`) | size -A, nm |
| Image (vectors to `__bss_end`) | 0xed000 (948 K) | computed |
| `.heap_mem` | `0x4011d000`, 0x60000: `shared_heapmem` 0x10000, `heapmem` at `0x4012d000`, 0x50000 | size -A, nm |
| `_unused_ram_start_marker` | `0xa017d000` | nm |
| L2 after image | 0x323000 (3212 K) | computed |
| VMH | none | .config |
| vpages | `0xa017d000`-`0xa0460000`, 0x2e3000, 739 pages | computed; boot log on PTL: `region 0xa017d000 size 0x2e3000 pages 739` |
| `SOF_VPAGE_LLEXT_SIZE` | 0x40000 (256 K) | .config |
| IMR | `0xa1048000`-`0xa1062000` (`.cold` 0x17806, no `.coldrodata`) | size -A, nm |

PTL is the only ACE build here with `CONFIG_USERSPACE=y`. Its board conf sets
`CONFIG_SOF_USERSPACE_PROXY=y`, which selects
`CONFIG_SOF_USERSPACE_USE_SHARED_HEAP` (`sof/zephyr/Kconfig:219-222`); the
64 K shared buffer heap is carved out of the 384 K `HEAPMEM_SIZE`.

### wcl (ACE 3.0)

Same shape as the `mtl` diagram.

| Item | Value | Source |
|---|---|---|
| `sram0` | `0xa0020000`, 0x360000 (3456 K) | `intel_adsp_ace30_wcl.dtsi:12` |
| `ram` region | `0xa0031000`, 0x34f000 | zephyr.map |
| `.text` | `0xa0031000`, 0x43000 | size -A |
| `.rodata` | `0xa0074000`, 0x1a000 | size -A |
| `.cached` | `0xa009a000`, 0x100d0 | size -A |
| `.data` | `0x40096000`, 0x3000 | size -A |
| `.bss` | `0x400ab000`-`0x400f1000`, 0x46000; contains `l2_page_tables` 0x40000 (64 tables) | size -A, nm |
| Image (vectors to `__bss_end`) | 0xc1000 (772 K) | computed |
| `.heap_mem` | `0x400f1000`, 0x60000 (`heapmem`) | size -A, nm |
| `_unused_ram_start_marker` | `0xa0151000` | nm |
| L2 after image | 0x22f000 (2236 K) | computed |
| VMH | none | .config |
| vpages | `0xa0151000`-`0xa0340000`, 0x1ef000, 495 pages | computed |
| `SOF_VPAGE_LLEXT_SIZE` | 0x40000 (256 K) | .config |
| IMR | `0xa1048000`-`0xa105b000` (`.cold` 0x10ffa, no `.coldrodata`) | size -A, nm |

### nvl (ACE 4.0)

Same shape as the `mtl` diagram.

| Item | Value | Source |
|---|---|---|
| `sram0` | `0xa0020000`, 0x480000 (4608 K) | `intel_adsp_ace40_nvl.dtsi:28` |
| `ram` region | `0xa0031000`, 0x46f000 | zephyr.map |
| `.text` | `0xa0031000`, 0x43000 | size -A |
| `.rodata` | `0xa0074000`, 0x16000 | size -A |
| `.cached` | `0xa0096000`, 0x13c88 | size -A |
| `.data` | `0x40092000`, 0x3000 | size -A |
| `.bss` | `0x400aa000`-`0x400ef000`, 0x45000; `l2_page_tables` 0x40000 | size -A, nm |
| Image (vectors to `__bss_end`) | 0xbf000 (764 K) | computed |
| `.heap_mem` | `0x400ef000`, 0x60000 (`heapmem`) | size -A, nm |
| `_unused_ram_start_marker` | `0xa014f000` | nm |
| L2 after image | 0x351000 (3396 K) | computed |
| VMH | none | .config |
| vpages | `0xa014f000`-`0xa0460000`, 0x311000, 785 pages | computed |
| `SOF_VPAGE_LLEXT_SIZE` | 0x40000 (256 K) | .config |
| IMR | `0xa1048000`-`0xa10ab600` (`.cold` 0x110d2, `.coldrodata` 0x4f600) | size -A, nm |

### nvl-s (ACE 4.0)

Same shape as `mtl`, with the smallest L2 of the ACE targets:

```
 0xa0220000 +===========================================+ sram0 end (2048 K)
            | unassigned VA; 256 K physical for LLEXT   | SOF_VPAGE_LLEXT_SIZE
 0xa01e0000 +-------------------------------------------+
            | vpage region, 153 pages = 612 K           | vpage_init() (computed)
 0xa0147000 +-------------------------------------------+ _unused_ram_start_marker
            | heapmem, 384 K (system heap)              | SOF_ZEPHYR_HEAP_SIZE=0x60000
 0xa00e7000 +-------------------------------------------+ __bss_end
            | image (.bss incl. 256 K l2_page_tables)   |
 0xa0030000 +-------------------------------------------+
            | HP_SRAM_RESERVE                           |
 0xa0020000 +===========================================+
```

| Item | Value | Source |
|---|---|---|
| `sram0` | `0xa0020000`, 0x200000 (2048 K) | `intel_adsp_ace40_nvls.dtsi:12` |
| `ram` region | `0xa0031000`, 0x1ef000 | zephyr.map |
| `.text` | `0xa0031000`, 0x43000 | size -A |
| `.rodata` | `0xa0074000`, 0x16000 | size -A |
| `.cached` | `0xa0096000`, 0xc488 | size -A |
| `.data` | `0x40092000`, 0x3000 | size -A |
| `.bss` | `0x400a3000`-`0x400e7000`, 0x44000; `l2_page_tables` 0x40000 | size -A, nm |
| Image (vectors to `__bss_end`) | 0xb7000 (732 K) | computed |
| `.heap_mem` | `0x400e7000`, 0x60000 (`heapmem`) | size -A, nm |
| `_unused_ram_start_marker` | `0xa0147000` | nm |
| L2 after image | 0xd9000 (868 K) | computed |
| VMH | none | .config |
| vpages | `0xa0147000`-`0xa01e0000`, 0x99000, 153 pages | computed |
| `SOF_VPAGE_LLEXT_SIZE` | 0x40000 (256 K) | .config |
| IMR | `0xa1048000`-`0xa10ab600` (`.cold` 0x110ca, `.coldrodata` 0x4f600) | size -A, nm |

## Intel cAVS 2.5

Targets: `tgl` (board `intel_adsp/cavs25`, aliases `adl`, `adl-n`, `rpl`) and
`tgl-h` (`intel_adsp/cavs25/tgph`, aliases `adl-s`, `rpl-s`),
`sof/scripts/xtensa-build-zephyr.py:160-175`. Both built and
linked. cAVS has no VMH, no vregions/vpages (`SOF_VREGIONS` depends on `ACE`)
and no library manager (`CONFIG_LIBRARY_BASE_ADDRESS=0`). The TLB node is
`intel,adsp-tlb` (`CONFIG_DT_HAS_INTEL_ADSP_TLB_ENABLED`), not the MTL TLB
driver.

cAVS 2.5 cannot use the ACE vpage/LLEXT scheme yet, independent of SOF
configuration:

- Its driver, `zephyr/drivers/mm/mm_drv_intel_adsp_tlb.c`
  (`CONFIG_MM_DRV_INTEL_ADSP_TLB`), only maps physical pages that the caller
  passes in. `sys_mm_drv_map_page()` rejects a physical address outside L2
  (`:95-99`), and `sys_mm_drv_map_region()` (`:144-150`) maps a contiguous
  physical range through the generic helper. There is no free physical page
  allocator: physical address 0 fails that bounds check, where the MTL driver
  would take a free page (`mm_drv_intel_adsp_mtl_tlb.c:269-275`).
- There is no virtual region API. `adsp_add_virtual_memory_region()` and the
  real `sys_mm_drv_query_memory_regions()` are in
  `zephyr/drivers/mm/mm_drv_intel_adsp_regions.c`, which is only built with
  `CONFIG_MM_DRV_INTEL_ADSP_MTL_TLB` (`zephyr/drivers/mm/CMakeLists.txt:13-17`).
  On cAVS only the weak generic `sys_mm_drv_query_memory_regions()` is
  linked, which returns an empty list (`zephyr/drivers/mm/mm_drv_common.c:570-581`).
- The cAVS 2.5 Devicetree (`intel_adsp_cavs25.dtsi`,
  `intel_adsp_cavs25_tgph.dtsi`) has no `sram0virtual` node, so there is no
  virtual window beyond physical L2 to place vpages or a library region in.

`CONFIG_SOF_ZEPHYR_HEAP_SIZE=0` and neither `CONFIG_ACE` nor `CONFIG_IMX` is
set, so the system heap takes the generic branch of `alloc.c:153-157`:
`heapmem = ALIGN_UP(_end, PLATFORM_DCACHE_ALIGN)` (64, from
`CONFIG_DCACHE_LINE_SIZE` via `sof/zephyr/include/rtos/cache.h:45`) and
`HEAPMEM_SIZE = _heap_sentry - heapmem`, i.e. all of `sram0` after `.bss`.
`_end` and `_heap_sentry` are set in `xtensa-cavs-linker.ld:284-288`.

Both targets have the same shape; diagram with `tgl` addresses:

```
 0xbe810000 +-------------------------------------------+
            | LP SRAM 64 K (lpram, .lpbuf DMA buffers)  | sram1
 0xbe800000 +-------------------------------------------+ _image_ram_end
            | (gap)                                     |
 0xbe2e0000 +===========================================+ sram0 end (2944 K), _heap_sentry (uc)
            | system heap, 0x218a00 (2146.5 K)          | alloc.c generic branch
 0xbe0c7600 +-------------------------------------------+ _end (uc 0x9e0c7600)
            | .bss, .cached, .data, .noinit (uc 0x9e..) |
            | .fw_ready, iterable sections, .rodata,    |
            | .text                                     |
 0xbe011000 +-------------------------------------------+ RAM_BASE
            | vectors 4 K                               |
 0xbe010000 +-------------------------------------------+
            | HP_SRAM_RESERVE 64 K, host windows 0-3    |
 0xbe000000 +===========================================+ sram0 base

 0xb1000000 +-------------------------------------------+
            | IMR1 16 M (host DRAM); imr linker         | IMR1: memory@b0000000
            | region 0xb0038000 +1 M holds .imr only    |
 0xb0000000 +-------------------------------------------+
```

SOF mailbox on TGL comes from fixed offsets in
`sof/src/platform/tigerlake/include/platform/lib/mailbox_layout.h:21-46`
(`HP_SRAM_BASE` 0xBE000000): SW registers `0xbe004000` 4 K, outbox
`0xbe005000` 4 K, inbox `0xbe006000` 8 K, debug `0xbe008000` 2 K, exception
`0xbe008800` 2 K, stream `0xbe009000` 4 K, trace size 0 (computed). The
Zephyr windows (`intel_adsp_cavs25.dtsi:237-263`, window 0 `offset = <0x4000>`)
cover the same range. The rest of
`sof/src/kernel/platform/tigerlake/include/platform/lib/memory.h` describes the
XTOS-era heap layout and does not drive the Zephyr link.

| Item | tgl | tgl-h | Source |
|---|---|---|---|
| `sram0` | `0xbe000000`, 0x2e0000 (2944 K) | `0xbe000000`, 0x1e0000 (1920 K) | `intel_adsp_cavs25.dtsi:570`, `intel_adsp_cavs25_tgph.dtsi:410` |
| `sram1` (LP) | `0xbe800000`, 0x10000 | same | `.dtsi:576` / `:416` |
| `IMR1` | `0xb0000000`, 16 M | same | `.dtsi:563` / `:403` |
| `RAM` region | `0xbe011000`, 0x2cf000 | `0xbe011000`, 0x1cf000 | zephyr.map |
| `ucram` | `0x9e011000` | `0x9e011000` | zephyr.map |
| `.text` | `0xbe011000`, 0x57623 | `0xbe011000`, 0x56737 | size -A |
| `.rodata` | `0xbe069000`, 0x3431c | `0xbe068000`, 0x34188 | size -A |
| `.data` | `0x9e0a0dc0`, 0xf7d0 | `0x9e09f940`, 0xe980 | size -A |
| `.cached` | `0xbe0b0f80`, 0x13488 | `0xbe0aec00`, 0xbc88 | size -A |
| `.bss` | `0x9e0c5000`, 0x25c8 | `0x9e0bb000`, 0x1f08 | size -A |
| `_end` | `0x9e0c7600` | `0x9e0bcf40` | nm |
| `_heap_sentry` | `0x9e2e0000` | `0x9e1e0000` | nm |
| Image (vectors to `__bss_end`) | 0xb75c8 (733.4 K) | 0xacf08 (691.8 K) | computed |
| System heap | 0x218a00 (2146.5 K) | 0x1230c0 (1164.2 K) | computed |
| `.imr` | `0xb0038000`, 0x777 | `0xb0038000`, 0x733 | size -A |

`_image_ram_end` is `0xbe800000` on both: the cAVS linker script defines it
inside `.lpbuf` in `lpram` (`xtensa-cavs-linker.ld:291-295`), not at the end
of `sram0` as on ACE.

## NXP i.MX

### imx8, imx8x, imx8m, imx8ulp (Xtensa HiFi4 DSP)

Boards: `imx8qm_mek/mimx8qm6/adsp`, `imx8qxp_mek/mimx8qx6/adsp`,
`imx8mp_evk/mimx8ml8/adsp`, `imx8ulp_evk/mimx8ud7/adsp`. All four built and
linked. imx8x uses the SOF `imx8` platform directory
(`sof/zephyr/CMakeLists.txt:321,337`); on the Zephyr side it has its own SoC
directory whose `linker.ld` and `memory.h` match imx8 apart from one blank line.

The DSP linker scripts (`zephyr/soc/nxp/imx/<soc>/adsp/linker.ld`) do not use
Devicetree for RAM. `MEMORY` (`linker.ld:30-107`) has the vector sub-regions
and `iram_text_start` in IRAM, then:

- `sdram0` (`:86-88`): `SDRAM0_BASE`, `SDRAM0_SIZE`
- `sdram1` (`:89-91`): `SDRAM1_BASE + SOF_MAILBOX_SIZE`, `SDRAM1_SIZE - SOF_MAILBOX_SIZE`

with the base/size macros hard coded in the Zephyr SoC `memory.h`
(`zephyr/soc/nxp/imx/imx8/adsp/memory.h:15-19`: SDRAM0 `0x92400000`, SDRAM1
`0x92C00000`, both 8 M; imx8ulp `zephyr/soc/nxp/imx/imx8ulp/adsp/memory.h:14-18`:
`0x1a000000`/`0x1a800000`). The DTS `sram0`/`sram1` nodes (512 K each,
`zephyr/dts/xtensa/nxp/nxp_imx8.dtsi:185,191`, `nxp_imx8m.dtsi:172,178`,
`nxp_imx8ulp.dtsi:37,43`) only set `CONFIG_SRAM_BASE_ADDRESS`/`CONFIG_SRAM_SIZE`.
The `sdram1` region is listed in `zephyr.map`.

Everything except the vectors and `.iram.text` goes to `sdram0` (`.rodata` first, then
`.text`, `.fw_ready`, `.noinit`, `.data`, `.bss`). The SOF heap takes the
`CONFIG_IMX` + `CONFIG_XTENSA` branch of `alloc.c:80-89`: `heapmem[HEAPMEM_SIZE]`
in `.heap_mem`, which the linker puts in `sdram1` (imx8 `linker.ld:445-451`).
`HEAPMEM_SIZE = HEAP_SYSTEM_SIZE + HEAP_RUNTIME_SIZE + HEAP_BUFFER_SIZE` from
`sof/src/kernel/platform/<imx8|imx8m|imx8ulp>/include/platform/lib/memory.h`
(imx8: `:99`, `:109-114`, `:117-119`), where `HEAP_BUFFER_SIZE` is whatever is
left of SDRAM1 after the mailbox, the other heaps, the stack and the unused
XTOS system-runtime heap. The resulting sizes match the linked `.heap_mem`.

The SOF mailbox is at the start of SDRAM1 and is excluded from the `sdram1`
linker region: `SDRAM1_BASE` in
`sof/src/platform/<imx8|imx8m>/include/platform/lib/mailbox_layout.h:18`
(imx8ulp `:20`), outbox 4 K, inbox 4 K, debug 2 K (imx8ulp 10 K), exception
2 K, stream 4 K, trace 4 K, total 0x5000 (imx8ulp 0x7000, computed).

Diagram for `imx8` (imx8x identical; imx8m identical apart from IRAM at
`0x3b6f8000`):

```
 0x93400000 +-------------------------------------------+ SDRAM1 end, __stack (linker.ld:457)
            | 0x7000: stack + unused sys-runtime space  |
 0x933f9000 +-------------------------------------------+ _end
            | .heap_mem: heapmem 0x7f4000 (8144 K)      | HEAP_*_SIZE, alloc.c:89
 0x92c05000 +-------------------------------------------+ sdram1 origin
            | SOF mailbox 0x5000                        | mailbox_layout.h
 0x92c00000 +===========================================+ SDRAM1_BASE, SDRAM0 end
            | unused part of sdram0                     |
 0x92469518 +-------------------------------------------+ end of .bss
            | .bss, .data, .noinit, .fw_ready, .text,   | sdram0
            | .rodata                                   |
 0x92400000 +===========================================+ SDRAM0_BASE
     ...
 0x596f8800 +-------------------------------------------+
            | IRAM 2 K: reset/window/interrupt vectors, | vector_* regions,
            | .iram.text                                | iram_text_start
 0x596f8000 +-------------------------------------------+ IRAM_BASE
```

| Item | imx8 | imx8x | imx8m | imx8ulp | Source |
|---|---|---|---|---|---|
| DTS `sram0` / `sram1` | `0x92400000` / `0x92c00000`, 512 K each | same | same | `0x1a000000` / `0x1a800000`, 512 K each | dtsi |
| `sdram0` region | `0x92400000`, 0x800000 | same | same | `0x1a000000`, 0x800000 | zephyr.map |
| `sdram1` region | `0x92c05000`, 0x7fb000 | same | same | `0x1a807000`, 0x7f9000 | zephyr.map |
| IRAM vectors | `0x596f8000` | `0x596f8000` | `0x3b6f8000` | `0x21170000` (`iram_text_start` 0xf9a8) | zephyr.map |
| `.rodata` | `0x92400000`, 0x2ac0c | `0x92400000`, 0x2b08c | `0x92400000`, 0x2a9b4 | `0x1a000000`, 0x2adc8 | size -A |
| `.text` | `0x9242ac0c`, 0x2ebe4 | `0x9242b08c`, 0x30e7f | `0x9242a9b4`, 0x2f4a3 | `0x1a02adc8`, 0x30dd7 | size -A |
| `.data` | `0x92462258`, 0x1da8 | `0x924649d8`, 0x1628 | `0x924627c0`, 0x4840 | `0x1a063bf8`, 0x1408 | size -A |
| `.bss` | `0x92467c80`, 0x1898 | `0x92469c80`, 0x18d0 | `0x92468900`, 0x1a10 | `0x1a065180`, 0x18d0 | size -A |
| Image in sdram0 | 0x69518 (421.3 K) | 0x6b550 (429.3 K) | 0x6a310 (424.8 K) | 0x66a50 (410.6 K) | computed |
| `.heap_mem` | `0x92c05000`, 0x7f4000 | same | same | `0x1a807000`, 0x7f2000 | size -A |
| `_end` | `0x933f9000` | `0x933f9000` | `0x933f9000` (= `_heap_sentry`) | `0x1aff9000` | nm |
| SOF mailbox | `0x92c00000`, 0x5000 | same | same | `0x1a800000`, 0x7000 | computed |
| MU (DTS) | `mailbox0@5d310000` | same | `mailbox0@30e70000` | `mu3@2da20000` | dtsi |

### imx8m_cm7 (Cortex-M7, DDR)

Board `imx8mp_evk/mimx8ml8/m7/ddr`, built and linked. The SoC script
`zephyr/soc/nxp/imx/imx8m/m7/linker.ld` adds a `DDR` region (`:13`,
`0x80400000`, 12 M with `CONFIG_CODE_DDR`) and includes the generic Cortex-M
script, whose `FLASH` and `RAM` regions come from `chosen { zephyr,flash =
&ddr_code; zephyr,sram = &ddr_sys; }`
(`zephyr/boards/nxp/imx8mp_evk/imx8mp_evk_mimx8ml8_m7_ddr.dts:18-19`).
`ROMSTART_REGION` at 0 comes from `CONFIG_ROMSTART_RELOCATION_ROM=y`
(`sof/app/boards/imx8mp_evk_mimx8ml8_m7_ddr.conf:8`). The heap takes the
non-Xtensa i.MX branch: `heapmem[HEAPMEM_SIZE]` in `.bss` (`alloc.c:102`),
`HEAPMEM_SIZE` 0x10000 (`sof/src/kernel/platform/imx8m_cm7/include/platform/lib/memory.h:19`).

```
 0x82003000 +-------------------------------------------+
            | SOF mailbox: dspbox, hostbox, stream 4 K  | sof/src/platform/imx8m_cm7/
 0x82000000 +-------------------------------------------+   .../mailbox.h:29-41 (outside all
            |                                           |   linker regions)
 0x81000000 +-------------------------------------------+
            | DDR region 12 M (no sections)             | soc linker.ld:13
 0x80400000 +===========================================+
            | RAM (ddr_sys) 2 M: nocache, datas,        | chosen zephyr,sram
            | .bss (incl. heapmem 64 K), noinit         |
 0x80200000 +===========================================+
            | FLASH (ddr_code) 2 M: text, rodata        | chosen zephyr,flash
 0x80000000 +===========================================+
            | ROMSTART_REGION 1 K                       |
 0x00000000 +-------------------------------------------+
```

| Item | Value | Source |
|---|---|---|
| `FLASH` / `RAM` / `DDR` regions | `0x80000000` 0x200000 / `0x80200000` 0x200000 / `0x80400000` 0xc00000 | zephyr.map |
| `text` | `0x80000000`, 0x2c420 | size -A |
| `rodata` | `0x8002d028`, 0x24934 | size -A |
| `.last_section` (end of flash image) | `0x800554a4` | size -A |
| `nocache` | `0x80200000`, 0x1a80 (+ pad 0x580) | size -A |
| `datas` | `0x80202000`, 0x26cc | size -A |
| `bss` | `0x80205b60`, 0x11ab5; `heapmem` `0x80205ba0`, 0x10000 | size -A, zephyr.map |
| `noinit` | `0x80217618`, 0x7188 | size -A |
| RAM used to `_end` | `0x8021e7a0`, 0x1e7a0 (121.9 K) | nm, computed |
| MU | `mailbox0@30ab0000` (= `MU_BASE`, `memory.h:22`) | dtsi |

### imx95 (Cortex-M7, DDR)

Board `imx95_evk/mimx9596/m7/ddr`, built and linked.
`zephyr/soc/nxp/imx/imx9/imx95/m7/linker.ld` has no `MEMORY` block; the
generic Cortex-M script uses `chosen { zephyr,sram = &ddr; }` with
`zephyr,flash` deleted
(`zephyr/boards/nxp/imx95_evk/imx95_evk_mimx9596_m7_ddr.dts:15-20`), so code
and data share `RAM` and `FLASH` has length 0. `ITCM` (`0x0`, 256 K) comes
from the `itcm` node's `zephyr,memory-region` and holds no sections. Heap as
imx8m_cm7: 64 K `heapmem` in `.bss`
(`sof/src/kernel/platform/imx95/include/platform/lib/memory.h:19`).

```
 0x86003000 +-------------------------------------------+
            | SOF mailbox: hostbox, dspbox, stream 4 K  | sof/src/platform/imx95/.../mailbox.h:29-41
 0x86000000 +-------------------------------------------+   (outside RAM)
            |                                           |
 0x82a00000 +===========================================+ RAM end (42 M)
            | free                                      |
 0x8009b188 +-------------------------------------------+ _end
            | noinit, bss (incl. heapmem 64 K), datas   |
 0x80080000 +-------------------------------------------+ _image_ram_start
            | padding_section                           |
            | rodata, text, rom_start                   |
 0x80000000 +===========================================+ ddr: memory@80000000
     ...
 0x00040000 +-------------------------------------------+
            | ITCM 256 K (no sections)                  |
 0x00000000 +-------------------------------------------+
```

| Item | Value | Source |
|---|---|---|
| `RAM` region | `0x80000000`, 0x2a00000 | zephyr.map, DTS `ddr` |
| `rom_start` + `text` + `rodata` | `0x80000000`-`0x8005b330` (364.8 K) | size -A, computed |
| `padding_section` | `0x8005b330`, 0x24cd0 | size -A |
| `datas` | `0x80080000`, 0x680 | size -A |
| `bss` | `0x800825a0`, 0x11a59; `heapmem` `0x800825e0`, 0x10000 | size -A, zephyr.map |
| `noinit` | `0x80094000`, 0x7188 | size -A |
| `_end` | `0x8009b188` | nm |
| SCMI shared memory | `scmi_shmem0: memory@44611000`, 0x80 | `zephyr/dts/arm/nxp/imx/nxp_imx95_m7.dtsi:67-69` |
| MU | `mu7: mailbox@42440000` (= `MU_BASE`) | dtsi |

## AMD ACP (derived, not linked)

Targets `acp_6_0` (board `acp_6_0_adsp/acp_6_0`, SOF platform directory
`amd/rembrandt`), `acp_7_0` (`acp_7_0_adsp/acp_7_0`) and `acp_7_x`
(`acp_7_x_adsp/acp_7_x`). None of them builds at this Zephyr pin:

- `acp_6_0`: compile errors in `sof/src/kernel/ipc/ipc3/dai.c`
  (`'struct dai' has no member named 'dev'`).
- `acp_7_0`: `sof/zephyr/lib/dma.c` references the missing Devicetree node
  `acp_dmic_dma` (`__device_dts_ord_DT_N_NODELABEL_acp_dmic_dma_ORD undeclared`).
- `acp_7_x`: Kconfig aborts on undefined symbols set in
  `sof/app/boards/acp_7_x_adsp.conf` (`DMA_AMD_ACP_DMIC`, `DAI_AMD_DMIC`,
  `AMD_ACP_HOST_DMA_PROBE`); no `.config` is produced.

Everything below comes from the generated `zephyr.dts` and `.config` (where
present), the SoC linker scripts and SOF headers. The DTS has no memory nodes
and an empty `chosen`, so `CONFIG_SRAM_SIZE=0`, and all region addresses are
hard-coded `#define`s. On acp_6_0 and acp_7_0 the linker scripts place `.text`
in IRAM (`iram_text_start`), data in `sdram0`, and `heapmem` (`.heap_mem`) in
`sdram1`/`sram1`; on acp_7_x everything, including `.text`, goes to `sram1`. The heap takes the `CONFIG_AMD` branch of `alloc.c:80-89`
with `HEAPMEM_SIZE = HEAP_SYSTEM_SIZE + HEAP_RUNTIME_SIZE + HEAP_BUFFER_SIZE`.
The SOF mailbox is in the ACP scratch memory at `SRAM0_BASE` `0x9FF00000`:
outbox, inbox, debug, exception, stream and trace, 0x400 each, 0x1800 total
(computed; e.g. `sof/src/kernel/platform/amd/rembrandt/include/platform/lib/memory.h:59-84`).

Common shape (acp_6_0 addresses; acp_7_0 and acp_7_x differ as in the table):

```
 0xe0020000 +-------------------------------------------+
            | sdram0 (DRAM0): .rodata, .fw_ready,       | DRAM0_BASE/SIZE
            | .noinit, .data, .bss                      |
 0xe0000000 +===========================================+
     ...
 0x9ff01800 +-------------------------------------------+
            | SOF mailbox in ACP scratch, 0x1800        | SRAM0_BASE (SOF memory.h)
 0x9ff00000 +===========================================+
     ...
 0x7f060000 +-------------------------------------------+
            | IRAM 384 K: vectors, .iram.text, .text    | IRAM_BASE/SIZE
 0x7f000000 +===========================================+
     ...
 0x60086000 +-------------------------------------------+
            | sdram1 (SRAM1): .heap_mem, heapmem        | SRAM1_BASE/SIZE
 0x60006000 +===========================================+
```

| Item | acp_6_0 | acp_7_0 | acp_7_x | Source |
|---|---|---|---|---|
| Linker script | `zephyr/soc/amd/acp_6_0/adsp/linker.ld` (macros from `adsp/memory.h`) | `zephyr/soc/amd/acp_7_0/linker.ld:25-32` | `zephyr/soc/amd/acp_7_x/linker.ld:25-32` | |
| IRAM | `0x7f000000`, 0x60000 | `0x7f000000`, 0x60000 | `0x7f000000`, 0x20000 | linker macros |
| `sdram0` | `0xe0000000`, 0x20000 (DRAM0) | `0xe0000000`, 0x1f000 (DRAM0+DRAM1 minus 4 K stack) | `0xe0000000`, 0x1f000 | linker MEMORY (computed for 7.x) |
| SRAM1 region | `sdram1` `0x60006000`, 0x80000 | `sram1` `0x6000c000`, 0x274000 | `sram1` `0x6000c000`, 0x350000 | linker MEMORY |
| Code/data placement | `.text` IRAM, data `sdram0` | same | everything in `sram1` (`RAMABLE_REGION` = `sram1`) | linker script |
| `HEAP_SYSTEM/RUNTIME/BUFFER_SIZE` | 0xE000 / 0xE700 / 0xF000 | 0x40000 / 0x40000 / 0x60000 | same as 7.0 | SOF platform `memory.h` (rembrandt `:104,111-116,119`; acp_7_0 `:125,132-137,140`; acp_7_x `:174,181-186,189`) |
| `HEAPMEM_SIZE` | 0x2B700 (173.75 K) | 0xE0000 (896 K) | 0xE0000 (896 K) | computed |
| Heap address | `0x60006000`-`0x600316ff` | `0x6000c000`-`0x600ebfff` | after `.bss` in `sram1` (depends on link) | computed, assuming `.heap_mem` is first in the region |
| `CONFIG_SYS_HEAP_*` | `SMALL_ONLY=y` | `SMALL_ONLY=y` | `BIG_ONLY=y` (board conf `:65`) | .config / board conf |

## MediaTek (derived, not linked)

Targets `mt8186`, `mt8188`, `mt8195`, `mt8196`, `mt8365` (boards
`<soc>/<soc>/adsp`, board confs `sof/app/boards/<soc>_<soc>_adsp.conf`). All
five stop at CMake with `PLATFORM is not defined, check your Kconfiguration?`
(`sof/zephyr/CMakeLists.txt:574`): SOF tests `CONFIG_SOC_FAMILY_MTK`
(`sof/zephyr/CMakeLists.txt:525`, `sof/zephyr/lib/alloc.c:131`,
`sof/zephyr/Kconfig:292`), but Zephyr at this pin defines
`SOC_FAMILY_MT8XXX` (`zephyr/soc/mediatek/mt8xxx/Kconfig.soc:4`) and the
`.config`s have `CONFIG_SOC_FAMILY_MT8XXX=y`.

All five share `zephyr/soc/mediatek/mt8xxx/common/adsp/linker.ld` (the
per-SoC `linker.ld` files are wrappers that include it). Its `MEMORY` uses
Devicetree: `sram` = `sram0`, `dram` = `dram0` (`:8-15`). Vectors and
`.iram` go to `sram`, followed by `_mtk_adsp_sram_end` (`:41`); `.text`,
`.rodata`, `.data`, `.bss` go to `dram`, followed by `_end` and
`_mtk_adsp_dram_end` (`:110-111`), then `.nocache`. `dram1` is not a linker
region; the SoC MPU setup (`zephyr/soc/mediatek/mt8xxx/common/adsp/soc.c:201-217`)
maps it uncached as the host DMA area.

The intended SOF heap (`alloc.c:131-151`, dead code at this pin because of
the symbol rename) runs from `ALIGN_UP(_mtk_adsp_sram_end, 128)` to the end of
`sram0` (on MT8365 all of `sram1`), capped at `0x7fff * 8` = 0x3FFF8 bytes
because `CONFIG_SYS_HEAP_SMALL_ONLY=y` (no `zephyr,sram` chosen, so
`CONFIG_SRAM_SIZE=0`). The IPC mailbox is in `dram0` at `MTK_IPC_BASE`
(`sof/src/kernel/platform/mtk/include/platform/lib/memory.h:34-56`): upbox
4 K, downbox 4 K, debug 2 K, exception 2 K, stream 4 K, trace 4 K, 0x5000
total (computed).

```
            +-------------------------------------------+ dram1 end
            | dram1: uncached host DMA area (MPU only)  | DT dram1
            +===========================================+
            | gap on MT8188/8195/8196; none on 8186/8365|
            +-------------------------------------------+ dram0 end
            | uncached from _mtk_adsp_dram_end          |
            | (log buffer, IPC mailbox, boot stack are  |
            |  fixed addresses in this range)           |
            +-------------------------------------------+ _end / _mtk_adsp_dram_end
            | .text .rodata .data .bss .noinit          | dram
            +===========================================+ dram0 base
     ...
            +-------------------------------------------+ sram0 end
            | SOF heap (intended), max 0x3FFF8          | alloc.c:131-151
            +-------------------------------------------+ _mtk_adsp_sram_end
            | .sof_entry, vectors, .iram                | sram
            +===========================================+ sram0 base
```

| Item | mt8186 | mt8188 | mt8195 | mt8196 | mt8365 | Source |
|---|---|---|---|---|---|---|
| `sram0` | `0x4e100000`, 0x100000 | `0x4e100000`, 0x80000 | `0x40000000`, 0x40000 | `0x4e100000`, 0x80000 | `0x40020000`, 0x40000 | zephyr.dts |
| `sram1` | - | - | - | - | `0x1e000000`, 0x68000 | zephyr.dts |
| `dram0` | `0x60000000`, 0x1000000 | `0x60000000`, 0xf00000 | `0x60000000`, 0xd80000 | `0x90000000`, 0x600000 | `0x60000000`, 0xd80000 | zephyr.dts |
| `dram1` | `0x61000000`, 0x100000 | `0x61000000`, 0x100000 | `0x60e80000`, 0x280000 | `0x90800000`, 0x100000 | `0x60d80000`, 0x280000 | zephyr.dts |
| IPC mailbox | `0x60500000`, 0x5000 | `0x60500000` | `0x60800000` | `0x90500000` | `0x60800000` | computed (`dram0` + 0x500000, or + 0x800000 on MT8195/MT8365) |
| Log buffer | `0x60700000`, 1 M | `0x60700000`, 1 M | `0x60700000`, 1 M | `0x90580000`, 0x80000 | `0x60700000`, 1 M | `soc.c:35-40` |
| Boot stack (`INIT_STACK`) | `0x60e00000` | `0x60e00000` | `0x60e00000` | `0x90400000` | `0x60e00000` | `soc.c:34,38` |
| Intended heap | `sram0` rest, capped 0x3FFF8 | same | `sram0` rest (< 256 K) | `sram0` rest, capped | `sram1`, 0x3FFF8 of 0x68000 | computed |

## QEMU Xtensa

Targets `qemu_xtensa` (board `qemu_xtensa/dc233c`) and `qemu_xtensa_mmu`
(`qemu_xtensa/dc233c/mmu`), both built and linked. SOC `dc233c`, linker
script `zephyr/soc/cdns/dc233c/include/xtensa-dc233c.ld`. RAM comes from DT
`sram0` (`zephyr/dts/xtensa/dc233c.dtsi:27`, `0x0`, 16 M) offset by
`CONFIG_SRAM_OFFSET` (`xtensa-dc233c.ld:36-44`), which is 0x2400 without MMU
and 0x100000 with MMU (`zephyr/soc/cdns/dc233c/Kconfig.defconfig:16-19`). With
`CONFIG_XTENSA_MMU` the gap becomes the `vec_helpers` region
(`xtensa-dc233c.ld:49-51`) for exception and syscall code. The heap takes the
generic branch (`alloc.c:153-157`): from `ALIGN_UP(_end, 32)` to
`_heap_sentry` = end of `RAM` (`xtensa-dc233c.ld:343`). There is no VMH, vpage
or library region (`CONFIG_MM_DRV=y` is set by the board conf, but
`SOF_VREGIONS` depends on `ACE`). The SOF mailbox macros
(`sof/src/platform/qemu_xtensa/include/platform/lib/mailbox.h:10-22`) are
placeholders at `0x10000000`-`0x10005fff`, outside `sram0`.

```
                 qemu_xtensa                      qemu_xtensa_mmu
 0x01000000 +--------------------------+  +--------------------------+ sram0 end, _heap_sentry
            | heap 0xf996e0 (15.6 M)   |  | heap 0xe3b000 (14.2 M)   |
 _end       +--------------------------+  +--------------------------+ 0x66904 / 0x1c5000
            | noinit, .bss, .data,     |  | kobject_data, noinit,    |
            | .rodata, .text           |  | .bss, .data, app_smem,   |
 0x00002400 +--------------------------+  | .rodata, .text           |
            | vectors                  |  +--------------------------+ 0x00100000 RAM origin
 0x00002000 +--------------------------+  | .vec_helpers 0xea94      |
                                          +--------------------------+ 0x00002400
                                          | vectors                  |
                                          +--------------------------+ 0x00002000
 0xfe000000: rom0 16 K, .ResetVector.text (both)
```

| Item | qemu_xtensa | qemu_xtensa_mmu | Source |
|---|---|---|---|
| `vectors` | `0x2000`, 0x2400 | `0x2000`, 0x2400 | zephyr.map |
| `vec_helpers` | - | `0x2400`, 0xfdc00 | zephyr.map |
| `RAM` | `0x2400`, 0xffdc00 | `0x100000`, 0xf00000 | zephyr.map |
| `.text` | `0x2400`, 0x32458 | `0x100000`, 0x48000 | size -A |
| `.rodata` | `0x34858`, 0x2c37c | `0x148000`, 0x2f450 | size -A |
| `.data` | `0x61784`, 0x1d4 | `0x17b000`, 0x1000 | size -A |
| `.bss` | `0x619c0`, 0x17f8 | `0x17d000`, 0x3c000 | size -A |
| `_end` | `0x66904` | `0x1c5000` | nm |
| Image in `RAM` | 0x64504 (401.3 K) | 0xc5000 (788 K) | computed |
| System heap | `0x66920`, 0xf996e0 | `0x1c5000`, 0xe3b000 | computed |

## Not built: imx93 A55, native_sim

### imx93_evk_mimx9352_a55

Not built. From `sof/app/boards/imx93_evk_mimx9352_a55.{conf,overlay}` and
`zephyr/boards/nxp/imx93_evk/imx93_evk_mimx9352_a55.dts`: the SOF overlay
deletes `memory@d0000000` and adds `dram: memory@ce000000`, 1 M (overlay
`:8-13`), which `chosen { zephyr,sram = &dram; }` (board DTS `:21`) makes the
RAM region of the generic arm64 linker script
(`zephyr/include/zephyr/arch/arm64/scripts/linker.ld:56-81`, `CONFIG_XIP=n`,
so code and data share RAM). The heap is a 64 K `heapmem` in `.bss`
(`alloc.c:102`, `HEAPMEM_SIZE` in
`sof/src/kernel/platform/imx93_a55/include/platform/lib/memory.h:42`). The SOF
mailbox follows RAM: hostbox `0xce100000`, dspbox `0xce101000`, stream
`0xce102000`, 4 K each
(`sof/src/platform/imx93_a55/include/platform/lib/mailbox.h:47-59`), matching
the overlay `outbox`/`inbox`/`stream` nodes. The overlay also describes
`host_ram` at `0x80000000` (1 G) and `mu2_a` at `0x42430000`.

```
 0xce103000 +-------------------------------------------+
            | SOF mailbox: hostbox, dspbox, stream      | mailbox.h, overlay nodes
 0xce100000 +===========================================+
            | RAM 1 M: image + .bss (heapmem 64 K)      | overlay dram, chosen zephyr,sram
 0xce000000 +===========================================+
```

### native_sim, native_sim_libfuzzer

Not built. These run SOF as a host process; the host linker places all
sections (`zephyr/include/zephyr/arch/posix/linker.ld` only inserts Zephyr
sections into the host's default script), and
`zephyr/boards/native/native_sim/native_sim.dts` has no `sram` node or
`zephyr,sram` (only a simulated 2 M flash). The SOF heap is a 256 K host
array (`alloc.c:125-129`). With `CONFIG_SOF_NATIVE_SIM_HOST_HEAP=y`
(`sof/zephyr/Kconfig:79`, default n, not set in
`sof/app/boards/native_sim*.conf`) SOF allocations go to the host C library
instead. The mailboxes are host globals (`sof/src/kernel/platform/posix/posix.c:13-16`).

## Summary

Sizes in K (1024 bytes). "image" is the linked RAM image: ACE/cAVS from the
vectors to `__bss_end`, others from the start of the data RAM region to the
end of `.bss`/`noinit`. Values in italics are computed or derived.

| Target | Source | Main RAM | Image | System heap | VMH | vpages | LLEXT size | Notes |
|---|---|---|---|---|---|---|---|---|
| mtl (ARL) | built | L2 2816 K | *685.2 K* | 384 K | - | *356 (1424 K)* | 256 K | LLEXT region 1 M at `0xa0688000` (all ACE); 356 confirmed by boot log on ARL-S |
| lnl | built | L2 2816 K | *642.5 K* | 384 K | - | *367 (1468 K)* | 256 K | as mtl |
| ptl | built | L2 4608 K | *948 K* | 384 K (320 K + 64 K shared) | - | *739 (2956 K)* | 256 K | USERSPACE, 200 K page tables; 739 confirmed by boot log |
| wcl | built | L2 3456 K | *772 K* | 384 K | - | *495 (1980 K)* | 256 K | 256 K page tables |
| nvl | built | L2 4608 K | *764 K* | 384 K | - | *785 (3140 K)* | 256 K | 256 K page tables |
| nvl-s | built | L2 2048 K | *732 K* | 384 K | - | *153 (612 K)* | 256 K | 256 K page tables |
| tgl | built | L2 2944 K | *733.4 K* | *2146.5 K* | - | - | - | heap = rest of `sram0`; no vpage support in the cAVS TLB driver |
| tgl-h | built | L2 1920 K | *691.8 K* | *1164.2 K* | - | - | - | as tgl |
| imx8 | built | sdram0 8 M + sdram1 8 M (DTS: 512 K + 512 K) | *421.3 K* | 8144 K | - | - | - | heap in sdram1 |
| imx8x | built | as imx8 | *429.3 K* | 8144 K | - | - | - | as imx8 |
| imx8m | built | as imx8 | *424.8 K* | 8144 K | - | - | - | as imx8 |
| imx8ulp | built | as imx8 at `0x1a000000` | *410.6 K* | 8136 K | - | - | - | mailbox 0x7000 |
| imx8m_cm7 | built | FLASH 2 M + RAM 2 M (DDR) | *341.2 K flash, 121.9 K RAM* | 64 K (in `.bss`) | - | - | - | |
| imx95 | built | RAM 42 M (DDR) | *364.8 K code, 108.4 K data* | 64 K (in `.bss`) | - | - | - | |
| qemu_xtensa | built | 16 M | *401.3 K* | *15973.7 K* | - | - | - | heap = rest of RAM |
| qemu_xtensa_mmu | built | 16 M (RAM from 1 M) | *788 K* | *14572 K* | - | - | - | |
| acp_6_0 | derived | IRAM 384 K, DRAM0 128 K, SRAM1 512 K | n/a | *173.75 K* | - | - | - | does not build |
| acp_7_0 | derived | IRAM 384 K, sdram0 124 K, SRAM1 2512 K | n/a | *896 K* | - | - | - | does not build |
| acp_7_x | derived | IRAM 128 K, sdram0 124 K, SRAM1 3392 K | n/a | *896 K* | - | - | - | does not build (Kconfig) |
| mt8186 | derived | sram0 1 M, dram0 16 M | n/a | *<= 255.99 K* | - | - | - | does not build |
| mt8188 | derived | sram0 512 K, dram0 15 M | n/a | *<= 255.99 K* | - | - | - | does not build |
| mt8195 | derived | sram0 256 K, dram0 13.5 M | n/a | *< 256 K* | - | - | - | does not build |
| mt8196 | derived | sram0 512 K, dram0 6 M | n/a | *<= 255.99 K* | - | - | - | does not build |
| mt8365 | derived | sram0 256 K, sram1 416 K, dram0 13.5 M | n/a | *255.99 K* | - | - | - | does not build |
| imx93 A55 | config only | RAM 1 M | n/a | 64 K | - | - | - | not built |
| native_sim | config only | host process | n/a | 256 K | - | - | - | not built |

## System heap users

Measured on ARL-S (ACE 1.5, the MTL build above) with
`sof-mtl-nocodec.tplg` and `multiple-pipeline.sh -f a -c 20 -l 3`.
This is a debug build that records the callers of every system heap
(`sof_heap`) allocation through `__builtin_return_address()`. The "at peak"
column is the live size of each user at the moment the heap reached its
peak; the column adds up to the peak.

| User (allocation site) | At peak | Share | Lifetime |
|---|---|---|---|
| Inter-module buffers: `buffer_new()` (`sof/src/ipc/ipc-helper.c:81`) -> `buffer_alloc()` with no context (`sof/src/audio/buffers/comp_buffer.c:300,309`) | 39,040 B | 35% | stream |
| Host DMA buffers: `host_common_params()` -> `buffer_alloc_range(&hd->alloc_ctx, ...)` (`sof/src/audio/host-zephyr.c:1064`) | 30,336 B | 27% | stream |
| AMS shared context: `ams_init()` -> `coherent_init()` (`sof/src/kernel/lib/ams.c:604`) | 13,312 B | 12% | boot |
| DAI DMA buffers: `dai_set_dma_buffer()` (`sof/src/audio/dai-zephyr.c:1221`) | 7,680 B | 7% | stream |
| IPC: `ipc_init()` | 4,288 B | 4% | boot |
| `comp_dev` objects, `ipc4_add_comp_dev()` | 2,176 B | 2% | stream |
| Host/DAI DMA buffer structures, SG lists, DMA configs and channels | 7,232 B | 6% | stream |
| vregion metadata, `vregion_create()` | 1,536 B | 1% | stream |
| Pipelines, LL tasks, schedulers, notifier, DP/TWB, other | 6,208 B | 6% | boot and stream |
| **Total** | **111,808 B** | | |

The heap holds 20.7 K after boot and 109 K at the peak of the test. Of the
peak, about 85% is audio data that does not use the pipeline vregion:
- `buffer_new()` passes no allocation context, so `buffer_alloc()` falls back
  to `sof_heap_alloc(sof_sys_user_heap_get(), ... | SOF_MEM_FLAG_LARGE_BUFFER)`.
- The host copier allocates its DMA buffer from its own `hd->alloc_ctx`,
  which has no vregion (`sof_ctx_alloc()` falls back to the heap when
  `ctx->vreg` is NULL, `sof/zephyr/include/rtos/alloc.h:274`).

Where these allocations land by target, at this tree:

| Target | Inter-module buffers | Host/DAI DMA buffers | Pipeline/module memory |
|---|---|---|---|
| mtl, lnl, ptl, wcl, nvl, nvl-s | system heap | system heap | pipeline vregion |
| tgl, tgl-h, i.MX, QEMU | system heap | system heap | system heap (no vregions) |

With `CONFIG_SOF_USERSPACE_LL`, `sof_sys_user_heap_get()` returns the LL user
heap instead, so inter-module buffers go there. With `CONFIG_VIRTUAL_HEAP`
(off on all ACE builds here) `rballoc()` would send `SOF_MEM_FLAG_LARGE_BUFFER`
allocations to the VMH instead (`alloc.c:829-833`).

Possible optimizations, not done here:

1. **ACE:** allocate the inter-module buffers and the host/DAI DMA buffers
   from the pipeline vregion, by passing the pipeline allocation context. On
   MTL this would take the measured heap peak from 109 K to about 28 K, so
   the 384 K ACE system heap could shrink to about 128 K, giving 64 more
   vpages on each ACE target. The pipeline vregions have room for it: the
   kernel sends 49-96 K per pipeline from the topology defaults, and about
   4 K of that is used.
2. **All targets with `CONFIG_AMS`:** the AMS shared context has one
   `AMS_MAX_MSG_SIZE` (4 K) message slot per core (`sof/src/include/sof/lib/ams.h:30`).
   Its users (KPB, `detect_test`) send small messages, so a smaller slot or
   a Kconfig option would save about 11 K of heap from boot on every AMS
   platform.
3. **cAVS, i.MX, QEMU:** the system heap is the rest of RAM, and the pipeline
   memory comes from it as well. Nothing to gain without vregions.
4. **imx8m_cm7, imx95, imx93:** 64 K heaps hold everything. On MTL the
   pipeline objects, buffers and DMA buffers of this test peak at 109 K, so
   larger topologies may not fit there. Not measured.

## Notes and observations

These are observations from the data above, not proposed fixes.

1. All six ACE targets now use the same memory configuration: 384 K system
   heap, no VMH, 256 K LLEXT size. The vpage count only follows the L2 size
   and the image size: from 153 pages on NVL-S (2048 K L2) to 785 on NVL
   (4608 K L2).
2. NVL-S has the fewest vpages (153, 612 K). Everything below
   `_unused_ram_start_marker` (reserve, vectors, image, heap) takes 1180 K of
   its 2048 K L2, and the 256 K LLEXT size is a larger share of the rest
   than on the other targets.
3. MTL and LNL vpage regions end at the same address, `0xa02a0000`, because
   both have 2816 K of L2 and a 256 K reserve; the vpage count differs only
   by the image size.
4. The ACE `.bss` on MMU targets is dominated by the Xtensa MMU L2 page
   tables: 200 K on PTL (50 tables), 256 K on WCL/NVL/NVL-S (64 tables),
   which directly reduces the L2 left for vpages.
5. The vpage formula's virtual cap (`LIBRARY_BASE_ADDRESS`) does not bind on
   any ACE target here; the physical term does. The VA between the vpage end
   and `0xa0688000` is not assigned to any region.
6. The physical page pool is sized at boot from `ace_hpsram_get_bank_count()`
   (`mm_drv_intel_adsp_mtl_tlb.c:811-813`), not from DTS; the vpage size uses
   the DTS `sram0` size. If hardware reports fewer banks than DTS, the
   computed numbers here are upper bounds.
7. If `CONFIG_VIRTUAL_HEAP` is enabled again, its region size equals the
   sum of its static buffer table: 0x140000 with the extended 8 x 32 K
   entry, 0x100000 without (`alloc.c:396-411`, computed). It comes straight
   out of the vpage budget, since the vpage region starts after it.
8. cAVS `_image_ram_end` is `0xbe800000` (LP SRAM), while ACE sets it to the
   end of `sram0`. cAVS 2.5 (tgl, tgl-h) cannot use the ACE vpage scheme yet:
   its TLB driver has no free physical page allocator and no virtual region
   API, and its Devicetree has no `sram0virtual` window (see
   [Intel cAVS 2.5](#intel-cavs-25)).
9. i.MX DSP DTS `sram0`/`sram1` nodes describe 512 K each, but the linker
   uses hard-coded 8 M SDRAM0/SDRAM1 regions and the SOF heap is 8 M; the
   DTS does not describe the memory actually used.
10. imx8m_cm7 and imx95 SOF mailboxes (`0x82000000`, `0x86000000`) lie outside
    every linker region; on imx8m_cm7 an MPU region maps them non-cacheable.
11. qemu_xtensa without MMU: the `vectors` region (`0x2000`, 0x2400 long)
    overlaps the start of `RAM` (`0x2400`); only 0x3cd bytes of vectors are
    used.
12. No target here builds for AMD or MediaTek at this Zephyr pin. For
    MediaTek the same undefined `CONFIG_SOC_FAMILY_MTK` also guards the SOF
    heap branch in `alloc.c:131`, so while it stays undefined the heap would
    fall into the generic branch, which needs `_heap_sentry`; the MTK linker
    script does not define it.
13. AMD: `acp_7_0` computes a 896 K `HEAPMEM_SIZE` with
    `CONFIG_SYS_HEAP_SMALL_ONLY=y`, whose limit is 0x7fff * 8 = 262136
    bytes. SOF and Zephyr headers disagree on some AMD sizes: acp_6_0 DRAM0
    (Zephyr 0x20000 vs SOF 0x10000) and SRAM1 (Zephyr 0x80000 vs SOF
    0x13A000); acp_7_x SRAM1 (Zephyr 0x350000 vs SOF 0x3F4000).
14. MT8195: the boot stack `0x60e00000` is between `dram0` end
    (`0x60d80000`) and `dram1` (`0x60e80000`); MT8365: it is inside `dram1`.
