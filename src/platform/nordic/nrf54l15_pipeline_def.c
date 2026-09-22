// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/dai-zephyr.h>
#include <ipc/dai.h>
#include <zephyr/device.h>

/* UUID External Declarations */
extern const struct sof_uuid volume_uuid;
extern const struct sof_uuid eq_iir_uuid;
extern const struct sof_uuid drc_uuid;
extern const struct sof_uuid tone_uuid;
extern const struct sof_uuid level_multiplier_uuid;
extern const struct sof_uuid selector_uuid;
extern const struct sof_uuid mixer_uuid;
extern const struct sof_uuid usb_audio_uuid;
extern const struct sof_uuid bt_audio_uuid;
extern const struct sof_uuid i2s_audio_uuid;

/* Default 2-channel 4-band Parametric IIR EQ Coefficients */
static const uint32_t nrf54l_default_iir_coef_2ch[51] = {
	0x00464f53, 0x00000000, 0x000000ac, 0x03013000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x000000ac, 0x00000002, 0x00000001, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000004, 0x00000004, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0xc12c82bd,
	0x7ed0b52e, 0x1fc7cc0c, 0xc07067e9, 0x1fc7cc0c,
	0x00000000, 0x00004000, 0xcad0cdef, 0x742e8c5d,
	0x0cdc9086, 0xe2f11723, 0x10b2f932, 0x00000000,
	0x00004000, 0xcf45334a, 0x68260de9, 0x0a54e176,
	0xe5d6cb75, 0x11fc1f3d, 0x00000000, 0x00004000,
	0xf2940609, 0xe25f3930, 0x0d69ba64, 0x1ad374c8,
	0x0d69ba64, 0xfffffffb, 0x000045bf
};

/* Default Speaker DRC Compressor Profile */
static const uint32_t nrf54l_default_drc_coef[35] = {
	0x00464f53, 0x00000000, 0x0000006c, 0x0301a000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x0000006c, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000001, 0xe2000000, 0x14000000,
	0x0a000000, 0x00624dd3, 0x02061b8a, 0x06666666,
	0x00ba972f, 0x001e0c18, 0xffe04220, 0x0050f44e,
	0x08349f9a, 0x04d82cd3, 0x0071c71c, 0xff777777,
	0x001f77d8, 0x00000005, 0x00438000, 0x00047dd7,
	0x0025cea0, 0x00097dd7, 0x0000b5b1
};

/* -------------------------------------------------------------------------
 * 1. Component / Module Declarations
 * ------------------------------------------------------------------------- */
static const struct sof_static_comp nrf54l15_comps[] = {
	/* --- Audio Processing Pipeline (Pipeline 1) --- */
	SOF_STATIC_COMP_HOST(
		.id = 1, .pipeline_id = 1, .name = "I2S_RX",
		.uuid = &i2s_audio_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 2, .pipeline_id = 1, .name = "VOL_PB",
		.uuid = &volume_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 3, .pipeline_id = 1, .name = "EQ_PB",
		.uuid = &eq_iir_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2),
		.init_blob = nrf54l_default_iir_coef_2ch,
		.init_blob_size = sizeof(nrf54l_default_iir_coef_2ch)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 4, .pipeline_id = 1, .name = "DRC_PB",
		.uuid = &drc_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2),
		.init_blob = nrf54l_default_drc_coef,
		.init_blob_size = sizeof(nrf54l_default_drc_coef)
	),
	SOF_STATIC_COMP_HOST(
		.id = 5, .pipeline_id = 1, .name = "I2S_TX",
		.uuid = &i2s_audio_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),

	/* --- Synth & Routing Test Pipeline (Pipeline 2) --- */
	SOF_STATIC_COMP_MODULE(
		.id = 6, .pipeline_id = 2, .name = "TONE_SYNTH",
		.uuid = &tone_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 7, .pipeline_id = 2, .name = "LEVEL_SYNTH",
		.uuid = &level_multiplier_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 8, .pipeline_id = 2, .name = "SEL_SYNTH",
		.uuid = &selector_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 9, .pipeline_id = 2, .name = "MIXER_SYNTH",
		.uuid = &mixer_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_HOST(
		.id = 10, .pipeline_id = 2, .name = "SINK_SYNTH",
		.uuid = &usb_audio_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2),
		.ep.usb.terminal_id = 2
	),
};

/* -------------------------------------------------------------------------
 * 2. Intermediate Audio Buffers
 * ------------------------------------------------------------------------- */
static const struct sof_static_buffer nrf54l15_buffers[] = {
	/* Pipeline 1 Buffers */
	SOF_STATIC_BUFFER(.id = 1, .size = 1536, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 2, .size = 1536, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 3, .size = 1536, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 4, .size = 1536, .fmt = SOF_IPC_FRAME_FLOAT),

	/* Pipeline 2 Buffers */
	SOF_STATIC_BUFFER(.id = 5, .size = 1536, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 6, .size = 1536, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 7, .size = 1536, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 8, .size = 1536, .fmt = SOF_IPC_FRAME_FLOAT),
};

/* -------------------------------------------------------------------------
 * 3. Static Audio Routes
 * ------------------------------------------------------------------------- */
static const struct sof_static_route nrf54l15_routes[] = {
	/* Pipeline 1: Tone (1) -> [1] -> Vol (2) -> [2] -> EQ (3) -> [3] -> DRC (4) -> [4] -> Sink (5) */
	SOF_STATIC_ROUTE(.src_comp_id = 1, .buffer_id = 1, .sink_comp_id = 2),
	SOF_STATIC_ROUTE(.src_comp_id = 2, .buffer_id = 2, .sink_comp_id = 3),
	SOF_STATIC_ROUTE(.src_comp_id = 3, .buffer_id = 3, .sink_comp_id = 4),
	SOF_STATIC_ROUTE(.src_comp_id = 4, .buffer_id = 4, .sink_comp_id = 5),

	/* Pipeline 2: Tone (6) -> [5] -> Level (7) -> [6] -> Sel (8) -> [7] -> Mixer (9) -> [8] -> Sink (10) */
	SOF_STATIC_ROUTE(.src_comp_id = 6, .buffer_id = 5, .sink_comp_id = 7),
	SOF_STATIC_ROUTE(.src_comp_id = 7, .buffer_id = 6, .sink_comp_id = 8),
	SOF_STATIC_ROUTE(.src_comp_id = 8, .buffer_id = 7, .sink_comp_id = 9),
	SOF_STATIC_ROUTE(.src_comp_id = 9, .buffer_id = 8, .sink_comp_id = 10),
};

/* -------------------------------------------------------------------------
 * 4. Controls (Kcontrols)
 * ------------------------------------------------------------------------- */
static const struct sof_static_kcontrol nrf54l15_controls[] = {
	SOF_STATIC_KCONTROL_VOLUME(
		.id = 1,
		.name = "Master Playback Volume",
		.target_comp_id = 2,
		.channels = 2,
		.min = 0,
		.max = 65536,
		.def = 65536
	),
	SOF_STATIC_KCONTROL_SWITCH(
		.id = 2,
		.name = "Master Playback Switch",
		.target_comp_id = 2,
		.channels = 2,
		.min = 0,
		.max = 1,
		.def = 1
	),
	SOF_STATIC_KCONTROL_VOLUME(
		.id = 3,
		.name = "Synth Level Multiplier",
		.target_comp_id = 7,
		.channels = 2,
		.min = 0,
		.max = 65536,
		.def = 32768
	),
	SOF_STATIC_KCONTROL_SWITCH(
		.id = 4,
		.name = "Synth Switch",
		.target_comp_id = 7,
		.channels = 2,
		.min = 0,
		.max = 1,
		.def = 1
	),
};

/* -------------------------------------------------------------------------
 * 5. Pipelines
 * ------------------------------------------------------------------------- */
static const struct sof_static_pipeline_desc nrf54l15_pipelines[] = {
	{
		.pipeline_id = 1,
		.name = "Nordic I2S Echo Pipeline",
		.direction = SOF_IPC_STREAM_PLAYBACK,
		.priority = 0,
		.core = 0,
		.period = 1000, /* 1ms */
		.frames_per_sched = 48,
		.time_domain = SOF_TIME_DOMAIN_TIMER,
		.sched_comp_id = 1,
		.source_comp_id = 1,
		.sink_comp_id = 5,
	},
	{
		.pipeline_id = 2,
		.name = "Nordic Synth & Mix Pipeline",
		.direction = SOF_IPC_STREAM_PLAYBACK,
		.priority = 0,
		.core = 0,
		.period = 1000, /* 1ms */
		.frames_per_sched = 48,
		.time_domain = SOF_TIME_DOMAIN_TIMER,
		.sched_comp_id = 6,
		.source_comp_id = 6,
		.sink_comp_id = 10,
	},
};

/* -------------------------------------------------------------------------
 * Top-Level Exported Topology
 * ------------------------------------------------------------------------- */
const struct sof_static_topology g_nrf54l15_static_topology = {
	.name = "nRF54L15 Static Audio Pipeline",
	.num_pipelines = ARRAY_SIZE(nrf54l15_pipelines),
	.pipelines = nrf54l15_pipelines,
	.num_comps = ARRAY_SIZE(nrf54l15_comps),
	.comps = nrf54l15_comps,
	.num_buffers = ARRAY_SIZE(nrf54l15_buffers),
	.buffers = nrf54l15_buffers,
	.num_routes = ARRAY_SIZE(nrf54l15_routes),
	.routes = nrf54l15_routes,
	.num_pcms = 0,
	.pcms = NULL,
	.num_controls = ARRAY_SIZE(nrf54l15_controls),
	.controls = nrf54l15_controls,
};
