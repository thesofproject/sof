// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <rtos/symbol.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

LOG_MODULE_REGISTER(asrc, CONFIG_SOF_LOG_LEVEL);
#if CONFIG_IPC_MAJOR_4
SOF_DEFINE_REG_UUID(asrc4);
DECLARE_TR_CTX(asrc_tr, SOF_UUID(asrc4_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(asrc4_uuid);
#elif CONFIG_IPC_MAJOR_3
SOF_DEFINE_REG_UUID(asrc);
DECLARE_TR_CTX(asrc_tr, SOF_UUID(asrc_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(asrc_uuid);
#endif
EXPORT_SYMBOL(asrc_tr);
EXPORT_SYMBOL(log_const_asrc);
