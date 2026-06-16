/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2026 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_UAOL_H__
#define __SOF_AUDIO_UAOL_H__

#include <stdint.h>
#include <sof/lib/memory.h>

struct sof_tlv;
struct device;

#if !CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY
__cold void tlv_value_set_uaol_caps(struct sof_tlv *tuple, uint32_t type);
#endif /* !CONFIG_SOF_OS_LINUX_COMPAT_PRIORITY */

__cold int uaol_stream_id_to_hda_link_stream_id(int uaol_stream_id);

const struct device *get_uaol_zdevice(int uaol_link_id);

/* these are called from dai-zephyr.c  */

struct dai;
struct comp_dev;
struct dai_data;

int dai_get_uaol_stream_id(struct dai *dai, int *uaol_link_id, int *uaol_stream_id);

void process_uaol_feedback(struct comp_dev *dev, struct dai_data *dd);

void adjust_uaol_rate(const struct dai_data *dd, bool increase);

int uaol_dma_buffer_copy_to(struct dai_data *dd, size_t bytes);

int setup_uaol_feedback_dma(struct dai_data *dd, struct comp_dev *dev);

void uaol_free(struct dai_data *dd);

#endif /* __SOF_AUDIO_UAOL_H__ */
