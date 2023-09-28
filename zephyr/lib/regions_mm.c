// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2022 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Jakub Dabek <jakub.dabek@intel.com>
 */

#include <zephyr/init.h>

#include <sof/lib/regions_mm.h>

/* list of vmh_heap objects created */
static struct list_item vmh_list;

/**
 * @brief Initialize new heap
 *
 * Initializes new heap based on region that heap is to be assigned to
 * and config. The heap size overall must be aligned to physical page
 * size.
 * @param cfg Configuration structure with block structure for the heap
 * @param memory_region_attribute A zephyr defined virtual region identifier
 * @param core_id Core id of a heap that will be created
 * @param allocating_continuously Flag deciding if heap can do contiguous allocation.
 *
 * @retval ptr to a new heap if successful.
 * @retval NULL on creation failure.
 */
struct vmh_heap *vmh_init_heap(const struct vmh_heap_config *cfg,
	int memory_region_attribute, int core_id, bool allocating_continuously)
{
	const struct sys_mm_drv_region *virtual_memory_regions =
		sys_mm_drv_query_memory_regions();
	int i;

	/* Check if we haven't created heap for this region already */
	if (vmh_get_heap_by_attribute(memory_region_attribute, core_id))
		return NULL;

	struct vmh_heap *new_heap =
		rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*new_heap));

	if (!new_heap)
		return NULL;

	new_heap->core_id = core_id;
	list_init(&new_heap->node);
	struct vmh_heap_config new_config = {0};

	/* Search for matching attribute so we place heap on right
	 * virtual region.
	 * In case of core heaps regions we count theme from 0 to max
	 * available cores
	 */
	if (memory_region_attribute == MEM_REG_ATTR_CORE_HEAP) {
		new_heap->virtual_region = &virtual_memory_regions[core_id];
	} else {
		for (i = CONFIG_MP_MAX_NUM_CPUS;
			i < CONFIG_MP_MAX_NUM_CPUS + VIRTUAL_REGION_COUNT; i++) {

			if (memory_region_attribute == virtual_memory_regions[i].attr) {
				new_heap->virtual_region = &virtual_memory_regions[i];
				break;
			}
		}
	}

	if (!new_heap->virtual_region)
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
		struct sys_mem_blocks *new_allocator = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED,
			0, SOF_MEM_CAPS_RAM, sizeof(sys_mem_blocks_t));

		if (!new_allocator)
			goto fail;

		/* Assign allocator(mem_block) to table */
		new_heap->physical_blocks_allocators[i] = new_allocator;

		/* Fill allocators data based on config and virtual region data */
		new_allocator->info.num_blocks = cfg->block_bundles_table[i].number_of_blocks;
		new_allocator->info.blk_sz_shift = ilog2(cfg->block_bundles_table[i].block_size);
		new_allocator->buffer =	(uint8_t *)new_heap->virtual_region->addr + offset;

		/* Create bit array that is a part of mem_block kept as a ptr */
		struct sys_bitarray *allocators_bitarray = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED,
			0, SOF_MEM_CAPS_RAM, sizeof(sys_bitarray_t));

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
			rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED,
				0, SOF_MEM_CAPS_RAM, sizeof(sys_bitarray_t));

		if (!allocation_sizes_bitarray)
			goto fail;

		allocation_sizes_bitarray->num_bits = allocators_bitarray->num_bits;
		allocation_sizes_bitarray->num_bundles = allocators_bitarray->num_bundles;
		new_heap->allocation_sizes[i] = allocation_sizes_bitarray;

		/* Each sys_bitarray has a ptr to bit field that has to be allocated
		 * based on its size.
		 */
		uint32_t *allocator_bitarray_bitfield =
			rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, bitfield_size);

		if (!allocator_bitarray_bitfield)
			goto fail;

		allocators_bitarray->bundles = allocator_bitarray_bitfield;

		uint32_t *sizes_bitarray_bitfield = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0,
			SOF_MEM_CAPS_RAM, bitfield_size);

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

	/* Save the new heap pointer to a linked list */
	list_item_append(&vmh_list, &new_heap->node);

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
 * @brief Alloc function
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
void *vmh_alloc(struct vmh_heap *heap, uint32_t alloc_size)
{
	if (!alloc_size)
		return NULL;
	/* Only operations on the same core are allowed */
	if (heap->core_id != cpu_get_id())
		return NULL;

	void *ptr = NULL;
	uintptr_t phys_aligned_ptr, phys_aligned_alloc_end, phys_block_ptr;
	int allocation_error_code = -ENOMEM;
	size_t i, mem_block_iterator, allocation_bitarray_offset,
		check_offset, check_size, check_position, physical_block_count,
		block_count = 0, block_size = 0, allocation_bitarray_position = 0;
	uintptr_t *ptrs_to_map = NULL;

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

		/* If we do not span alloc and block is smaller than alloc we try next mem_block */
		if (block_size < alloc_size && !heap->allocating_continuously)
			continue;
		/* calculate block count needed to allocate for current
		 * mem_block.
		 */
		block_size =
		    1 << heap->physical_blocks_allocators[mem_block_iterator]->info.blk_sz_shift;
		block_count = SOF_DIV_ROUND_UP((uint64_t)alloc_size, (uint64_t)block_size);

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
		} else if (block_size > alloc_size) {
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

	/* Now we need to check if have mapped physical page already
	 * We can do it by working out if there was any other allocation
	 * done in the allocator section that is in CONFIG_MM_DRV_PAGE_SIZE chunk.
	 * Start by getting start of allocation
	 * ptr aligned to physical pages boundary
	 */
	phys_aligned_ptr = ALIGN_DOWN((uintptr_t)ptr, CONFIG_MM_DRV_PAGE_SIZE);
	phys_aligned_alloc_end = ALIGN_UP((uintptr_t)ptr
		+ alloc_size, CONFIG_MM_DRV_PAGE_SIZE);

	/* To check if there was any allocation done before on block we need
	 * to perform check operations however
	 * we just made allocation there. We will clear it
	 * from bit array of allocator for ease of operation.
	 * RFC if someone knows a better way to handle it in bit array API.
	 */

	/* Clear bits of the allocation that was done */
	sys_bitarray_clear_region(heap->physical_blocks_allocators[mem_block_iterator]->bitmap,
		block_count, allocation_bitarray_position);

	/* Now that we cleared the allocation we just made we can check
	 * if there was any other allocation in the space that would need to be
	 * mapped. Since this API is only one that uses mapping in specified address
	 * ranges we can assume that we made mapping for the space if it has any previous
	 * allocations. We check that iterating over all physical allocation blocks
	 */
	physical_block_count = (phys_aligned_alloc_end
		- phys_aligned_ptr) / CONFIG_MM_DRV_PAGE_SIZE;

	for (i = 0; i < physical_block_count; i++) {

		phys_block_ptr = phys_aligned_ptr + i * CONFIG_MM_DRV_PAGE_SIZE;

		check_offset = phys_block_ptr
			- (uintptr_t)heap->physical_blocks_allocators[mem_block_iterator]->buffer;
		check_position = check_offset / block_size;

		check_size = CONFIG_MM_DRV_PAGE_SIZE / block_size;

		/* We are now sure that if the allocation bit array between
		 * block_realigned_ptr and block_realigned_alloc_end shows us if we already had
		 * any allocations there and by our logic if it needs mapping or not and
		 * we calculated position in bit array and size to check in bit array
		 */
		if (!sys_bitarray_is_region_cleared(
				heap->physical_blocks_allocators[mem_block_iterator]->bitmap,
				check_size, check_position))
			continue;

		/* needed in case of mid mapping failure
		 * Rewrite of the concept is needed. Instead of failure handling we
		 * need to check if there is enough physical memory available.
		 */
		if (!ptrs_to_map) {
			ptrs_to_map = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED,
				0, SOF_MEM_CAPS_RAM, sizeof(uintptr_t) * physical_block_count);
			if (!ptrs_to_map)
				goto fail;
		}

		ptrs_to_map[i] = phys_block_ptr;

		if (sys_mm_drv_map_region((void *)phys_block_ptr, (uintptr_t)NULL,
					CONFIG_MM_DRV_PAGE_SIZE, SYS_MM_MEM_PERM_RW) != 0) {
			ptrs_to_map[i] = (uintptr_t)NULL;
			goto fail;
		}
	}

	/* Set back allocation bits */
	sys_bitarray_set_region(heap->physical_blocks_allocators[mem_block_iterator]->bitmap,
		block_count, allocation_bitarray_position);
	rfree(ptrs_to_map);
	return ptr;

fail:
	if (heap->allocating_continuously)
		sys_mem_blocks_free_contiguous(
		    heap->physical_blocks_allocators[mem_block_iterator], ptr, block_count);
	else
		sys_mem_blocks_free(heap->physical_blocks_allocators[mem_block_iterator],
				    block_count, &ptr);
	if (ptrs_to_map) {
		/* If by any chance we mapped one page and couldn't map rest we need to
		 * free up those physical spaces.
		 */
		for (i = 0; i < physical_block_count; i++) {
			if (ptrs_to_map[i])
				sys_mm_drv_unmap_region((void *)ptrs_to_map[i],
					CONFIG_MM_DRV_PAGE_SIZE);
		}
		rfree(ptrs_to_map);
	}
	return NULL;
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
	list_item_del(&heap->node);
	rfree(heap);
	return 0;
}

/**
 * @brief Free ptr allocated on given heap
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
int vmh_free(struct vmh_heap *heap, void *ptr)
{
	int retval;

	if (heap->core_id != cpu_get_id())
		return -EINVAL;

	size_t mem_block_iter, i, size_to_free, block_size, ptr_bit_array_offset,
		ptr_bit_array_position, physical_block_count,
		check_offset, check_position, check_size;
	uintptr_t phys_aligned_ptr, phys_aligned_alloc_end, phys_block_ptr;
	bool ptr_range_found;

	/* Get allocator from which ptr was allocated */
	for (mem_block_iter = 0, ptr_range_found = false;
		mem_block_iter < MAX_MEMORY_ALLOCATORS_COUNT;
		mem_block_iter++) {
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
			/* We know we have more than one block was allocated so
			 * we need to find the size
			 */
			size_t bits_to_check =
				heap->physical_blocks_allocators
					[mem_block_iter]->info.num_blocks - ptr_bit_array_position;

			/* Neeeeeeeds optimization - thinking how to do it properly
			 * each set bit in order after another means one allocated block.
			 * When we get to 0 in such range we know that is last allocated block.
			 * Testing bundles looks promising - need to investigate.
			 */
			for (i = ptr_bit_array_position;
				i < bits_to_check;
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

		retval = sys_mem_blocks_free_contiguous(
			heap->physical_blocks_allocators[mem_block_iter], ptr,
			size_to_free / block_size);
	} else {
		retval = sys_mem_blocks_free(heap->physical_blocks_allocators[mem_block_iter],
			1, &ptr);
	}

	if (retval)
		return retval;

	/* Go similar route to the allocation one but
	 * this time marking phys pages that can be unmapped
	 */
	phys_aligned_ptr = ALIGN_DOWN((uintptr_t)ptr, CONFIG_MM_DRV_PAGE_SIZE);
	phys_aligned_alloc_end = ALIGN_UP((uintptr_t)ptr + size_to_free, CONFIG_MM_DRV_PAGE_SIZE);

	/* Calculate how many blocks we need to check */
	physical_block_count = (phys_aligned_alloc_end
		- phys_aligned_alloc_end) / CONFIG_MM_DRV_PAGE_SIZE;

	/* Unmap physical blocks that are not currently used
	 * we check that by looking for allocations on mem_block
	 * bitarrays
	 */
	for (i = 0; i < physical_block_count; i++) {
		phys_block_ptr = phys_aligned_ptr + i * CONFIG_MM_DRV_PAGE_SIZE;

		check_offset = phys_block_ptr
			- (uintptr_t)heap->physical_blocks_allocators[mem_block_iter]->buffer;
		check_position = check_offset / block_size;

		check_size = CONFIG_MM_DRV_PAGE_SIZE / block_size;

		if (sys_bitarray_is_region_cleared(
				heap->physical_blocks_allocators[mem_block_iter]->bitmap,
				check_size, check_offset))
			sys_mm_drv_unmap_region((void *)phys_block_ptr,
					CONFIG_MM_DRV_PAGE_SIZE);
	}

	return 0;
}

/**
 * @brief Reconfigure heap with given config
 *
 * This will destroy the heap from the pointer and recreate it
 * using provided config. Individual region attribute is the
 * "anchor" to the virtual space we use.
 *
 * @param heap Ptr to the heap that should be reconfigured
 * @param cfg heap blocks configuration
 * @param allocating_continuously allow contiguous on the reconfigured heap
 *
 * @retval heap_pointer Returns new heap pointer on success
 * @retval NULL when reconfiguration failed
 */
struct vmh_heap *vmh_reconfigure_heap(
	struct vmh_heap *heap, struct vmh_heap_config *cfg,
	int core_id, bool allocating_continuously)
{
	uint32_t region_attribute = heap->virtual_region->attr;

	if (vmh_free_heap(heap))
		return NULL;

	return vmh_init_heap(cfg, region_attribute, core_id, allocating_continuously);
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

/**
 * @brief Initialization of static objects in virtual heaps API
 *
 * This will initialize lists of allocations and physical mappings.
 *
 * @param unused Variable is unused - needed by arch to clear warning.
 *
 * @retval Returns 0 on success
 */
static int virtual_heaps_init(void)
{
	list_init(&vmh_list);
	return 0;
}

SYS_INIT(virtual_heaps_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

/**
 * @brief Gets pointer to already created heap identified by
 * provided attribute.
 *
 * @param attr Virtual memory region attribute.
 * @param core_id id of the core that heap was created for (core heap specific).
 *
 * @retval heap ptr on success
 * @retval NULL if there was no heap created fitting the attr.
 */
struct vmh_heap *vmh_get_heap_by_attribute(uint32_t attr, uint32_t core_id)
{
	struct list_item *vm_heaps_iterator;
	struct vmh_heap *retval;

	if (attr == MEM_REG_ATTR_CORE_HEAP) {
		/* if we look up cpu heap we need to match its cpuid with addr
		 * we know that regions keep cpu heaps from 0 to max so we match
		 * the region of the cpu id with lists one to find match
		 */
		const struct sys_mm_drv_region *virtual_memory_region =
			sys_mm_drv_query_memory_regions();
		/* we move ptr to cpu vmr */
		virtual_memory_region = &virtual_memory_region[core_id];

		list_for_item(vm_heaps_iterator, &vmh_list) {
			retval =
			    CONTAINER_OF(vm_heaps_iterator, struct vmh_heap, node);

			if (retval->virtual_region->addr == virtual_memory_region->addr)
				return retval;
		}
	} else {
		list_for_item(vm_heaps_iterator, &vmh_list) {
			retval =
			    CONTAINER_OF(vm_heaps_iterator, struct vmh_heap, node);
			if (retval->virtual_region->attr == attr)
				return retval;
		}
	}
	return NULL;
}
