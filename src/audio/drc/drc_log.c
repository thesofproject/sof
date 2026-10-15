// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <rtos/symbol.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#if CONFIG_COMP_DRC_MODULE
SOF_DEFINE_REG_UUID(drc);
#else
extern const struct sof_uuid_entry _drc_uuid;
extern const struct sof_uuid drc_uuid;
#endif
LOG_MODULE_REGISTER(drc, CONFIG_SOF_LOG_LEVEL);
/* unused with Zephyr, generates no output */
DECLARE_TR_CTX(drc_tr, SOF_UUID(drc_uuid), LOG_LEVEL_INFO);
EXPORT_SYMBOL(log_const_drc);
#ifdef CONFIG_LOG_RUNTIME_FILTERING
EXPORT_SYMBOL(log_dynamic_drc);
#endif
