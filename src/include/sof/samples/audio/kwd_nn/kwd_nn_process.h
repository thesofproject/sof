/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 NXP
 *
 * Author: Mihai Despotovici <mihai.despotovici@nxp.com>
 */

#ifndef __KWD_NN_PROCESS_H__
#define __KWD_NN_PROCESS_H__

#include "kwd_nn_config.h"

void kwd_nn_process_data
	(uint8_t preprocessed_data[KWD_NN_CONFIG_PREPROCESSED_SIZE],
	uint8_t confidences[KWD_NN_CONFIDENCES_SIZE]);

#endif /* __KWD_NN_PROCESS_H__ */
