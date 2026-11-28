/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_CODEC_INFO_H__
#define __SOF_AUDIO_CODEC_INFO_H__

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/**
 * \brief Encodes codec and direction information into a single 32-bit value.
 * \param codec Codec type (bits 0-7, e.g. SND_AUDIOCODEC_*)
 * \param dir Stream direction (bits 8-11, e.g. SOF_IPC_STREAM_*)
 * \return Encoded 32-bit codec capability item
 */
#define SOF_CODEC_INFO_ITEM(codec, dir) \
	(((uint32_t)(codec) & 0xffU) | (((uint32_t)(dir) & 0xfU) << 8))

/** ELF section name holding LLEXT module codec capability items. */
#define SOF_CODEC_INFO_SECTION ".sof_codec_info"

#if CONFIG_COMP_BASEFW_IPC4 && !CONFIG_LIBRARY
/**
 * \brief Add a single runtime codec capability item for a module or library.
 * \param[in] owner Opaque owner token (e.g. module interface or library module ctx).
 * \param[in] item Encoded SOF_CODEC_INFO_ITEM(codec, dir) value.
 * \return 0 on success, negative error code on failure.
 */
int sof_codec_info_add(const void *owner, uint32_t item);

/**
 * \brief Remove runtime codec capability item(s) for an owner token.
 * \param[in] owner Opaque owner token previously passed to sof_codec_info_add().
 * \param[in] item Encoded item to remove, or 0 to remove all items for @p owner.
 */
void sof_codec_info_remove(const void *owner, uint32_t item);

/**
 * \brief Add multiple runtime codec capability items for a module or library.
 * \param[in] owner Opaque owner token (e.g. module interface or library module ctx).
 * \param[in] items Array of encoded SOF_CODEC_INFO_ITEM(codec, dir) values.
 * \param[in] count Number of entries in @p items.
 * \return 0 on success, negative error code on failure.
 */
int sof_codec_info_add_items(const void *owner, const uint32_t *items, size_t count);
#else
static inline int sof_codec_info_add(const void *owner, uint32_t item)
{
	(void)owner;
	(void)item;
	return 0;
}

static inline void sof_codec_info_remove(const void *owner, uint32_t item)
{
	(void)owner;
	(void)item;
}

static inline int sof_codec_info_add_items(const void *owner, const uint32_t *items, size_t count)
{
	(void)owner;
	(void)items;
	(void)count;
	return 0;
}
#endif

#endif /* __SOF_AUDIO_CODEC_INFO_H__ */
