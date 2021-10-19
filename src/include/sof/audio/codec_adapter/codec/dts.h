// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Xperi. All rights reserved.
//
// Author: Mark Barton <mark.barton@xperi.com>
//

#ifndef __SOF_AUDIO_DTS_CODEC__
#define __SOF_AUDIO_DTS_CODEC__

int dts_codec_init(struct comp_dev *dev);
int dts_codec_prepare(struct comp_dev *dev);
int dts_codec_init_process(struct comp_dev *dev);
int dts_codec_process(struct comp_dev *dev);
int dts_codec_apply_config(struct comp_dev *dev);
int dts_codec_reset(struct comp_dev *dev);
int dts_codec_free(struct comp_dev *dev);

#endif /* __SOF_AUDIO_DTS_CODEC__ */
