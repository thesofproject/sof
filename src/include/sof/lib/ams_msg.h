/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023 Intel Corporation
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef __SOF_LIB_AMS_MSG_H__
#define __SOF_LIB_AMS_MSG_H__

/* AMS messages */
typedef uint8_t ams_uuid_t[16];

/* Key-phrase detected AMS message uuid: 80a11122-b36c-11ed-afa1-0242ac120002*/
#define AMS_KPD_MSG_UUID { 0x80, 0xa1, 0x11, 0x22, 0xb3, 0x6c, 0x11, 0xed, \
			   0xaf, 0xa1, 0x02, 0x42, 0xac, 0x12, 0x00, 0x02 }

#endif /* __SOF_LIB_AMS_MSG_H__ */
