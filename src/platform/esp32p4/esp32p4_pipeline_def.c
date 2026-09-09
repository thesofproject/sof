// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2026 Sound Open Firmware (SOF) Project
 */

#include <sof/audio/pipeline/static_pipeline.h>
#include <sof/audio/component_ext.h>
#include <sof/lib/dai-zephyr.h>
#include <zephyr/usb/class/usbd_uac2.h>
#include <zephyr/device.h>

/* UUID External Declarations */
extern const struct sof_uuid usb_audio_uuid;
extern const struct sof_uuid volume_uuid;
extern const struct sof_uuid eq_iir_uuid;
extern const struct sof_uuid drc_uuid;
extern const struct sof_uuid tdfb_uuid;
extern const struct sof_uuid dai_uuid;
extern const struct sof_uuid tone_uuid;
extern const struct sof_uuid level_multiplier_uuid;
extern const struct sof_uuid selector_uuid;
extern const struct sof_uuid mixer_uuid;

/* UAC2 Device Tree Entity IDs (static constant initializers) */
#define UAC2_STATIC_ENTITY_ID(node) (DT_NODE_CHILD_IDX(node) + 1)

#define PLAYBACK_FU_ID       UAC2_STATIC_ENTITY_ID(DT_NODELABEL(i2s_fu))
#define PLAYBACK_EQ_FU_ID    UAC2_STATIC_ENTITY_ID(DT_NODELABEL(pb_eq_fu))
#define PLAYBACK_DRC_FU_ID   UAC2_STATIC_ENTITY_ID(DT_NODELABEL(pb_drc_fu))
#define CAPTURE_TDFB_FU_ID   UAC2_STATIC_ENTITY_ID(DT_NODELABEL(cap_tdfb_fu))
#define CAPTURE_EQ_FU_ID     UAC2_STATIC_ENTITY_ID(DT_NODELABEL(cap_eq_fu))
#define CAPTURE_FU_ID        UAC2_STATIC_ENTITY_ID(DT_NODELABEL(i2s_in_fu))
#define PLAYBACK_TERM_ID     UAC2_STATIC_ENTITY_ID(DT_NODELABEL(i2s_out_terminal))
#define CAPTURE_TERM_ID      UAC2_STATIC_ENTITY_ID(DT_NODELABEL(i2s_in_terminal))

/* Default 2-channel 4-band Parametric IIR EQ Coefficients */
static const uint32_t esp32p4_default_iir_coef_2ch[51] = {
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
static const uint32_t esp32p4_default_drc_coef[35] = {
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
static const struct sof_static_comp esp32p4_comps[] = {
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
	SOF_STATIC_COMP_MODULE(
		.id = 3, .pipeline_id = 1, .name = "EQ_PB",
		.uuid = &eq_iir_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2),
		.init_blob = esp32p4_default_iir_coef_2ch,
		.init_blob_size = sizeof(esp32p4_default_iir_coef_2ch)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 4, .pipeline_id = 1, .name = "DRC_PB",
		.uuid = &drc_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2),
		.init_blob = esp32p4_default_drc_coef,
		.init_blob_size = sizeof(esp32p4_default_drc_coef)
	),
	SOF_STATIC_COMP_DAI(
		.id = 5, .pipeline_id = 1, .name = "DAI_I2S_PB",
		.uuid = &dai_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2),
		.ep.dai.dai_type = SOF_DAI_ESP32_I2S,
		.ep.dai.dai_index = 0,
		.ep.dai.format = SOF_DAI_FMT_I2S
	),

	/* --- Capture Pipeline (Pipeline 2) --- */
	SOF_STATIC_COMP_DAI(
		.id = 6, .pipeline_id = 2, .name = "DAI_I2S_CAP",
		.uuid = &dai_uuid, .direction = SOF_IPC_STREAM_CAPTURE,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2),
		.ep.dai.dai_type = SOF_DAI_ESP32_I2S,
		.ep.dai.dai_index = 0,
		.ep.dai.format = SOF_DAI_FMT_I2S
	),
	SOF_STATIC_COMP_MODULE(
		.id = 7, .pipeline_id = 2, .name = "TDFB_CAP",
		.uuid = &tdfb_uuid, .direction = SOF_IPC_STREAM_CAPTURE,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 8, .pipeline_id = 2, .name = "EQ_CAP",
		.uuid = &eq_iir_uuid, .direction = SOF_IPC_STREAM_CAPTURE,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2),
		.init_blob = esp32p4_default_iir_coef_2ch,
		.init_blob_size = sizeof(esp32p4_default_iir_coef_2ch)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 9, .pipeline_id = 2, .name = "VOL_CAP",
		.uuid = &volume_uuid, .direction = SOF_IPC_STREAM_CAPTURE,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2)
	),
	SOF_STATIC_COMP_HOST(
		.id = 10, .pipeline_id = 2, .name = "USB_CAP",
		.uuid = &usb_audio_uuid, .direction = SOF_IPC_STREAM_CAPTURE,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2),
		.ep.usb.terminal_id = CAPTURE_TERM_ID
	),

	/* --- Float Synth & Routing Test Pipeline (Pipeline 3) --- */
	SOF_STATIC_COMP_MODULE(
		.id = 11, .pipeline_id = 3, .name = "TONE_TEST",
		.uuid = &tone_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 12, .pipeline_id = 3, .name = "LEVEL_TEST",
		.uuid = &level_multiplier_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 13, .pipeline_id = 3, .name = "SEL_TEST",
		.uuid = &selector_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
	SOF_STATIC_COMP_MODULE(
		.id = 14, .pipeline_id = 1, .name = "MIXER_PB",
		.uuid = &mixer_uuid, .direction = SOF_IPC_STREAM_PLAYBACK,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_FLOAT, 48000, 2)
	),
};

/* -------------------------------------------------------------------------
 * 2. Intermediate Audio Buffers
 * ------------------------------------------------------------------------- */
static const struct sof_static_buffer esp32p4_buffers[] = {
	/* Playback Buffers */
	SOF_STATIC_BUFFER(.id = 1, .size = 2048, .fmt = SOF_IPC_FRAME_S16_LE),
	SOF_STATIC_BUFFER(.id = 2, .size = 2048, .fmt = SOF_IPC_FRAME_S16_LE),
	SOF_STATIC_BUFFER(.id = 3, .size = 2048, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 4, .size = 2048, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 9, .size = 2048, .fmt = SOF_IPC_FRAME_FLOAT),

	/* Capture Buffers */
	SOF_STATIC_BUFFER(.id = 5, .size = 2048, .fmt = SOF_IPC_FRAME_S16_LE),
	SOF_STATIC_BUFFER(.id = 6, .size = 2048, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 7, .size = 2048, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 8, .size = 2048, .fmt = SOF_IPC_FRAME_S16_LE),

	/* Test Pipeline Buffers */
	SOF_STATIC_BUFFER(.id = 11, .size = 2048, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 12, .size = 2048, .fmt = SOF_IPC_FRAME_FLOAT),
	SOF_STATIC_BUFFER(.id = 13, .size = 2048, .fmt = SOF_IPC_FRAME_FLOAT),
};

/* -------------------------------------------------------------------------
 * 3. Pipeline Connections / Graph Routing
 * ------------------------------------------------------------------------- */
static const struct sof_static_route esp32p4_routes[] = {
	/* Playback Route: USB_PB (1) -> [1] -> VOL_PB (2) -> [2] -> DAI (5) */
	SOF_STATIC_ROUTE(.src_comp_id = 1, .buffer_id = 1, .sink_comp_id = 2),
	SOF_STATIC_ROUTE(.src_comp_id = 2, .buffer_id = 2, .sink_comp_id = 5),

	/* Capture Route: DAI (6) -> [5] -> VOL (9) -> [8] -> USB (10) */
	SOF_STATIC_ROUTE(.src_comp_id = 6, .buffer_id = 5, .sink_comp_id = 9),
	SOF_STATIC_ROUTE(.src_comp_id = 9, .buffer_id = 8, .sink_comp_id = 10),

	/* Test Route: TONE (11) -> [11] -> LEVEL (12) -> [12] -> SEL (13) -> [13] -> MIXER (14) */
	SOF_STATIC_ROUTE(.src_comp_id = 11, .buffer_id = 11, .sink_comp_id = 12),
	SOF_STATIC_ROUTE(.src_comp_id = 12, .buffer_id = 12, .sink_comp_id = 13),
	SOF_STATIC_ROUTE(.src_comp_id = 13, .buffer_id = 13, .sink_comp_id = 14),
};

/* -------------------------------------------------------------------------
 * 4. PCMs (Pulse Code Modulated Endpoints)
 * ------------------------------------------------------------------------- */
static const struct sof_static_pcm esp32p4_pcms[] = {
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
		.host_comp_id = 10,
		.caps = SOF_STATIC_CAPS(SOF_IPC_FRAME_S16_LE, 48000, 2)
	),
};

/* -------------------------------------------------------------------------
 * 5. Kcontrols (Volume, Mute, EQ/DRC/TDFB Bypass Switches)
 * ------------------------------------------------------------------------- */
static const struct sof_static_kcontrol esp32p4_controls[] = {
	SOF_STATIC_KCONTROL_VOLUME(
		.id = 1, .name = "Master Playback Volume",
		.target_comp_id = 2,
		.min = 0, .max = 65536, .def = 65536, .channels = 2,
		.uac2_entity_id = PLAYBACK_FU_ID
	),
	SOF_STATIC_KCONTROL_SWITCH(
		.id = 2, .name = "Playback EQ Switch",
		.target_comp_id = 3,
		.def = 0, /* 1 = Enabled, 0 = Bypassed */
		.uac2_entity_id = PLAYBACK_EQ_FU_ID
	),
	SOF_STATIC_KCONTROL_SWITCH(
		.id = 3, .name = "Playback DRC Switch",
		.target_comp_id = 4,
		.def = 0,
		.uac2_entity_id = PLAYBACK_DRC_FU_ID
	),
	SOF_STATIC_KCONTROL_SWITCH(
		.id = 4, .name = "Capture TDFB Switch",
		.target_comp_id = 7,
		.def = 0,
		.uac2_entity_id = CAPTURE_TDFB_FU_ID
	),
	SOF_STATIC_KCONTROL_SWITCH(
		.id = 5, .name = "Capture EQ Switch",
		.target_comp_id = 8,
		.def = 0,
		.uac2_entity_id = CAPTURE_EQ_FU_ID
	),
	SOF_STATIC_KCONTROL_VOLUME(
		.id = 6, .name = "Master Capture Volume",
		.target_comp_id = 9,
		.min = 0, .max = 65536, .def = 65536, .channels = 2,
		.uac2_entity_id = CAPTURE_FU_ID
	),
	SOF_STATIC_KCONTROL_VOLUME(
		.id = 11, .name = "Test Level Gain",
		.target_comp_id = 12,
		.min = 0, .max = 0x7FFFFFFF, .def = 0x7FFFFFFF, .channels = 2,
		.uac2_entity_id = 0
	),
	SOF_STATIC_KCONTROL_SWITCH(
		.id = 12, .name = "Test Level Mute",
		.target_comp_id = 12,
		.def = 1,
		.uac2_entity_id = 0
	),
	SOF_STATIC_KCONTROL_ENUM(
		.id = 13, .name = "Test Selector Channel",
		.target_comp_id = 13,
		.min = 0, .max = 1, .def = 0, .channels = 1,
		.uac2_entity_id = 0
	),
};

/* -------------------------------------------------------------------------
 * 6. Pipeline Descriptors
 * ------------------------------------------------------------------------- */
static const struct sof_static_pipeline_desc esp32p4_pipelines[] = {
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
		.sink_comp_id = 5,
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
		.sched_comp_id = 10,
		.source_comp_id = 6,
		.sink_comp_id = 10,
	},
	{
		.pipeline_id = 3,
		.name = "Float Synth & Route Test",
		.direction = SOF_IPC_STREAM_PLAYBACK,
		.priority = 0,
		.core = 0,
		.period = 1000, /* 1ms */
		.frames_per_sched = 48,
		.time_domain = SOF_TIME_DOMAIN_TIMER,
		.sched_comp_id = 11,
		.source_comp_id = 11,
		.sink_comp_id = 13,
	},
};

/* -------------------------------------------------------------------------
 * Top-Level Exported Topology
 * ------------------------------------------------------------------------- */
const struct sof_static_topology g_esp32p4_static_topology = {
	.name = "ESP32-P4 Float Playback & Capture Pipeline",
	.num_pipelines = ARRAY_SIZE(esp32p4_pipelines),
	.pipelines = esp32p4_pipelines,
	.num_comps = ARRAY_SIZE(esp32p4_comps),
	.comps = esp32p4_comps,
	.num_buffers = ARRAY_SIZE(esp32p4_buffers),
	.buffers = esp32p4_buffers,
	.num_routes = ARRAY_SIZE(esp32p4_routes),
	.routes = esp32p4_routes,
	.num_pcms = ARRAY_SIZE(esp32p4_pcms),
	.pcms = esp32p4_pcms,
	.num_controls = ARRAY_SIZE(esp32p4_controls),
	.controls = esp32p4_controls,
};
