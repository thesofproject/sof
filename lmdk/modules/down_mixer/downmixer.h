/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: 
 * 
 */

#ifndef DOWNMIXER_MODULE_H_
#define DOWNMIXER_MODULE_H_

#include <stddef.h>

//#include "loadable_processing_module.h"
//#include "build/module_design_config.h"
#include <module/interface.h>

#include "downmixer_config.h"

#define INPUT_NUMBER 2
#define OUTPUT_NUMBER 1

#define PROCESS_SUCCEED 0
#define INVALID_IN_BUFFERS_SIZE 1

struct module_self_data {
	/* Indicates the bits per audio sample in the input streams and to produce in the output
	 *stream */
	size_t bits_per_sample_;

	/* Indicates the count of channels on the input pin 0. */
	size_t input0_channels_count_;

	/* Indicates the count of channels on the input pin 1. It can be worth 0 if the input pin 1
	 * has not been configued.
	 * If so, any audio samples reaching the input pin 1 will be discarded. */
	size_t input1_channels_count_;

	/* Indicates the count of channels on the output pin. */
	size_t output_channels_count_;

	/* Indicates tCurrent active configuration */
	struct downmixer_config config_;

	/* current processing mode */
	enum module_processing_mode processing_mode_;
};


#endif // DOWNMIXER_MODULE_H_