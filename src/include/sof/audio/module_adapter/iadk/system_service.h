/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2022 - 2024 Intel Corporation. All rights reserved.
 */
/*! \file system_service.h */

#ifndef _ADSP_SYSTEM_SERVICE_H_
#define _ADSP_SYSTEM_SERVICE_H_

#include <stdint.h>
#include "adsp_stddef.h"
#include <module/iadk/adsp_error_code.h>
#include <native_system_service.h>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextern-c-compat"
#endif //__clang__

#ifdef __cplusplus
namespace intel_adsp
{
typedef struct system_service AdspSystemService;

/*! \brief Alias type of AdspSystemService which can be used in C++.
 */
struct SystemService : public AdspSystemService {};
}
#endif

#ifdef __clang__
#pragma clang diagnostic pop // ignored "-Wextern-c-compat"
#endif //__clang__

#endif /* _ADSP_SYSTEM_SERVICE_H_ */
