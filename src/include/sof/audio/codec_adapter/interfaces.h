/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 *
 * \file interfaces.h
 * \brief Description of supported codecs
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */
#ifndef __SOF_AUDIO_CODEC_INTERFACES__
#define __SOF_AUDIO_CODEC_INTERFACES__

#if CONFIG_CADENCE_CODEC
#include <sof/audio/codec_adapter/codec/cadence.h>
#endif /* CONFIG_CADENCE_CODEC */

#if CONFIG_DUMMY_CODEC
#include <sof/audio/codec_adapter/codec/dummy.h>
#endif

#define CADENCE_ID 0xCADE01
#define DUMMY_ID   0xD03311

/*****************************************************************************/
/* Linked codecs interfaces						     */
/*****************************************************************************/
static struct codec_interface interfaces[] = {
#if CONFIG_CADENCE_CODEC
	{
		.id = CADENCE_ID, /**< Cadence interface */
		.init  = cadence_codec_init,
		.prepare = cadence_codec_prepare,
		.process = cadence_codec_process,
		.apply_config = cadence_codec_apply_config,
		.reset = cadence_codec_reset,
		.free = cadence_codec_free
	},
#endif /* CONFIG_CADENCE_CODEC */

#ifdef CONFIG_DUMMY_CODEC
	{
		.id = DUMMY_ID, /** dummy interface */
		.init  = dummy_codec_init,
		.prepare = dummy_codec_prepare,
		.process = dummy_codec_process,
		.apply_config = dummy_codec_apply_config,
		.reset = dummy_codec_reset,
		.free = dummy_codec_free
	},
#endif /* CONFIG_DUMMY_CODEC */
};

#endif /* __SOF_AUDIO_CODEC_INTERFACES__ */
