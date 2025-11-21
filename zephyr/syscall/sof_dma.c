// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <sof/lib/dma.h>
#include <zephyr/kernel.h>
#include <zephyr/internal/syscall_handler.h>

#ifdef CONFIG_USERSPACE

static inline bool sof_dma_has_access(struct sof_dma *dma)
{
	/*
	 * use the Zephyr dma.h device handle to check calling
	 * thread has access to it
	 */
	return k_object_is_valid(dma->z_dev, K_OBJ_ANY);
}

static inline bool sof_dma_is_valid(struct sof_dma *dma)
{
	const struct dma_info *info = dma_info_get();
	struct sof_dma *dma_ko = NULL, *d;

	if (!info->num_dmas)
		return false;

	for (d = info->dma_array; d < info->dma_array + info->num_dmas; d++) {
		if (d == dma) {
			dma_ko = d;
			break;
		}
	}

	if (dma_ko && sof_dma_has_access(dma))
		return true;

	return false;
}

static inline struct sof_dma *z_vrfy_sof_dma_get(uint32_t dir, uint32_t cap,
						 uint32_t dev, uint32_t flags)
{
	struct sof_dma *dma = z_impl_sof_dma_get(dir, cap, dev, flags);

	if (sof_dma_has_access(dma))
		return dma;

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
	struct dma_block_config *kern_prev = NULL, *kern_next = kern_cfg, *user_next = cfg->head_block;
	int i = 0;

	if (!kern_cfg)
		return NULL;

	while (user_next) {
		K_OOPS(++i > cfg->block_count);
		K_OOPS(k_usermode_from_copy(kern_next, user_next, sizeof(*kern_next)));

		/* check access permissions for DMA src/dest memory */
		switch (cfg->channel_direction) {
		case MEMORY_TO_MEMORY:
			K_OOPS(K_SYSCALL_MEMORY_WRITE((void *)kern_next->dest_address,
						      kern_next->block_size));
			/* fallthrough */
		case MEMORY_TO_PERIPHERAL:
		case MEMORY_TO_HOST:
			K_OOPS(K_SYSCALL_MEMORY_READ((void *)kern_next->source_address,
						     kern_next->block_size));
			break;
		case PERIPHERAL_TO_MEMORY:
		case HOST_TO_MEMORY:
			K_OOPS(K_SYSCALL_MEMORY_WRITE((void *)kern_next->dest_address,
						      kern_next->block_size));
			break;
		default:
			rfree(kern_cfg);
			return NULL;
		}

		if (kern_prev)
			kern_prev->next_block = kern_next;

		kern_prev = kern_next;

		++kern_next;
		user_next = user_next->next_block;
	}

	/* set transfer list to point to first kernel transfer config object */
	cfg->head_block = kern_cfg;

	return kern_cfg;
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

#endif /* CONFIG_USERSPACE */
