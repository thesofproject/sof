// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022 - 2023 Intel Corporation.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

#include <zephyr/init.h>

#if defined(CONFIG_MM_DRV)
#include <sof/lib/regions_mm.h>


/** @struct vmh_heap
 *
 *  @brief This structure holds all information about virtual memory heap
 *  it aggregates information about its allocations and
 *  physical mappings.
 *
 *  @var virtual_region pointer to virtual region information, it holds its
 *  attributes size and beginning ptr provided by zephyr.
 *  @var physical_blocks_allocators[] a table of block allocators
 *  each representing a virtual regions part in blocks of a given size
 *  governed by sys_mem_blocks API.
 *  @var allocation_sizes[] a table of bit arrays representing sizes of allocations
 *  made in physical_blocks_allocators directly related to physical_blocks_allocators
 *  @var allocating_continuously configuration value deciding if heap allocations
 *  will be contiguous or single block.
 */
struct vmh_heap {
	struct k_mutex lock;
	const struct sys_mm_drv_region *virtual_region;
	struct sys_mem_blocks *physical_blocks_allocators[MAX_MEMORY_ALLOCATORS_COUNT];
	struct sys_bitarray *allocation_sizes[MAX_MEMORY_ALLOCATORS_COUNT];
	bool allocating_continuously;
};

/**
 * @brief Initialize new heap
 *
 * Initializes new heap based on region that heap is to be assigned to
 * and config. The heap size overall must be aligned to physical page
 * size.
 * @param cfg Configuration structure with block structure for the heap
 * @param allocating_continuously Flag deciding if heap can do contiguous allocation.
 *
 * @retval ptr to a new heap if successful.
 * @retval NULL on creation failure.
 */
struct vmh_heap *vmh_init_heap(const struct vmh_heap_config *cfg, bool allocating_continuously)
{
	const struct sys_mm_drv_region *virtual_memory_regions =
		sys_mm_drv_query_memory_regions();
	int i;

	struct vmh_heap *new_heap =
		rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT, sizeof(*new_heap));

	if (!new_heap)
		return NULL;

	k_mutex_init(&new_heap->lock);
	struct vmh_heap_config new_config = {0};
	const struct sys_mm_drv_region *region;

	SYS_MM_DRV_MEMORY_REGION_FOREACH(virtual_memory_regions, region) {
		if (region->attr == VIRTUAL_REGION_SHARED_HEAP_ATTR) {
			new_heap->virtual_region = region;
			break;
		}
	}
	if (!new_heap->virtual_region || !new_heap->virtual_region->size)
		goto fail;

	/* If no config was specified we will use default one */
	if (!cfg) {
		vmh_get_default_heap_config(new_heap->virtual_region, &new_config);
		cfg = &new_config;
	}

	/* Check if configuration provided can fit into virtual regions memory
	 * and if it is physical page aligned
	 */
	size_t total_requested_size = 0;

	for (i = 0; i < MAX_MEMORY_ALLOCATORS_COUNT; i++) {
		if (cfg->block_bundles_table[i].block_size > 0) {
			/* Block sizes can only be powers of 2*/
			if (!is_power_of_2(cfg->block_bundles_table[i].block_size))
				goto fail;
			total_requested_size += cfg->block_bundles_table[i].block_size *
				cfg->block_bundles_table[i].number_of_blocks;

			if (total_requested_size > new_heap->virtual_region->size ||
					total_requested_size % CONFIG_MM_DRV_PAGE_SIZE)
				goto fail;
		}
	}

	/* Offset used to calculate where in virtual region to start buffer
	 * for the memblock
	 */
	uint32_t offset = 0;

	/* Iterate over all blocks and fill those that have config size value
	 * higher than 0.
	 * Each cfg entry is converted into new allocators based on mem_blocks
	 * API. It needs a lot of runtime allocations for members of mem_blocks.
	 * The allocation_sizes bit arrays are runtime allocated too.
	 */
	for (i = 0; i < MAX_MEMORY_ALLOCATORS_COUNT; i++) {
		/* Only run for sizes > 0 */
		if (!cfg->block_bundles_table[i].block_size)
			continue;

		/* bitarray_size is number of bit bundles that bittaray needs
		 * to represent memory blocks that's why we div round up number of blocks
		 * by number of bits in byte
		 */
		size_t bitarray_size = SOF_DIV_ROUND_UP(
			SOF_DIV_ROUND_UP((uint64_t)cfg->block_bundles_table[i].number_of_blocks, 8),
			sizeof(uint32_t));
		size_t bitfield_size = sizeof(uint32_t) * bitarray_size;

		/* Need to create structs that are components of mem_blocks and constantly keep
		 * them in memory on sys_heap.
		 * First create allocator - instance of sys_mem_blocks struct.
		 */
		struct sys_mem_blocks *new_allocator = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			sizeof(sys_mem_blocks_t));

		if (!new_allocator)
			goto fail;

		/* Assign allocator(mem_block) to table */
		new_heap->physical_blocks_allocators[i] = new_allocator;

		/* Fill allocators data based on config and virtual region data */
		new_allocator->info.num_blocks = cfg->block_bundles_table[i].number_of_blocks;
		new_allocator->info.blk_sz_shift = ilog2(cfg->block_bundles_table[i].block_size);
		new_allocator->buffer =	(uint8_t *)new_heap->virtual_region->addr + offset;

		/* Create bit array that is a part of mem_block kept as a ptr */
		struct sys_bitarray *allocators_bitarray = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			sizeof(sys_bitarray_t));

		if (!allocators_bitarray)
			goto fail;

		allocators_bitarray->num_bits =	cfg->block_bundles_table[i].number_of_blocks;
		allocators_bitarray->num_bundles = bitarray_size;
		new_allocator->bitmap = allocators_bitarray;

		/* Create bit array responsible for saving sizes of allocations
		 * this bit array is saved in heap struct in a table allocation_sizes
		 * It is also the same size as the allocator one because they will work
		 * together as a means to save allocation data.
		 * Mechanism explained in detail in free and alloc function.
		 */
		struct sys_bitarray *allocation_sizes_bitarray =
			rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
				sizeof(sys_bitarray_t));

		if (!allocation_sizes_bitarray)
			goto fail;

		allocation_sizes_bitarray->num_bits = allocators_bitarray->num_bits;
		allocation_sizes_bitarray->num_bundles = allocators_bitarray->num_bundles;
		new_heap->allocation_sizes[i] = allocation_sizes_bitarray;

		/* Each sys_bitarray has a ptr to bit field that has to be allocated
		 * based on its size.
		 */
		uint32_t *allocator_bitarray_bitfield =
			rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT, bitfield_size);

		if (!allocator_bitarray_bitfield)
			goto fail;

		allocators_bitarray->bundles = allocator_bitarray_bitfield;

		uint32_t *sizes_bitarray_bitfield = rzalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
			bitfield_size);

		if (!sizes_bitarray_bitfield)
			goto fail;

		new_heap->allocation_sizes[i]->bundles = sizes_bitarray_bitfield;

		/* Update offset, note that offset is CONFIG_MM_DRV_PAGE_SIZE aligned
		 * since every cfg size was checked against it earlier
		 */
		offset += cfg->block_bundles_table[i].number_of_blocks
			* cfg->block_bundles_table[i].block_size;
	}

	new_heap->allocating_continuously = allocating_continuously;

	return new_heap;
fail:
	for (i = 0; i < MAX_MEMORY_ALLOCATORS_COUNT; i++) {
		struct sys_mem_blocks *allocator = new_heap->physical_blocks_allocators[i];
		struct sys_bitarray *allocation_sizes = new_heap->allocation_sizes[i];

		if (allocator) {
			if (allocator->bitmap)
				rfree(allocator->bitmap->bundles);
			rfree(allocator->bitmap);
		}
		if (allocation_sizes) {
			if (allocation_sizes->bundles)
				rfree(allocation_sizes->bundles);
			rfree(allocation_sizes);
		}
		rfree(allocator);
	}
	rfree(new_heap);
	return NULL;
}

/**
 * @brief Checks if region of a sys_mem_blocks have any allocated block
 *
 * @param blocks Pointer to the sys_mem_blocks allocator
 * @param ptr Pointer to the memory region to be checked
 * @param size Size of memory region
 *
 * @retval true if there is any allocation in the queried region
 */
static inline bool vmh_is_region_used(struct sys_mem_blocks *blocks, const uintptr_t ptr,
				      const size_t size)
{
	__ASSERT_NO_MSG(IS_ALIGNED(size, 1 << blocks->info.blk_sz_shift));
	return !sys_mem_blocks_is_region_free(blocks, UINT_TO_POINTER(ptr),
					      size >> blocks->info.blk_sz_shift);
}

/**
 * @brief Checks whether the memory region is located on memory pages unused by other allocations
 *
 * @param allocator Pointer to the sys_mem_blocks allocator
 * @param ptr Pointer to the memory region to be checked
 * @param size Size of memory region
 * @param begin Address to a variable where address of the first unused memory page will be placed
 * @param out_size Address to a variable where size of unused memory pages will be placed
 *
 * @retval true if there is any unused memory page
 */
static bool vmh_get_map_region_boundaries(struct sys_mem_blocks *blocks, const void *ptr,
					  const size_t size, uintptr_t *begin, size_t *out_size)
{
	__ASSERT_NO_MSG(blocks);
	__ASSERT_NO_MSG(begin);
	__ASSERT_NO_MSG(out_size);

	const size_t block_size = 1 << blocks->info.blk_sz_shift;
	const size_t size_aligned = ALIGN_UP(size, block_size);
	uintptr_t addr = ALIGN_DOWN((uintptr_t)ptr, CONFIG_MM_DRV_PAGE_SIZE);
	uintptr_t addr_end = ALIGN_UP((uintptr_t)ptr + size, CONFIG_MM_DRV_PAGE_SIZE);
	const uintptr_t size_before = (uintptr_t)ptr - addr;
	const uintptr_t size_after = addr_end - (uintptr_t)ptr - size_aligned;

	__ASSERT_NO_MSG(IS_ALIGNED(size_before, block_size));

	if (size_before && vmh_is_region_used(blocks, addr, size_before))
		/* skip first page */
		addr += CONFIG_MM_DRV_PAGE_SIZE;

	if (size_after && vmh_is_region_used(blocks, POINTER_TO_UINT(ptr) + size_aligned,
					     size_after))
		/* skip last page */
		addr_end -= CONFIG_MM_DRV_PAGE_SIZE;

	if (addr >= addr_end)
		return false;

	*begin = addr;
	*out_size = addr_end - addr;
	return true;
}

/**
 * @brief Determine the size of the mapped memory region.
 *
 * This function calculates the size of a mapped memory region starting from the given address.
 * It uses a binary search algorithm to find the boundary of the mapped region by checking if
 * pages are mapped or not.
 *
 * @param addr  Starting address of the memory region.
 * @param size  Pointer to the size of the memory region. This value will be updated to reflect
 *		the size of the mapped region.
 *
 * @retval None
 */
static void vmh_get_mapped_size(void *addr, size_t *size)
{
	int ret;
	uintptr_t check, unused;
	uintptr_t bottom, top;

	if (*size <= CONFIG_MM_DRV_PAGE_SIZE)
		return;

	bottom = (POINTER_TO_UINT(addr));
	top = bottom + *size;
	check = top - CONFIG_MM_DRV_PAGE_SIZE;
	while (top - bottom > CONFIG_MM_DRV_PAGE_SIZE) {
		ret = sys_mm_drv_page_phys_get(UINT_TO_POINTER(check), &unused);
		if (!ret)
			bottom = check;	/* Page is mapped */
		else
			top = check;	/* Page is unmapped */

		check = ALIGN_DOWN(bottom / 2 + top / 2, CONFIG_MM_DRV_PAGE_SIZE);
	}

	*size = top - POINTER_TO_UINT(addr);
}

/**
 * @brief Maps memory pages for a memory region if they have not been previously mapped for other
 *	  allocations.
 *
 * @param allocator Pointer to the sys_mem_blocks allocator
 * @param ptr Pointer to the memory region
 * @param size Size of memory region
 *
 * @retval 0 on success, error code otherwise.
 */
static int vmh_map_region(struct sys_mem_blocks *region, void *ptr, size_t size)
{
	const size_t block_size = 1 << region->info.blk_sz_shift;
	uintptr_t begin;
	int ret;

	if (block_size >= CONFIG_MM_DRV_PAGE_SIZE) {
		begin = POINTER_TO_UINT(ptr);
		size = ALIGN_UP(size, CONFIG_MM_DRV_PAGE_SIZE);
	} else {
		if (!vmh_get_map_region_boundaries(region, ptr, size, &begin, &size))
			return 0;
	}

	ret = sys_mm_drv_map_region(UINT_TO_POINTER(begin), 0, size, SYS_MM_MEM_PERM_RW);

	/* In case of an error, the pages that were successfully mapped must be manually released */
	if (ret)
		sys_mm_drv_unmap_region(UINT_TO_POINTER(begin), size);

	return ret;
}

/**
 * @brief Unmaps memory pages for a memory region if they are not used by other allocations.
 *
 * @param allocator Pointer to the sys_mem_blocks allocator
 * @param ptr Pointer to the memory region
 * @param size Size of memory region
 *
 * @retval 0 on success, error code otherwise.
 */
static int vmh_unmap_region(struct sys_mem_blocks *region, void *ptr, size_t size)
{
	const size_t block_size = 1 << region->info.blk_sz_shift;
	uintptr_t begin;

	if (block_size >= CONFIG_MM_DRV_PAGE_SIZE) {
		size = ALIGN_UP(size, CONFIG_MM_DRV_PAGE_SIZE);
		vmh_get_mapped_size(ptr, &size);
		return sys_mm_drv_unmap_region(ptr, size);
	}

	if (vmh_get_map_region_boundaries(region, ptr, size, &begin, &size))
		return sys_mm_drv_unmap_region((void *)begin, size);

	return 0;
}

/**
 * @brief Alloc function, not reentrant
 *
 * Allocates memory on heap from provided heap pointer.
 * Check if we need to map physical memory for given allocation
 * and do it when necessary.
 * @param heap pointer to a heap that will be used for allocation.
 * @param alloc_size Size in bytes to allocate.
 *
 * @retval ptr to allocated memory if successful
 * @retval NULL on allocation failure
 */
static void *_vmh_alloc(struct vmh_heap *heap, uint32_t alloc_size)
{
	if (!alloc_size)
		return NULL;

	void *ptr = NULL;
	int mem_block_iterator, allocation_error_code = -ENOMEM;
	size_t allocation_bitarray_offset, block_count = 0, block_size = 0,
		allocation_bitarray_position = 0;

	/* We will gather error code when allocating on physical block
	 * allocators.
	 * Loop will continue as long as there are blocks to iterate on and
	 * if allocation keeps returning error code.
	 * When alloc succeeded it will return 0 on allocation and we break
	 * form loop and continue.
	 * If allocating_continuously is on for heap, allocation will be made on first available
	 * block series on given block bundle. For allocating_continuously off allocation will only
	 * happened on first free block on first allocator that can accommodate
	 * requested size.
	 */
	for (mem_block_iterator = 0; mem_block_iterator < MAX_MEMORY_ALLOCATORS_COUNT;
	     mem_block_iterator++) {

		/* continiue so we do not check mem blocks that do not exist */
		if (!heap->physical_blocks_allocators[mem_block_iterator])
			continue;

		/* calculate block count needed to allocate for current
		 * mem_block.
		 */
		block_size =
		    1 << heap->physical_blocks_allocators[mem_block_iterator]->info.blk_sz_shift;
		block_count = SOF_DIV_ROUND_UP((uint64_t)alloc_size, (uint64_t)block_size);

		/* If we do not span alloc and block is smaller than alloc we try next mem_block */
		if (block_size < alloc_size && !heap->allocating_continuously)
			continue;

		if (block_count >
		    heap->physical_blocks_allocators[mem_block_iterator]->info.num_blocks)
			continue;
		/* Try span alloc on first available mem_block for non span
		 * check if block size is sufficient.
		 */
		if (heap->allocating_continuously) {
			allocation_error_code = sys_mem_blocks_alloc_contiguous(
				heap->physical_blocks_allocators[mem_block_iterator], block_count,
				&ptr);
		} else if (block_size >= alloc_size) {
			allocation_error_code = sys_mem_blocks_alloc(
				heap->physical_blocks_allocators[mem_block_iterator], block_count,
				&ptr);
		}
		/* Save allocation data for purpose of freeing correctly
		 * afterwards should be changed to a implementation in mem_blocks
		 * that keeps alloc data there.
		 */
		if (allocation_error_code == 0) {
			/* Mechanism of saving allocation size is a bit complicated and genius
			 * thought up by Adrian Warecki.
			 * This explanation is long and will be moved to documentation afterwards.
			 * Initial setup for mechanism:
			 * First bit array that saves allocated blocks
			 * in our system this will be bit array saved in mem_blocks API.
			 * Second bit array which is of exactly same size of first one
			 * that will save connection of allocation to size of allocation
			 * by marking if the bit of allocation is connected to next bit.
			 *
			 * Examples: lets have simple 16 bit representation of blocks
			 * First: 0000000000000000 allocate first four blocks 1111000000000000
			 * Second:0000000000000000                            1110000000000000
			 * The allocator saves whether block was allocated and second bit array
			 * saves information by marking in position of a bit allocated 1 if
			 * it is a start of the allocation and has more than one
			 * block allocated, 1 if following bit is also part of allocation.
			 * 0 for individually allocated block and 0 for block ending allocation
			 * Continuing the example above
			 * First: 1111100000000000 allocate another block 1111100000000000
			 * Second:1110000000000000 after the first one	  1110000000000000
			 * based on information above we know exactly how long allocations were
			 * by checking second bit array against first one
			 * First: 1111111100000000 allocate another block 1111111100000000
			 * Second:1110000000000000 and another two blocks 1110001000000000
			 * We are still able to pinpoint allocations size
			 */

			/* Set bits count - 1 on the same offset as in allocation bit array */
			allocation_bitarray_offset = (uintptr_t)ptr
					- (uintptr_t)heap->physical_blocks_allocators
						[mem_block_iterator]->buffer;
			allocation_bitarray_position = allocation_bitarray_offset / block_size;

			/* Need to set bits only if we have more than 1 block to allocate */
			if (block_count - 1)
				sys_bitarray_set_region(heap->allocation_sizes[mem_block_iterator],
					block_count - 1, allocation_bitarray_position);
			break;
		}
	}

	/* If ptr is NULL it means we did not allocate anything and we should
	 * break execution here
	 */
	if (!ptr)
		return NULL;

	allocation_error_code = vmh_map_region(heap->physical_blocks_allocators[mem_block_iterator],
					       ptr, alloc_size);
	if (allocation_error_code) {
		sys_mem_blocks_free_contiguous(heap->physical_blocks_allocators[mem_block_iterator],
					       ptr, block_count);
		return NULL;
	}

	return ptr;
}

/**
 *  @brief Alloc function, reentrant
 *	   see _vmh_alloc comment for details
 */
void *vmh_alloc(struct vmh_heap *heap, uint32_t alloc_size)
{
	k_mutex_lock(&heap->lock, K_FOREVER);

	void *ret = _vmh_alloc(heap, alloc_size);

	k_mutex_unlock(&heap->lock);
	return ret;
}

/**
 * @brief Free virtual memory heap
 *
 * Free the virtual memory heap object and its child allocations
 * @param heap Ptr to the heap that is supposed to be freed
 *
 * @retval 0 on success;
 * @retval -ENOTEMPTY on heap having active allocations.
 */
int vmh_free_heap(struct vmh_heap *heap)
{
	int i;

	for (i = 0; i < MAX_MEMORY_ALLOCATORS_COUNT; i++) {
		if (!heap->physical_blocks_allocators[i])
			continue;
		if (!sys_bitarray_is_region_cleared(heap->physical_blocks_allocators[i]->bitmap,
				heap->physical_blocks_allocators[i]->info.num_blocks, 0))
			return -ENOTEMPTY;
	}

	for (i = 0; i < MAX_MEMORY_ALLOCATORS_COUNT; i++) {
		if (heap->physical_blocks_allocators[i]) {
			rfree(heap->physical_blocks_allocators[i]->bitmap->bundles);
			rfree(heap->physical_blocks_allocators[i]->bitmap);
			rfree(heap->physical_blocks_allocators[i]);
			rfree(heap->allocation_sizes[i]->bundles);
			rfree(heap->allocation_sizes[i]);
		}
	}
	rfree(heap);
	return 0;
}

/**
 * @brief Free ptr allocated on given heap, not reentrant
 *
 * Free the ptr allocation. After free action is complete
 * check if any physical memory block can be freed up as
 * result.
 * @param heap Pointer to the heap on which ptr param is residing.
 * @param ptr pointer that should be freed
 *
 * @retval 0 on success;
 * @retval -ENOTEMPTY on heap having active allocations.
 */
static int _vmh_free(struct vmh_heap *heap, void *ptr)
{
	int retval;

	size_t mem_block_iter, i, size_to_free, block_size, ptr_bit_array_offset,
		ptr_bit_array_position, blocks_to_free;
	bool ptr_range_found;

	/* Get allocator from which ptr was allocated */
	for (mem_block_iter = 0, ptr_range_found = false;
		mem_block_iter < MAX_MEMORY_ALLOCATORS_COUNT;
		mem_block_iter++) {
		/* continiue so we do not check mem blocks that do not exist */
		if (!heap->physical_blocks_allocators[mem_block_iter])
			continue;

		block_size =
			1 << heap->physical_blocks_allocators[mem_block_iter]->info.blk_sz_shift;

		if (vmh_is_ptr_in_memory_range((uintptr_t)ptr,
				(uintptr_t)heap->physical_blocks_allocators
				[mem_block_iter]->buffer,
				heap->physical_blocks_allocators
				[mem_block_iter]->info.num_blocks * block_size)) {
			ptr_range_found = true;
			break;
		}
	}
	if (!ptr_range_found)
		return -EINVAL;

	/* Initially set size to block size */
	size_to_free = block_size;

	/* Get size of the allocation from ptr using allocation_sizes bit array
	 * and allocators bit array. Only span alloc can have different size than
	 * block_size.
	 */
	if (heap->allocating_continuously) {

		/* Not sure if that is fastest way to find the size comments welcome */
		ptr_bit_array_offset = (uintptr_t)ptr
			- (uintptr_t)heap->physical_blocks_allocators[mem_block_iter]->buffer;
		ptr_bit_array_position = ptr_bit_array_offset / block_size;

		/* Allocation bit array check */
		int bit_value, prev_bit_value = 0;

		sys_bitarray_test_bit(heap->allocation_sizes[mem_block_iter],
			ptr_bit_array_position, &bit_value);
		/* If checked bit is in position 0 we assume it is valid
		 * and assigned 0 for further logic
		 */
		if (ptr_bit_array_position)
			sys_bitarray_test_bit(heap->allocation_sizes[mem_block_iter],
				ptr_bit_array_position - 1, &prev_bit_value);

		/* If bit is 1 we know we could be at the start of the allocation,
		 * we need to test previous bit from allocation if it is set then
		 * something went wrong in calculations.
		 */
		if (prev_bit_value)
			return -EINVAL;

		if (bit_value) {
			/* Neeeeeeeds optimization - thinking how to do it properly
			 * each set bit in order after another means one allocated block.
			 * When we get to 0 in such range we know that is last allocated block.
			 * Testing bundles looks promising - need to investigate.
			 */
			for (i = ptr_bit_array_position;
				i < heap->physical_blocks_allocators
					[mem_block_iter]->info.num_blocks;
				i++) {

				sys_bitarray_test_bit(heap->allocation_sizes[mem_block_iter], i,
					&bit_value);
				if (bit_value)
					size_to_free += block_size;
				else
					break;
			}
		} else {
			/* We know that there is only one block allocated
			 * since bit related to alloc is 0
			 */
			size_to_free = block_size;
		}
		blocks_to_free = size_to_free / block_size;
		retval = sys_mem_blocks_free_contiguous(
			heap->physical_blocks_allocators[mem_block_iter], ptr,
			blocks_to_free);
		if (!retval)
			sys_bitarray_clear_region(heap->allocation_sizes[mem_block_iter],
				blocks_to_free, ptr_bit_array_position);
	} else {
		retval = sys_mem_blocks_free(heap->physical_blocks_allocators[mem_block_iter],
			1, &ptr);
	}

	if (retval)
		return retval;

	/* Platforms based on xtensa have a non-coherent cache between cores. Before releasing
	 * a memory block, it is necessary to invalidate the cache. This memory block can be
	 * allocated by another core and performing cache writeback by the previous owner will
	 * destroy current content of the main memory. The cache is invalidated by the
	 * sys_mm_drv_unmap_region function, when a memory page is unmapped. There is no need to
	 * invalidate it when releasing buffers of at least a page in size.
	 */
	if (size_to_free < CONFIG_MM_DRV_PAGE_SIZE)
		sys_cache_data_invd_range(ptr, size_to_free);

	return vmh_unmap_region(heap->physical_blocks_allocators[mem_block_iter], ptr,
				size_to_free);
}

/**
 *  @brief Free ptr function, reentrant
 *	   see _vmh_free comment for details
 */
int vmh_free(struct vmh_heap *heap, void *ptr)
{
	k_mutex_lock(&heap->lock, K_FOREVER);

	int ret = _vmh_free(heap, ptr);

	k_mutex_unlock(&heap->lock);
	return ret;
}

/**
 * @brief Get default configuration for heap
 *
 * This will return config created based on region provided.
 * It will try to split memory in even chunks and split it into
 * bundles of size 64,128,...,1024 - exactly 5 sizes.
 *
 * @param region Virtual region which resources we will be using for heap
 * @param cfg	 Ptr to the struct that function will populate with data.
 *
 */
void vmh_get_default_heap_config(const struct sys_mm_drv_region *region,
	struct vmh_heap_config *cfg)
{
	int buffer_size = ALIGN_DOWN(region->size / DEFAULT_CONFIG_ALOCATORS_COUNT,
		CONFIG_MM_DRV_PAGE_SIZE);

	int i, block_count, block_size;

	for (i = 0; i < DEFAULT_CONFIG_ALOCATORS_COUNT; i++) {
		block_size = DCACHE_LINE_SIZE << i;
		block_count = buffer_size / block_size;

		cfg->block_bundles_table[i].block_size = block_size;
		cfg->block_bundles_table[i].number_of_blocks = block_count;
	}
}

#endif /* if defined (CONFIG_MM_DRV) */
