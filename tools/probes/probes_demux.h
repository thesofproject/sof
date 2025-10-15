// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Jyri Sarha <jyri.sarha@intel.com>
//

#ifndef _PROBES_DEMUX_H_
#define _PROBES_DEMUX_H_

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

struct dma_frame_parser;

struct dma_frame_parser *parser_init(void);

void parser_log_to_stdout(struct dma_frame_parser *p);

void parser_free(struct dma_frame_parser *p);

void parser_fetch_free_buffer(struct dma_frame_parser *p, uint8_t **d, size_t *len);

int parser_parse_data(struct dma_frame_parser *p, size_t d_len);

void finalize_wave_files(struct dma_frame_parser *p);

#endif
