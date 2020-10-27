# SPDX-License-Identifier: BSD-3-Clause

set(STATE_DEFS
	-D__SPLIT__extra_size
	-D__SPLIT__extra_align
	-D__SPLIT__cpregs_size
	-D__SPLIT__cpregs_align
	-D__SPLIT__cp_names
	-D__SPLIT__all_extra_size
	-D__SPLIT__all_extra_align
	-D__SPLIT__num_coprocessors
	-D__SPLIT__cp_num
	-D__SPLIT__cp_max
	-D__SPLIT__cp_mask
	-D__SPLIT__cp_id_mappings
	-D__SPLIT__cp_mask_mappings
	-D__SPLIT__init_mem_extra
	-D__SPLIT__init_mem_cp
	-D__SPLIT__save_extra
	-D__SPLIT__restore_extra
	-D__SPLIT__save_cpregs
	-D__SPLIT__save_cp0
	-D__SPLIT__save_cp1
	-D__SPLIT__save_cp2
	-D__SPLIT__save_cp3
	-D__SPLIT__save_cp4
	-D__SPLIT__save_cp5
	-D__SPLIT__save_cp6
	-D__SPLIT__save_cp7
	-D__SPLIT__restore_cpregs
	-D__SPLIT__restore_cp0
	-D__SPLIT__restore_cp1
	-D__SPLIT__restore_cp2
	-D__SPLIT__restore_cp3
	-D__SPLIT__restore_cp4
	-D__SPLIT__restore_cp5
	-D__SPLIT__restore_cp6
	-D__SPLIT__restore_cp7
	-D__SPLIT__cpregs_save_fn
	-D__SPLIT__cpregs_restore_fn
	-D__SPLIT__validate_cp
	-D__SPLIT__invalidate_cp
	-D__SPLIT__get_cpenable
	-D__SPLIT__set_cpenable
)

set(DISASS_DEFS
	-D__SPLIT__op0_format_lengths
	-D__SPLIT__byte0_format_lengths
	-D__SPLIT__disassemble_size
	-D__SPLIT__disassemble
)

set(MISC_DEFS
	-D__SPLIT__clear_regcached_code
)

#  Call0 ABI means the xthal... and xthal..._nw functions are
#  identical.  If we're building for Call0 ABI, omit the ..._nw
#  functions (except for xthal_get_intpending_nw, an interrupt handler
#  helper function for which there is no duplicate and which does not
#  obey _any_ calling conventions).
set(INTERRUPTS_DEFS
	-D__SPLIT__num_intlevels
	-D__SPLIT__num_interrupts
	-D__SPLIT__excm_level
	-D__SPLIT__intlevel
	-D__SPLIT__get_intenable
	-D__SPLIT__set_intenable
	-D__SPLIT__get_interrupt
	-D__SPLIT__set_intset
	-D__SPLIT__set_intclear
)

set(CACHE_DEFS
	-D__SPLIT__get_cacheattr
	-D__SPLIT__get_icacheattr
	-D__SPLIT__set_cacheattr
	-D__SPLIT__set_icacheattr
	-D__SPLIT__set_dcacheattr
	-D__SPLIT__set_idcacheattr
	-D__SPLIT__idcache_is_enabled
	-D__SPLIT__icache_is_enabled
	-D__SPLIT__dcache_is_enabled
	-D__SPLIT__idcache_is_enabled
	-D__SPLIT__icache_all_invalidate
	-D__SPLIT__dcache_all_invalidate
	-D__SPLIT__dcache_all_writeback
	-D__SPLIT__dcache_all_writeback_inv
	-D__SPLIT__icache_all_unlock
	-D__SPLIT__dcache_all_unlock
	-D__SPLIT__icache_region_invalidate
	-D__SPLIT__dcache_region_invalidate
	-D__SPLIT__dcache_region_writeback
	-D__SPLIT__dcache_region_writeback_inv
	-D__SPLIT__icache_region_lock
	-D__SPLIT__dcache_region_lock
	-D__SPLIT__icache_region_unlock
	-D__SPLIT__dcache_region_unlock
	-D__SPLIT__icache_line_invalidate
	-D__SPLIT__dcache_line_invalidate
	-D__SPLIT__dcache_line_writeback
	-D__SPLIT__dcache_line_writeback_inv
	-D__SPLIT__icache_line_lock
	-D__SPLIT__dcache_line_lock
	-D__SPLIT__icache_line_unlock
	-D__SPLIT__dcache_line_unlock
	-D__SPLIT__icache_sync
	-D__SPLIT__dcache_sync
	-D__SPLIT__icache_get_ways
	-D__SPLIT__icache_set_ways
	-D__SPLIT__dcache_get_ways
	-D__SPLIT__dcache_set_ways
	-D__SPLIT__cache_coherence_on
	-D__SPLIT__cache_coherence_off
	-D__SPLIT__set_cache_prefetch_long
	-D__SPLIT__set_cache_prefetch
	-D__SPLIT__get_cache_prefetch
	-D__SPLIT__hw_configid0
	-D__SPLIT__hw_configid1
	-D__SPLIT__release_major
	-D__SPLIT__release_minor
)

add_library(hal STATIC "")
target_link_libraries(hal sof_options)
target_compile_definitions(hal PRIVATE
	${STATE_DEFS}
	${DISASS_DEFS}
	${MISC_DEFS}
	${INTERRUPTS_DEFS}
	${CACHE_DEFS}
)

add_local_sources(hal
	cache_asm.S
	clock.S
	int_asm.S
	interrupts.c
	memcopy.S
	windowspill_asm.S
)
