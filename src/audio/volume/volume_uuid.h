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
/* b77e677e-5ff4-4188-af14-fba8bdbf8682 */
SOF_DEFINE_UUID("volume", volume_uuid, 0xb77e677e, 0x5ff4, 0x4188,
		    0xaf, 0x14, 0xfb, 0xa8, 0xbd, 0xbf, 0x86, 0x82);
#else
/* these ids aligns windows driver requirement to support windows driver */
/* 8a171323-94a3-4e1d-afe9-fe5dbaa4c393 */
SOF_DEFINE_UUID("volume4", volume4_uuid, 0x8a171323, 0x94a3, 0x4e1d,
		    0xaf, 0xe9, 0xfe, 0x5d, 0xba, 0xa4, 0xc3, 0x93);
#define volume_uuid volume4_uuid

/* 61bca9a8-18d0-4a18-8e7b-2639219804b7 */
SOF_DEFINE_UUID("gain", gain_uuid, 0x61bca9a8, 0x18d0, 0x4a18,
		    0x8e, 0x7b, 0x26, 0x39, 0x21, 0x98, 0x04, 0xb7);

DECLARE_TR_CTX(gain_tr, SOF_UUID(gain_uuid), LOG_LEVEL_INFO);
#endif

DECLARE_TR_CTX(volume_tr, SOF_UUID(volume_uuid), LOG_LEVEL_INFO);

#endif /* __SOF_AUDIO_VOLUME_UUID_H__ */
