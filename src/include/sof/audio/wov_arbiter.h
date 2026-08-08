/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_AUDIO_WOV_ARBITER_H__
#define __SOF_AUDIO_WOV_ARBITER_H__

/* Maximum number of WOV detector slots (= KPB host-sink input pins). */
#define WOV_ARB_MAX_SLOTS 8

/* Sentinel: no slot is currently draining. */
#define WOV_ARB_NO_ACTIVE 0xff

/*
 * IPC4 SET_LARGE_CONFIG param ID used to set the arbiter's active input
 * explicitly from the host (testing / override).
 */
#define IPC4_WOV_ARB_SET_ACTIVE_SLOT 1

/*
 * IPC4 GET_LARGE_CONFIG param ID: read the currently active slot.
 * Host uses this for the volatile RO kcontrol; returns WOV_ARB_NO_ACTIVE
 * when no drain is in progress.
 */
#define IPC4_WOV_ARB_GET_ACTIVE_SLOT 2

/* Command codes for WOV_CTRL notifier payload (arbiter → detectors). */
#define WOV_ARB_CMD_PAUSE  0
#define WOV_ARB_CMD_RESUME 1

/* WOV detect notifier payload (detector → arbiter). */
struct wov_detect_notif {
	uint8_t slot_id; /* 0-based detector slot that fired */
};

/* WOV ctrl notifier payload (arbiter → detectors). */
struct wov_ctrl_notif {
	uint8_t cmd;     /* WOV_ARB_CMD_PAUSE or WOV_ARB_CMD_RESUME */
	uint8_t slot_id; /* winning slot; receivers skip if this is their own slot */
};

/* Sentinel value matching WOV_SLOT_INVALID in ams_msg.h */
#define WOV_SLOT_INVALID 0xff

#ifdef UNIT_TEST
void sys_comp_wov_arbiter_init(void);
#endif

#endif /* __SOF_AUDIO_WOV_ARBITER_H__ */
