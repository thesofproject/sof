// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <rtos/symbol.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

LOG_MODULE_REGISTER(src, CONFIG_SOF_LOG_LEVEL);
#if CONFIG_IPC_MAJOR_4
SOF_DEFINE_REG_UUID(src4);
DECLARE_TR_CTX(src_tr, SOF_UUID(src4_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(src4_uuid);
#elif CONFIG_IPC_MAJOR_3
SOF_DEFINE_REG_UUID(src);
DECLARE_TR_CTX(src_tr, SOF_UUID(src_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(src_uuid);
#endif
EXPORT_SYMBOL(src_tr);
EXPORT_SYMBOL(log_const_src);
