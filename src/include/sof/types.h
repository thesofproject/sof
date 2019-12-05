/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 NXP
 *
 * Author: Paul Olaru <paul.olaru@nxp.com>
 */
#ifndef __SOF_TYPES_H__
#define __SOF_TYPES_H__
#include <stdint.h>

struct comp_buffer;
struct comp_dev;
struct dai;
struct dai_config;
struct dai_hw_params;
struct dma;
struct dma_copy;
struct dma_sg_config;
struct dma_sg_elem_array;
struct dma_trace_data;
struct ipc;
struct ll_schedule_domain;
struct pipeline;
struct sa;
struct sof;
struct sof_ipc_buffer;
struct sof_ipc_comp;
struct sof_ipc_comp_event;
struct sof_ipc_ctrl_value_chan;
struct sof_ipc_dai_config;
struct sof_ipc_host_buffer;
struct sof_ipc_pcm_params;
struct sof_ipc_pipe_comp_connect;
struct sof_ipc_pipe_new;
struct sof_ipc_stream_posn;
struct sof_ipc_vorbis_params;
struct task;
struct timer;

/* Highly specific types which still aren't conditionally compiled;
 * their uses may however be conditionally compiled.
 */
struct sof_eq_fir_coef_data;
struct sof_eq_iir_header_df2t;

/* Arch specific */
struct _spinlock_t;

/* Typedefs */
typedef struct _spinlock_t spinlock_t;

#endif /* __SOF_TYPES_H__ */
