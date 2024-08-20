// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <rtos/symbol.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

SOF_DEFINE_REG_UUID(drc);
LOG_MODULE_REGISTER(drc, CONFIG_SOF_LOG_LEVEL);
DECLARE_TR_CTX(drc_tr, SOF_UUID(drc_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(drc_tr);
EXPORT_SYMBOL(drc_uuid);
EXPORT_SYMBOL(log_const_drc);
