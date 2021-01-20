/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifndef __SOF_AUDIO_CODEC_ADAPTER__
#define __SOF_AUDIO_CODEC_ADAPTER__

#include <sof/audio/codec_adapter/codec/generic.h>

#define MAX_BLOB_SIZE 8192
/*****************************************************************************/
/* Codec adapter internal functions							     */
/*****************************************************************************/
static int load_setup_config(struct comp_dev *dev, void *cfg, uint32_t size);

#endif /* __SOF_AUDIO_CODEC_ADAPTER__ */
