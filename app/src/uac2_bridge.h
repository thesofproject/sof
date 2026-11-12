/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#ifndef __APP_UAC2_BRIDGE_H__
#define __APP_UAC2_BRIDGE_H__

struct sof;
struct uac2_ops;

/**
 * \brief Initialize static audio pipelines for the active Nordic platform.
 * \param[in] sof SOF context pointer.
 * \return 0 on success, negative errno on failure.
 */
int sof_static_pipelines_init(struct sof *sof);

/**
 * \brief Return the Zephyr USB Audio Class 2.0 callback table.
 * \return Pointer to static uac2_ops structure.
 */
const struct uac2_ops *sof_get_uac2_ops(void);

#endif /* __APP_UAC2_BRIDGE_H__ */
