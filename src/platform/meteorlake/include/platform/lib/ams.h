/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifdef __SOF_LIB_AMS_H__

#ifndef __PLATFORM_LIB_AMS_H__
#define __PLATFORM_LIB_AMS_H__

/* max number of message UUIDs */
#define AMS_SERVICE_UUID_TABLE_SIZE 16
/* max number of async message routes */
#define AMS_ROUTING_TABLE_SIZE 16
/* Space allocated for async message content*/
#define AMS_MAX_MSG_SIZE 0x1000

#endif /* __PLATFORM_LIB_AMS_H__ */

#else

#error "This file shouldn't be included from outside of sof/lib/ams.h"

#endif /* __SOF_LIB_AMS_H__ */
