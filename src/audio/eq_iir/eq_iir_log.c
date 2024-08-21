// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <rtos/symbol.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

LOG_MODULE_REGISTER(eq_iir, CONFIG_SOF_LOG_LEVEL);
EXPORT_SYMBOL(log_const_eq_iir);

SOF_DEFINE_REG_UUID(eq_iir);
EXPORT_SYMBOL(eq_iir_uuid);

DECLARE_TR_CTX(eq_iir_tr, SOF_UUID(eq_iir_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(eq_iir_tr);
