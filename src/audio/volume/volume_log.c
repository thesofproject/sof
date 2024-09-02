// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <rtos/symbol.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

LOG_MODULE_REGISTER(volume, CONFIG_SOF_LOG_LEVEL);
EXPORT_SYMBOL(log_const_volume);

#if CONFIG_IPC_MAJOR_4

SOF_DEFINE_REG_UUID(volume4);
DECLARE_TR_CTX(volume_tr, SOF_UUID(volume4_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(volume4_uuid);

SOF_DEFINE_REG_UUID(gain);
DECLARE_TR_CTX(gain_tr, SOF_UUID(gain_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(gain_uuid);

#elif CONFIG_IPC_MAJOR_3

SOF_DEFINE_REG_UUID(volume);
DECLARE_TR_CTX(volume_tr, SOF_UUID(volume_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(volume_uuid);

#endif

EXPORT_SYMBOL(gain_tr);
EXPORT_SYMBOL(volume_tr);
