/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation. All rights reserved.
 */

#ifndef _TESTBENCH_REMOTE_SERVER_H
#define _TESTBENCH_REMOTE_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "testbench/utils.h"

#if !defined(__XTENSA__) && !defined(CONFIG_XT_SIMULATOR)

int tb_remote_server_start(struct testbench_prm *tp);
void tb_remote_server_stop(void);
void tb_remote_tick_begin(struct testbench_prm *tp);
void tb_remote_tick_end(struct testbench_prm *tp, uint64_t delta_cycles, uint64_t delta_ns);
bool tb_remote_should_continue(struct testbench_prm *tp, bool file_eof);
bool tb_remote_is_tone_active(uint32_t *freq_hz);
void tb_remote_record_output_samples_s16(const int16_t *samples, size_t count,
					 int channels, int rate);
void tb_remote_record_output_samples_s32(const int32_t *samples, size_t count,
					 int channels, int rate, int shift_to_s16);

#else

static inline int tb_remote_server_start(struct testbench_prm *tp)
{
	(void)tp;
	return 0;
}

static inline void tb_remote_server_stop(void) {}

static inline void tb_remote_tick_begin(struct testbench_prm *tp)
{
	(void)tp;
}

static inline void tb_remote_tick_end(struct testbench_prm *tp, uint64_t delta_cycles,
				      uint64_t delta_ns)
{
	(void)tp;
	(void)delta_cycles;
	(void)delta_ns;
}

static inline bool tb_remote_should_continue(struct testbench_prm *tp, bool file_eof)
{
	(void)tp;
	return !file_eof;
}

static inline bool tb_remote_is_tone_active(uint32_t *freq_hz)
{
	(void)freq_hz;
	return false;
}

static inline void tb_remote_record_output_samples_s16(const int16_t *samples, size_t count,
						       int channels, int rate)
{
	(void)samples;
	(void)count;
	(void)channels;
	(void)rate;
}

static inline void tb_remote_record_output_samples_s32(const int32_t *samples, size_t count,
						       int channels, int rate, int shift_to_s16)
{
	(void)samples;
	(void)count;
	(void)channels;
	(void)rate;
	(void)shift_to_s16;
}

#endif /* !__XTENSA__ && !CONFIG_XT_SIMULATOR */

#endif /* _TESTBENCH_REMOTE_SERVER_H */
