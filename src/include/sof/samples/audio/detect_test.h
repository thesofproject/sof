/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Slawomir Blauciak <slawomir.blauciak@linux.intel.com>
 */

#ifndef __USER_DETECT_TEST_H__
#define __USER_DETECT_TEST_H__

#include <stdint.h>

/** IPC blob types */
#define SOF_DETECT_TEST_CONFIG	0
#define SOF_DETECT_TEST_MODEL	1

struct sof_detect_test_config {
	uint32_t size;

	/** synthetic system load settings */
	uint32_t load_mips;
	uint32_t load_memory_size;
	/** time in ms after which detection is activated */
	uint32_t preamble_time;

	/** activation right shift, determines the speed of activation */
	uint16_t activation_shift;

	/** sample width in bits */
	int16_t sample_width;

	/** activation threshold */
	int32_t activation_threshold;

	/** default draining size in bytes */
	uint32_t drain_req;

	/** reserved for future use */
	uint32_t reserved[1];
} __attribute__((packed));

uint16_t test_keyword_get_sample_valid_bytes(struct comp_dev *dev);

uint32_t test_keyword_get_detected(struct comp_dev *dev);
void test_keyword_set_detected(struct comp_dev *dev, uint32_t detected);

#if CONFIG_KWD_NN_SAMPLE_KEYPHRASE
const int16_t *test_keyword_get_input(struct comp_dev *dev);

int16_t test_keyword_get_input_byte(struct comp_dev *dev, uint32_t index);
int16_t test_keyword_get_input_elem(struct comp_dev *dev, uint32_t index);
int test_keyword_set_input_elem(struct comp_dev *dev, uint32_t index, int16_t val);

size_t test_keyword_get_input_size(struct comp_dev *dev);
void test_keyword_set_input_size(struct comp_dev *dev, size_t input_size);
#endif

uint32_t test_keyword_get_drain_req(struct comp_dev *dev);
void test_keyword_set_drain_req(struct comp_dev *dev, uint32_t drain_req);

void detect_test_notify(const struct comp_dev *dev);

/** used for binary blob size sanity checks */
#define SOF_DETECT_TEST_MAX_CFG_SIZE sizeof(struct sof_detect_test_config)

#endif /* __USER_DETECT_TEST_H__ */
