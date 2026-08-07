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

/*
 * WOV arbiter AMS message UUIDs.
 *
 * AMS_WOV_DETECT_MSG_UUID: c3d7e841-12f0-4e8a-b901-5a6b7c8d9e0f
 *   Sent by a WOV detector (detect_test) to the WOV arbiter when a keyword
 *   is detected.  Payload: struct wov_detect_payload.
 *
 * AMS_WOV_CTRL_MSG_UUID: f1e2d3c4-b5a6-4789-8ace-1234567890ab
 *   Sent by the WOV arbiter to all WOV detectors to pause or resume
 *   detection.  Payload: struct wov_ctrl_payload.
 */
#define AMS_WOV_DETECT_MSG_UUID { 0xc3, 0xd7, 0xe8, 0x41, 0x12, 0xf0, 0x4e, \
				  0x8a, 0xb9, 0x01, 0x5a, 0x6b, 0x7c, 0x8d, \
				  0x9e, 0x0f }

#define AMS_WOV_CTRL_MSG_UUID   { 0xf1, 0xe2, 0xd3, 0xc4, 0xb5, 0xa6, 0x47, \
				  0x89, 0x8a, 0xce, 0x12, 0x34, 0x56, 0x78, \
				  0x90, 0xab }

#define WOV_SLOT_INVALID 0xff

/* Payload for AMS_WOV_DETECT_MSG_UUID (detector → arbiter) */
struct wov_detect_payload {
	uint8_t slot_id; /* detector slot: 0, 1, or 2 */
};

/* Command codes for AMS_WOV_CTRL_MSG_UUID (arbiter → detectors) */
#define WOV_CTRL_CMD_PAUSE  0
#define WOV_CTRL_CMD_RESUME 1

/* Payload for AMS_WOV_CTRL_MSG_UUID (arbiter → detectors) */
struct wov_ctrl_payload {
	uint8_t cmd;         /* WOV_CTRL_CMD_PAUSE or WOV_CTRL_CMD_RESUME */
	uint8_t active_slot; /* slot being activated (valid for PAUSE) */
};

#endif /* __SOF_LIB_AMS_MSG_H__ */
