// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <rtos/symbol.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

LOG_MODULE_REGISTER(mixin_mixout, CONFIG_SOF_LOG_LEVEL);
EXPORT_SYMBOL(log_const_mixin_mixout);

/* mixin 39656eb2-3b71-4049-8d3f-f92cd5c43c09 */
SOF_DEFINE_REG_UUID(mixin);
EXPORT_SYMBOL(mixin_uuid);

DECLARE_TR_CTX(mixin_tr, SOF_UUID(mixin_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(mixin_tr);

/* mixout 3c56505a-24d7-418f-bddc-c1f5a3ac2ae0 */
SOF_DEFINE_REG_UUID(mixout);
EXPORT_SYMBOL(mixout_uuid);

DECLARE_TR_CTX(mixout_tr, SOF_UUID(mixout_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(mixout_tr);
