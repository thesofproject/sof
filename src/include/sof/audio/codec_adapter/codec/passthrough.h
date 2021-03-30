/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __SOF_AUDIO_PASSTHROUGH_CODEC__
#define __SOF_AUDIO_PASSTHROUGH_CODEC__

int passthrough_codec_init(struct comp_dev *dev);
int passthrough_codec_prepare(struct comp_dev *dev);
int passthrough_codec_init_process(struct comp_dev *dev);
int passthrough_codec_process(struct comp_dev *dev);
int passthrough_codec_apply_config(struct comp_dev *dev);
int passthrough_codec_reset(struct comp_dev *dev);
int passthrough_codec_free(struct comp_dev *dev);

#endif /* __SOF_AUDIO_PASSTHROUGH_CODEC__ */
