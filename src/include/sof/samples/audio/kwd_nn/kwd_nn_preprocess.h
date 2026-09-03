/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright 2021 NXP
 *
 * Author: Mihai Despotovici <mihai.despotovici@nxp.com>
 */

#ifndef __KWD_NN_PREPROCESS_H__
#define __KWD_NN_PREPROCESS_H__

#include <stdint.h>

int kwd_nn_preprocess(const int16_t *input, uint8_t *output);

void kwd_nn_preprocess_1s(const int16_t *raw_data, uint8_t *preprocessed_data);

#endif /* __KWD_NN_PREPROCESS_H__ */
