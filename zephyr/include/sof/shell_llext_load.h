/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Kai Vehmanen <kai.vehmanen@linux.intel.com>
 */

/**
 * @file  shell_llext_load.h
 * @brief Shared mailbox structure for the DSP shell "llext_load" command.
 *
 * The DSP shell command and the Linux "sof-client-llext-load" driver
 * communicate through a single ADSP debug window slot identified by the
 * ADSP_DW_SLOT_LLEXT_LOAD type.  Both sides treat the first 96 bytes of the
 * slot as a struct sof_shell_llext_slot.
 *
 * Handshake protocol
 * ------------------
 *  1. DSP shell writes: magic, lib_id, name  →  state = REQUESTING
 *  2. Host driver checks state == REQUESTING, reads lib_id
 *  3. Host sets state = DMA_ACTIVE, starts HDA DMA + IPC4 load
 *  4a. On success: host writes xfer_bytes, then state = DMA_DONE
 *  4b. On failure: host writes result (errno), then state = ERROR
 *  5. DSP shell detects state change, reports result, sets state = IDLE
 *
 * The binary layout must match the Linux copy in shell-llext-shm.h.
 */

#ifndef __SOF_SHELL_LLEXT_LOAD_H__
#define __SOF_SHELL_LLEXT_LOAD_H__

#include <stdint.h>
#include <adsp_debug_window.h>  /* ADSP_DW_SLOT_LLEXT_LOAD */

/**
 * Magic placed in ->magic to indicate the DSP has initialized the slot.
 * Reuses the slot-type value so a single constant identifies both.
 */
#define SOF_SHELL_LLEXT_MAGIC           ADSP_DW_SLOT_LLEXT_LOAD

/**
 * Handshake states stored in struct sof_shell_llext_slot::state.
 * The DSP writes REQUESTING; the host writes DMA_ACTIVE, DMA_DONE or ERROR.
 */
enum sof_shell_llext_state {
	SOF_SHELL_LLEXT_IDLE       = 0, /* no request pending */
	SOF_SHELL_LLEXT_REQUESTING = 1, /* DSP ready, waiting for the host to DMA */
	SOF_SHELL_LLEXT_DMA_ACTIVE = 2, /* host: copy / DMA in progress */
	SOF_SHELL_LLEXT_DMA_DONE   = 3, /* host: library DMA + IPC load complete */
	SOF_SHELL_LLEXT_ERROR      = 4, /* host: load failed, ->result holds errno */
};

/**
 * struct sof_shell_llext_slot — placed at offset 0 of the debug window slot.
 *
 * Total size: 96 bytes (the slot is 4 KB; the remainder is unused).
 */
struct sof_shell_llext_slot {
	uint32_t magic;       /**< SOF_SHELL_LLEXT_MAGIC when valid */
	uint32_t state;       /**< enum sof_shell_llext_state */
	uint32_t lib_id;      /**< library slot [1 .. LIB_MANAGER_MAX_LIBS - 1] */
	uint32_t xfer_bytes;  /**< bytes transferred (written by host on success) */
	int32_t  result;      /**< 0 on success, negative errno on error */
	uint32_t reserved[3]; /**< pad, must be zero */
	char     name[64];    /**< filename hint, NUL-terminated, display only */
} __packed;

#endif /* __SOF_SHELL_LLEXT_LOAD_H__ */
