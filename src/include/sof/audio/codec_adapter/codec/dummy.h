/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2020 NXP
 *
 * Author: Daniel Baluta <daniel.baluta@nxp.com>
 */

#ifndef __SOF_AUDIO_DUMMY_CODEC__
#define __SOF_AUDIO_DUMMY_CODEC__

int dummy_codec_init(struct comp_dev *dev);
int dummy_codec_prepare(struct comp_dev *dev);
int dummy_codec_process(struct comp_dev *dev);
int dummy_codec_apply_config(struct comp_dev *dev);
int dummy_codec_reset(struct comp_dev *dev);
int dummy_codec_free(struct comp_dev *dev);

#endif /* __SOF_AUDIO_DUMMY_CODEC__ */
