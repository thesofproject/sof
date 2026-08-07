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

#ifdef UNIT_TEST
void sys_comp_wov_arbiter_init(void);
#endif

#endif /* __SOF_AUDIO_WOV_ARBITER_H__ */
