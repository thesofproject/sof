// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/dai-zephyr.h>
#include <zephyr/device.h>

/* UUID External Declarations */
extern const struct sof_uuid usb_audio_uuid;
extern const struct sof_uuid volume_uuid;
extern const struct sof_uuid dai_uuid;

/* UAC2 Device Tree Entity IDs */
#if DT_NODE_EXISTS(DT_NODELABEL(i2s_fu))
#define UAC2_STATIC_ENTITY_ID(node) (DT_NODE_CHILD_IDX(node) + 1)
#define PLAYBACK_FU_ID       UAC2_STATIC_ENTITY_ID(DT_NODELABEL(i2s_fu))
#define CAPTURE_FU_ID        UAC2_STATIC_ENTITY_ID(DT_NODELABEL(i2s_in_fu))
#define PLAYBACK_TERM_ID     UAC2_STATIC_ENTITY_ID(DT_NODELABEL(i2s_out_terminal))
#define CAPTURE_TERM_ID      UAC2_STATIC_ENTITY_ID(DT_NODELABEL(i2s_in_terminal))
#else
#define PLAYBACK_FU_ID       1
#define CAPTURE_FU_ID        2
#define PLAYBACK_TERM_ID     1
#define CAPTURE_TERM_ID      2
#endif

/* -------------------------------------------------------------------------
 * 1. Component / Module Declarations
 * ------------------------------------------------------------------------- */
static const struct sof_static_comp esp32s3_comps[] = {
	/* --- Playback Pipeline (Pipeline 1) --- */
	SOF_STATIC_COMP_HOST(
		.id = 1, .pipeline_id = 1, .name = "USB_PB",
		.uuid = &usb_audio_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2),
		.ep.usb.terminal_id = PLAYBACK_TERM_ID
	),
	SOF_STATIC_COMP_MODULE(
		.id = 2, .pipeline_id = 1, .name = "VOL_PB",
		.uuid = &volume_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2)
	),
	SOF_STATIC_COMP_DAI(
		.id = 3, .pipeline_id = 1, .name = "DAI_I2S_PB",
		.uuid = &dai_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2),
		.ep.dai.dai_type = SOF_DAI_ESP32_I2S,
		.ep.dai.dai_index = 0,
		.ep.dai.format = SOF_DAI_FMT_I2S
	),

	/* --- Capture Pipeline (Pipeline 2) --- */
	SOF_STATIC_COMP_DAI(
		.id = 4, .pipeline_id = 2, .name = "DAI_I2S_CAP",
		.uuid = &dai_uuid, .direction = SOF_IPC_STREAM_CAPTURE,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2),
		.ep.dai.dai_type = SOF_DAI_ESP32_I2S,
		.ep.dai.dai_index = 0,
		.ep.dai.format = SOF_DAI_FMT_I2S
	),
	SOF_STATIC_COMP_MODULE(
		.id = 5, .pipeline_id = 2, .name = "VOL_CAP",
		.uuid = &volume_uuid, .direction = SOF_IPC_STREAM_CAPTURE,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2)
	),
	SOF_STATIC_COMP_HOST(
		.id = 6, .pipeline_id = 2, .name = "USB_CAP",
		.uuid = &usb_audio_uuid, .direction = SOF_IPC_STREAM_CAPTURE,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2),
		.ep.usb.terminal_id = CAPTURE_TERM_ID
	),
};

/* -------------------------------------------------------------------------
 * 2. Intermediate Audio Buffers
 * ------------------------------------------------------------------------- */
static const struct sof_static_buffer esp32s3_buffers[] = {
	/* Playback Buffers */
	SOF_STATIC_BUFFER(.id = 1, .size = 3072, .fmt = SOF_IPC_FRAME_S16_LE),
	SOF_STATIC_BUFFER(.id = 2, .size = 3072, .fmt = SOF_IPC_FRAME_S16_LE),

	/* Capture Buffers */
	SOF_STATIC_BUFFER(.id = 3, .size = 3072, .fmt = SOF_IPC_FRAME_S16_LE),
	SOF_STATIC_BUFFER(.id = 4, .size = 3072, .fmt = SOF_IPC_FRAME_S16_LE),
};

/* -------------------------------------------------------------------------
 * 3. Pipeline Connections / Graph Routing
 * ------------------------------------------------------------------------- */
static const struct sof_static_route esp32s3_routes[] = {
	/* Playback Route: USB_PB (1) -> [1] -> VOL_PB (2) -> [2] -> DAI_I2S_PB (3) */
	SOF_STATIC_ROUTE(.src_comp_id = 1, .buffer_id = 1, .sink_comp_id = 2),
	SOF_STATIC_ROUTE(.src_comp_id = 2, .buffer_id = 2, .sink_comp_id = 3),

	/* Capture Route: DAI_I2S_CAP (4) -> [3] -> VOL_CAP (5) -> [4] -> USB_CAP (6) */
	SOF_STATIC_ROUTE(.src_comp_id = 4, .buffer_id = 3, .sink_comp_id = 5),
	SOF_STATIC_ROUTE(.src_comp_id = 5, .buffer_id = 4, .sink_comp_id = 6),
};

/* -------------------------------------------------------------------------
 * 4. PCMs (Pulse Code Modulated Endpoints)
 * ------------------------------------------------------------------------- */
static const struct sof_static_pcm esp32s3_pcms[] = {
	SOF_STATIC_PCM(
		.pcm_id = 0,
		.name = "Speaker Playback",
		.direction = SOF_IPC_STREAM_PLAYBACK,
		.pipeline_id = 1,
		.host_comp_id = 1,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2)
	),
	SOF_STATIC_PCM(
		.pcm_id = 1,
		.name = "Mic Capture",
		.direction = SOF_IPC_STREAM_CAPTURE,
		.pipeline_id = 2,
		.host_comp_id = 6,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2)
	),
};

/* -------------------------------------------------------------------------
 * 5. Kcontrols (Volume and Mute)
 * ------------------------------------------------------------------------- */
static const struct sof_static_kcontrol esp32s3_controls[] = {
	SOF_STATIC_KCONTROL_VOLUME(
		.id = 1, .name = "Master Playback Volume",
		.target_comp_id = 2,
		.min = 0, .max = 65536, .def = 65536, .channels = 2,
		.uac2_entity_id = PLAYBACK_FU_ID
	),
	SOF_STATIC_KCONTROL_VOLUME(
		.id = 2, .name = "Master Capture Volume",
		.target_comp_id = 5,
		.min = 0, .max = 65536, .def = 65536, .channels = 2,
		.uac2_entity_id = CAPTURE_FU_ID
	),
#if defined(CONFIG_COMP_BT_AUDIO)
	SOF_STATIC_KCONTROL_SWITCH(
		.id = 8, .name = "BT Audio Stream Switch",
		.target_comp_id = 1,
		.def = 0,
		.uac2_entity_id = 0
	),
	SOF_STATIC_KCONTROL_VOLUME(
		.id = 9, .name = "BT Stream Volume",
		.target_comp_id = 1,
		.min = 0, .max = 65536, .def = 65536, .channels = 2,
		.uac2_entity_id = 0
	),
	SOF_STATIC_KCONTROL_ENUM(
		.id = 10, .name = "Audio Endpoint Route",
		.target_comp_id = 1,
		.min = 0, .max = 2, .def = 0, .channels = 1,
		.uac2_entity_id = 0
	),
	SOF_STATIC_KCONTROL_ENUM(
		.id = 14, .name = "BT Audio Format",
		.target_comp_id = 1,
		.min = 0, .max = 6, .def = 0, .channels = 1,
		.uac2_entity_id = 0
	),
#endif
};

/* -------------------------------------------------------------------------
 * 6. Pipeline Descriptors
 * ------------------------------------------------------------------------- */
static const struct sof_static_pipeline_desc esp32s3_pipelines[] = {
	{
		.pipeline_id = 1,
		.name = "Playback Pipeline",
		.direction = SOF_IPC_STREAM_PLAYBACK,
		.priority = 0,
		.core = 0,
		.period = 1000, /* 1ms */
		.frames_per_sched = 48,
		.time_domain = SOF_TIME_DOMAIN_TIMER,
		.sched_comp_id = 1,
		.source_comp_id = 1,
		.sink_comp_id = 3,
	},
	{
		.pipeline_id = 2,
		.name = "Capture Pipeline",
		.direction = SOF_IPC_STREAM_CAPTURE,
		.priority = 0,
		.core = 0,
		.period = 1000, /* 1ms */
		.frames_per_sched = 48,
		.time_domain = SOF_TIME_DOMAIN_TIMER,
		.sched_comp_id = 6,
		.source_comp_id = 4,
		.sink_comp_id = 6,
	},
};

/* -------------------------------------------------------------------------
 * Top-Level Exported Topology
 * ------------------------------------------------------------------------- */
const struct sof_static_topology g_esp32s3_static_topology = {
	.name = "ESP32-S3 S16_LE Playback & Capture Pipeline",
	.num_pipelines = ARRAY_SIZE(esp32s3_pipelines),
	.pipelines = esp32s3_pipelines,
	.num_comps = ARRAY_SIZE(esp32s3_comps),
	.comps = esp32s3_comps,
	.num_buffers = ARRAY_SIZE(esp32s3_buffers),
	.buffers = esp32s3_buffers,
	.num_routes = ARRAY_SIZE(esp32s3_routes),
	.routes = esp32s3_routes,
	.num_pcms = ARRAY_SIZE(esp32s3_pcms),
	.pcms = esp32s3_pcms,
	.num_controls = ARRAY_SIZE(esp32s3_controls),
	.controls = esp32s3_controls,
};
