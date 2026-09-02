/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 *
 * DRC configuration blobs for the process unit test. Generated from
 * tools/ctl/ipc4/drc/{passthrough,speaker_default}.txt (see
 * src/audio/drc/tune/sof_example_drc.m).
 */

#ifndef SOF_ZTEST_DRC_TEST_COEF_H
#define SOF_ZTEST_DRC_TEST_COEF_H

#include <stdint.h>

/** DRC configuration with processing disabled. */
static const uint32_t drc_coef_pass_2ch[35] = {
	0x00464f53, 0x00000000, 0x0000006c, 0x03013000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000006c, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0xe8000000, 0x1e000000,
	0x0c000000, 0x00624dd3, 0x0409c2b1, 0x05555555,
	0x001efa50, 0x00946055, 0xff6a987e, 0x01fec983,
	0x22474764, 0x01745617, 0x0071c71c, 0xff777777,
	0x001f77d8, 0x00000005, 0x00438000, 0x00047dd7,
	0x0025cea0, 0x00097dd7, 0x0000b5b1
};

/** DRC configuration with the small-speaker processing curve enabled. */
static const uint32_t drc_coef_enabled_2ch[35] = {
	0x00464f53, 0x00000000, 0x0000006c, 0x03013000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000006c, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000001, 0xe2000000, 0x14000000,
	0x0a000000, 0x00624dd3, 0x02061b8a, 0x06666666,
	0x00ba972f, 0x001e0c18, 0xffe04220, 0x0050f44e,
	0x08349f9a, 0x04d82cd3, 0x0071c71c, 0xff777777,
	0x001f77d8, 0x00000005, 0x00438000, 0x00047dd7,
	0x0025cea0, 0x00097dd7, 0x0000b5b1
};

#endif /* SOF_ZTEST_DRC_TEST_COEF_H */
