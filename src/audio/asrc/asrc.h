/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 */

#ifndef __SOF_ASRC_H__
#define __SOF_ASRC_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/component.h>
#include "asrc_farrow.h"

struct comp_data;
#ifdef CONFIG_IPC_MAJOR_4

#include <ipc4/base-config.h>
#include "asrc_ipc4.h"

typedef struct ipc4_asrc_module_cfg ipc_asrc_cfg;

static inline uint32_t asrc_get_source_rate(const struct ipc4_asrc_module_cfg *ipc_asrc)
{
	return ipc_asrc->base.audio_fmt.sampling_frequency;
}

static inline uint32_t asrc_get_sink_rate(const struct ipc4_asrc_module_cfg *ipc_asrc)
{
	return ipc_asrc->out_freq;
}

static inline uint32_t asrc_get_operation_mode(const struct ipc4_asrc_module_cfg *ipc_asrc)
{
	return ipc_asrc->asrc_mode & (1 << IPC4_MOD_ASRC_PUSH_MODE) ? ASRC_OM_PUSH : ASRC_OM_PULL;
}

static inline bool asrc_get_asynchronous_mode(const struct ipc4_asrc_module_cfg *ipc_asrc)
{
	return false;
}

#else /*IPC3 version*/
typedef struct ipc_config_asrc ipc_asrc_cfg;

static inline uint32_t asrc_get_source_rate(const struct ipc_config_asrc *ipc_asrc)
{
	return ipc_asrc->source_rate;
}

static inline uint32_t asrc_get_sink_rate(const struct ipc_config_asrc *ipc_asrc)
{
	return ipc_asrc->sink_rate;
}

static inline uint32_t asrc_get_operation_mode(const struct ipc_config_asrc *ipc_asrc)
{
	return ipc_asrc->operation_mode;
}

static inline bool asrc_get_asynchronous_mode(const struct ipc_config_asrc *ipc_asrc)
{
	return ipc_asrc->asynchronous_mode;
}

#endif

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
int asrc_dai_get_timestamp(struct comp_data *cd, struct dai_ts_data *tsd);
#else
int asrc_dai_get_timestamp(struct comp_data *cd, struct timestamp_data *tsd);
#endif
/* Simple count value to prevent first delta timestamp
 * from being input to low-pass filter.
 */
#define TS_STABLE_DIFF_COUNT	2

/* Low pass filter coefficient for measured drift factor,
 * The low pass function is y(n) = c1 * x(n) + c2 * y(n -1)
 * coefficient c2 needs to be 1 - c1.
 */
#define COEF_C1		Q_CONVERT_FLOAT(0.01, 30)
#define COEF_C2		Q_CONVERT_FLOAT(0.99, 30)

typedef void (*asrc_proc_func)(struct processing_module *mod,
			       const struct audio_stream *source,
			       struct audio_stream *sink,
			       int *consumed,
			       int *produced);
/* asrc component private data */
struct comp_data {
	ipc_asrc_cfg ipc_config;
	struct asrc_farrow *asrc_obj;	/* ASRC core data */
	struct comp_dev *dai_dev;	/* Associated DAI component */
	enum asrc_operation_mode mode;  /* Control for push or pull mode */
	uint64_t ts;
	uint32_t sink_rate;	/* Sample rate in Hz */
	uint32_t source_rate;	/* Sample rate in Hz */
	uint32_t sink_format;	/* For used PCM sample format */
	uint32_t source_format;	/* For used PCM sample format */
	uint32_t copy_count;	/* Count copy() operations  */
	int32_t ts_prev;
	int32_t sample_prev;
	int32_t skew;		/* Rate factor in Q2.30 */
	int32_t skew_min;
	int32_t skew_max;
	int ts_count;
	int asrc_size;		/* ASRC object size */
	int buf_size;		/* Samples buffer size */
	int frames;		/* IO buffer length */
	int source_frames;	/* Nominal # of frames to process at source */
	int sink_frames;	/* Nominal # of frames to process at sink */
	int source_frames_max;	/* Max # of frames to process at source */
	int sink_frames_max;	/* Max # of frames to process at sink */
	int data_shift;		/* Optional shift by 8 to process S24_4LE */
	uint8_t *buf;		/* Samples buffer for input and output */
	uint8_t *ibuf[PLATFORM_MAX_CHANNELS];	/* Input channels pointers */
	uint8_t *obuf[PLATFORM_MAX_CHANNELS];	/* Output channels pointers */
	bool track_drift;
	asrc_proc_func asrc_func;		/* ASRC processing function */
};

int asrc_dai_configure_timestamp(struct comp_data *cd);
int asrc_dai_start_timestamp(struct comp_data *cd);
int asrc_dai_stop_timestamp(struct comp_data *cd);
void asrc_update_buffer_format(struct comp_buffer *buf_c, struct comp_data *cd);
void asrc_set_stream_params(struct comp_data *cd, struct sof_ipc_stream_params *params);
extern struct tr_ctx asrc_tr;

/* Different UUID names for different build configurations... */
#ifdef CONFIG_IPC_MAJOR_4
#define ASRC_UUID asrc4_uuid
#else
#define ASRC_UUID asrc_uuid
#endif
extern const struct sof_uuid ASRC_UUID;

#endif
