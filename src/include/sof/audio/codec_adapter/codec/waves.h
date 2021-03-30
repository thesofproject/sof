/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Waves Audio Ltd. All rights reserved.
 *
 * Author: Oleksandr Strelchenko <oleksandr.strelchenko@waves.com>
 */
#ifndef __SOF_AUDIO_WAVES_CODEC__
#define __SOF_AUDIO_WAVES_CODEC__

int waves_codec_init(struct comp_dev *dev);
int waves_codec_prepare(struct comp_dev *dev);
int waves_codec_process(struct comp_dev *dev);
int waves_codec_init_process(struct comp_dev *dev);
int waves_codec_apply_config(struct comp_dev *dev);
int waves_codec_reset(struct comp_dev *dev);
int waves_codec_free(struct comp_dev *dev);

#endif /* __SOF_AUDIO_WAVES_CODEC__ */
