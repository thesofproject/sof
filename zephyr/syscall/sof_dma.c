// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/lib/dma.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

#ifdef CONFIG_SOF_USERSPACE_INTERFACE_DMA

static inline bool sof_dma_has_access(struct sof_dma *dma)
{
	/*
	 * use the Zephyr dma.h device handle to check calling
	 * thread has access to it
	 */
	return k_object_is_valid(dma->z_dev, K_OBJ_DRIVER_DMA);
}

static inline bool sof_dma_is_valid(struct sof_dma *dma)
{
	const struct dma_info *info = dma_info_get();
	uintptr_t offset = (uintptr_t)dma - (uintptr_t)info->dma_array;
	struct sof_dma *array_end = info->dma_array + info->num_dmas;

	if (!info->num_dmas)
		return false;

	/*
	 * The 'dma' pointer is not trusted, so we must first ensure it
	 * points to a valid "struct sof_dma *" kernel object.
	 */
	if (dma < info->dma_array || dma >= array_end || offset % sizeof(struct sof_dma))
		return false;

	return sof_dma_has_access(dma);
}

static inline struct sof_dma *z_vrfy_sof_dma_get(uint32_t dir, uint32_t cap,
						 uint32_t dev, uint32_t flags)
{
	struct sof_dma *dma = z_impl_sof_dma_get(dir, cap, dev, flags);

	/*
	 * note: Usually validation is done first, but here
	 * z_impl_sof_dma_get() is first called with unvalidated input
	 * arguments on purpose. This is done to reuse the existing SOF
	 * code to track DMA kernel objects. When called from
	 * user-space, we use existing functionality to look up the
	 * kernel object, but add an extra layer to check for access
	 * permission.
	 */
	if (dma) {
		if (sof_dma_has_access(dma))
			return dma;

		/* no access, release reference on error */
		z_impl_sof_dma_put(dma);
	}

	return NULL;
}
#include <zephyr/syscalls/sof_dma_get_mrsh.c>

static inline void z_vrfy_sof_dma_put(struct sof_dma *dma)
{
	K_OOPS(!sof_dma_is_valid(dma));

	z_impl_sof_dma_put(dma);
}
#include <zephyr/syscalls/sof_dma_put_mrsh.c>

static inline int z_vrfy_sof_dma_get_attribute(struct sof_dma *dma, uint32_t type, uint32_t *value)
{
	K_OOPS(!sof_dma_is_valid(dma));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(value, sizeof(*value)));

	return z_impl_sof_dma_get_attribute(dma, type, value);
}
#include <zephyr/syscalls/sof_dma_get_attribute_mrsh.c>

static inline int z_vrfy_sof_dma_request_channel(struct sof_dma *dma, uint32_t stream_tag)
{
	K_OOPS(!sof_dma_is_valid(dma));

	return z_impl_sof_dma_request_channel(dma, stream_tag);
}
#include <zephyr/syscalls/sof_dma_request_channel_mrsh.c>

static inline void z_vrfy_sof_dma_release_channel(struct sof_dma *dma,
				    uint32_t channel)
{
	K_OOPS(!sof_dma_is_valid(dma));

	return z_impl_sof_dma_release_channel(dma, channel);
}
#include <zephyr/syscalls/sof_dma_release_channel_mrsh.c>

/**
 * Creates a deep copy of the DMA transfer blocks in kernel address space,
 * based on the DMA config description given as argument.
 *
 * All pointers in 'cfg' are validated for access permission, and if
 * ok, contents is copied to a kernel side object.
 *
 * @arg cfg kernel object for DMA configuration that contains
 *      user-space pointers to DMA transfer objects
 * @return array of kernel DMA block/transfer config objects
 */
static inline struct dma_block_config *deep_copy_dma_blk_cfg_list(struct dma_config *cfg)
{
	struct dma_block_config *kern_cfg =
		rmalloc(0, sizeof(*kern_cfg) * cfg->block_count);
	struct dma_block_config *kern_prev = NULL, *kern_next, *user_next;
	int i = 0;

	if (!kern_cfg)
		return NULL;

	for (user_next = cfg->head_block, kern_next = kern_cfg;
	     user_next;
	     user_next = user_next->next_block, kern_next++, i++) {
		if (i == cfg->block_count) {
			/* last block can point to first one */
			if (user_next != cfg->head_block)
				goto err;

			kern_prev->next_block = kern_cfg;
			break;
		}

		if (k_usermode_from_copy(kern_next, user_next, sizeof(*kern_next)))
			goto err;

		/* check access permissions for DMA src/dest memory */
		switch (cfg->channel_direction) {
		case MEMORY_TO_MEMORY:
			if (K_SYSCALL_MEMORY_WRITE((void *)kern_next->dest_address,
						   kern_next->block_size))
				goto err;
			COMPILER_FALLTHROUGH;
		case MEMORY_TO_PERIPHERAL:
		case MEMORY_TO_HOST:
			if (K_SYSCALL_MEMORY_READ((void *)kern_next->source_address,
						  kern_next->block_size))
				goto err;
			break;
		case PERIPHERAL_TO_MEMORY:
		case HOST_TO_MEMORY:
			if (K_SYSCALL_MEMORY_WRITE((void *)kern_next->dest_address,
						   kern_next->block_size))
				goto err;
			break;
		default:
			goto err;
		}

		if (kern_prev)
			kern_prev->next_block = kern_next;

		kern_prev = kern_next;
	}

	/* set transfer list to point to first kernel transfer config object */
	cfg->head_block = kern_cfg;

	return kern_cfg;

err:
	/* do not call K_OOPS until kernel memory is freed */
	rfree(kern_cfg);
	return NULL;
}

static inline int z_vrfy_sof_dma_config(struct sof_dma *dma, uint32_t channel,
					struct dma_config *config)
{
	struct dma_block_config *blk_cfgs;
	struct dma_config kern_cfg, user_cfg;
	int ret;

	K_OOPS(!sof_dma_is_valid(dma));
	K_OOPS(k_usermode_from_copy(&user_cfg, config, sizeof(user_cfg)));

	/* use only DMA config attributes that are safe to use from user-space */
	kern_cfg.dma_slot = user_cfg.dma_slot;
	kern_cfg.channel_direction = user_cfg.channel_direction;
	kern_cfg.cyclic = user_cfg.cyclic;
	kern_cfg.source_data_size = user_cfg.source_data_size;
	kern_cfg.dest_data_size = user_cfg.dest_data_size;
	kern_cfg.source_burst_length = user_cfg.source_burst_length;
	kern_cfg.dest_burst_length = user_cfg.dest_burst_length;
	kern_cfg.block_count = user_cfg.block_count;
	kern_cfg.head_block = user_cfg.head_block;

	/* validate and copy transfer blocks to kernel */
	blk_cfgs = deep_copy_dma_blk_cfg_list(&kern_cfg);
	K_OOPS(blk_cfgs == NULL);

	/* TODO: add checks for peripheral/host FIFO access? */

	ret = z_impl_sof_dma_config(dma, channel, &kern_cfg);

	rfree(blk_cfgs);

	return ret;
}
#include <zephyr/syscalls/sof_dma_config_mrsh.c>

static inline int z_vrfy_sof_dma_start(struct sof_dma *dma, uint32_t channel)
{
	K_OOPS(!sof_dma_is_valid(dma));

	return z_impl_sof_dma_start(dma, channel);
}
#include <zephyr/syscalls/sof_dma_start_mrsh.c>

static inline int z_vrfy_sof_dma_stop(struct sof_dma *dma, uint32_t channel)
{
	K_OOPS(!sof_dma_is_valid(dma));

	return z_impl_sof_dma_stop(dma, channel);
}
#include <zephyr/syscalls/sof_dma_stop_mrsh.c>

static inline int z_vrfy_sof_dma_get_status(struct sof_dma *dma, uint32_t channel,
					     struct dma_status *stat)
{
	K_OOPS(!sof_dma_is_valid(dma));
	K_OOPS(K_SYSCALL_MEMORY_WRITE(stat, sizeof(*stat)));

	return z_impl_sof_dma_get_status(dma, channel, stat);
}
#include <zephyr/syscalls/sof_dma_get_status_mrsh.c>

static inline int z_vrfy_sof_dma_reload(struct sof_dma *dma, uint32_t channel, size_t size)
{
	K_OOPS(!sof_dma_is_valid(dma));

	return z_impl_sof_dma_reload(dma, channel, size);
}
#include <zephyr/syscalls/sof_dma_reload_mrsh.c>

static inline int z_vrfy_sof_dma_suspend(struct sof_dma *dma, uint32_t channel)
{
	K_OOPS(!sof_dma_is_valid(dma));

	return z_impl_sof_dma_suspend(dma, channel);
}
#include <zephyr/syscalls/sof_dma_suspend_mrsh.c>

static inline int z_vrfy_sof_dma_resume(struct sof_dma *dma, uint32_t channel)
{
	K_OOPS(!sof_dma_is_valid(dma));

	return z_impl_sof_dma_resume(dma, channel);
}
#include <zephyr/syscalls/sof_dma_resume_mrsh.c>

#endif /* CONFIG_SOF_USERSPACE_INTERFACE_DMA */
