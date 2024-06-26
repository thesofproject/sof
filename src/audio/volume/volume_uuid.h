/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Baofeng Tian <baofeng.tian@intel.com>
 */

#ifndef __SOF_AUDIO_VOLUME_UUID_H__
#define __SOF_AUDIO_VOLUME_UUID_H__

#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>

#if CONFIG_IPC_MAJOR_3
SOF_DEFINE_REG_UUID(volume);
#else
/* these ids aligns windows driver requirement to support windows driver */
SOF_DEFINE_REG_UUID(volume4);
#define volume_uuid volume4_uuid

SOF_DEFINE_REG_UUID(gain);

DECLARE_TR_CTX(gain_tr, SOF_UUID(gain_uuid), LOG_LEVEL_INFO);
#endif

DECLARE_TR_CTX(volume_tr, SOF_UUID(volume_uuid), LOG_LEVEL_INFO);

#endif /* __SOF_AUDIO_VOLUME_UUID_H__ */
