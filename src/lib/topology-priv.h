/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *	   Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 *	   Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef _TOPOLOGY_PRIV_H
#define _TOPOLOGY_PRIV_H

#if defined(CONFIG_TOPOLOGY)

#include <stddef.h>

#define TPLG_CTL_ELEM_ID_NAME_MAXLEN  44

#define SOF_TPLG_NUM_TEXTS            16
#define SOF_TPLG_MAX_CHAN             8

#define SOF_TPLG_CTL_VOLSW            1
#define SOF_TPLG_CTL_VOLSW_SX         2
#define SOF_TPLG_CTL_VOLSW_XR_SX      3
#define SOF_TPLG_CTL_ENUM             4
#define SOF_TPLG_CTL_BYTES            5
#define SOF_TPLG_CTL_ENUM_VALUE       6
#define SOF_TPLG_CTL_RANGE            7
#define SOF_TPLG_CTL_STROBE           8

#define SOF_TPLG_DAPM_CTL_VOLSW       64
#define SOF_TPLG_DAPM_CTL_ENUM_DOUBLE 65
#define SOF_TPLG_DAPM_CTL_ENUM_VIRT   66
#define SOF_TPLG_DAPM_CTL_ENUM_VALUE  67
#define SOF_TPLG_DAPM_CTL_PIN         68

#define SOF_TPLG_TLV_SIZE             32

#define SOF_TPLG_TYPE_MIXER           1
#define SOF_TPLG_TYPE_BYTES           2
#define SOF_TPLG_TYPE_ENUM            3
#define SOF_TPLG_TYPE_DAPM_GRAPH      4
#define SOF_TPLG_TYPE_DAPM_WIDGET     5
#define SOF_TPLG_TYPE_DAI_LINK        6
#define SOF_TPLG_TYPE_PCM             7
#define SOF_TPLG_TYPE_MANIFEST        8
#define SOF_TPLG_TYPE_CODEC_LINK      9
#define SOF_TPLG_TYPE_BACKEND_LINK    10
#define SOF_TPLG_TYPE_PDATA           11
#define SOF_TPLG_TYPE_DAI             12

#define SOF_TPLG_TYPE_VENDOR_FW       1000
#define SOF_TPLG_TYPE_VENDOR_CONFIG   1001
#define SOF_TPLG_TYPE_VENDOR_COEFF    1002
#define SOF_TPLG_TYPE_VENDOR_CODEC    1003

#define SOF_TPLG_STREAM_PLAYBACK      0
#define SOF_TPLG_STREAM_CAPTURE       1

#define SOF_TPLG_TUPLE_TYPE_UUID      0
#define SOF_TPLG_TUPLE_TYPE_STRING    1
#define SOF_TPLG_TUPLE_TYPE_BOOL      2
#define SOF_TPLG_TUPLE_TYPE_BYTE      3
#define SOF_TPLG_TUPLE_TYPE_WORD      4
#define SOF_TPLG_TUPLE_TYPE_SHORT     5

#define SOF_TPLG_DAPM_INPUT           0
#define SOF_TPLG_DAPM_OUTPUT          1
#define SOF_TPLG_DAPM_MUX             2
#define SOF_TPLG_DAPM_MIXER           3
#define SOF_TPLG_DAPM_PGA             4
#define SOF_TPLG_DAPM_OUT_DRV         5
#define SOF_TPLG_DAPM_ADC             6
#define SOF_TPLG_DAPM_DAC             7
#define SOF_TPLG_DAPM_SWITCH          8
#define SOF_TPLG_DAPM_PRE             9
#define SOF_TPLG_DAPM_POST            10
#define SOF_TPLG_DAPM_AIF_IN          11
#define SOF_TPLG_DAPM_AIF_OUT         12
#define SOF_TPLG_DAPM_DAI_IN          13
#define SOF_TPLG_DAPM_DAI_OUT         14
#define SOF_TPLG_DAPM_DAI_LINK        15
#define SOF_TPLG_DAPM_BUFFER          16
#define SOF_TPLG_DAPM_SCHEDULER       17
#define SOF_TPLG_DAPM_EFFECT          18
#define SOF_TPLG_DAPM_SIGGEN          19
#define SOF_TPLG_DAPM_SRC             20
#define SOF_TPLG_DAPM_ASRC            21
#define SOF_TPLG_DAPM_ENCODER         22
#define SOF_TPLG_DAPM_DECODER         23
#define SOF_TPLG_DAPM_LAST            SOF_TPLG_DAPM_DECODER

struct sof_tplg_hdr {
	uint32_t magic;
	uint32_t abi;
	uint32_t version;
	uint32_t type;
	uint32_t size;
	uint32_t vendor_type;
	uint32_t payload_size;
	uint32_t index;
	uint32_t count;
} __attribute__((packed));

struct sof_tplg_vendor_uuid_elem {
	uint32_t token;
	char uuid[16];
} __attribute__((packed));

struct sof_tplg_vendor_value_elem {
	uint32_t token;
	uint32_t value;
} __attribute__((packed));

struct sof_tplg_vendor_string_elem {
	uint32_t token;
	char string[TPLG_CTL_ELEM_ID_NAME_MAXLEN];
} __attribute__((packed));

struct sof_tplg_vendor_array {
	uint32_t size;
	uint32_t type;
	uint32_t num_elems;

	union {
		struct sof_tplg_vendor_uuid_elem uuid[0];
		struct sof_tplg_vendor_value_elem value[0];
		struct sof_tplg_vendor_string_elem string[0];
	};
} __attribute__((packed));

struct sof_tplg_private {
	uint32_t size;

	union {
		char data[0];
		struct sof_tplg_vendor_array array[0];
	};
} __attribute__((packed))dbsc;

struct sof_tplg_tlv_dbscale {
	uint32_t min;
	uint32_t step;
	uint32_t mute;
} __attribute__((packed));

struct sof_tplg_ctl_tlv {
	uint32_t size;
	uint32_t type;

	union {
		uint32_t data[SOF_TPLG_TLV_SIZE];
		struct sof_tplg_tlv_dbscale scale;
	};
} __attribute__((packed));

struct sof_tplg_channel {
	uint32_t size;
	uint32_t reg;
	uint32_t shift;
	uint32_t id;
} __attribute__((packed));

struct sof_tplg_io_ops {
	uint32_t get;
	uint32_t put;
	uint32_t info;
} __attribute__((packed));

struct sof_tplg_ctl_hdr {
	uint32_t size;
	uint32_t type;
	char name[TPLG_CTL_ELEM_ID_NAME_MAXLEN];
	uint32_t access;
	struct sof_tplg_io_ops ops;
	struct sof_tplg_ctl_tlv tlv;
} __attribute__((packed));

struct sof_tplg_manifest {
	uint32_t size;
	uint32_t control_elems;
	uint32_t widget_elems;
	uint32_t graph_elems;
	uint32_t pcm_elems;
	uint32_t dai_link_elems;
	uint32_t dai_elems;
	uint32_t reserved[20];
	struct sof_tplg_private priv;
} __attribute__((packed));

struct sof_tplg_mixer_control {
	struct sof_tplg_ctl_hdr hdr;
	uint32_t size;
	uint32_t min;
	uint32_t max;
	uint32_t platform_max;
	uint32_t invert;
	uint32_t num_channels;
	struct sof_tplg_channel channel[SOF_TPLG_MAX_CHAN];
	struct sof_tplg_private priv;
} __attribute__((packed));

struct sof_tplg_enum_control {
	struct sof_tplg_ctl_hdr hdr;
	uint32_t size;
	uint32_t num_channels;
	struct sof_tplg_channel channel[SOF_TPLG_MAX_CHAN];
	uint32_t items;
	uint32_t mask;
	uint32_t count;
	char texts[SOF_TPLG_NUM_TEXTS][TPLG_CTL_ELEM_ID_NAME_MAXLEN];
	uint32_t values[SOF_TPLG_NUM_TEXTS *
			TPLG_CTL_ELEM_ID_NAME_MAXLEN / 4];
	struct sof_tplg_private priv;
} __attribute__((packed));

struct sof_tplg_bytes_control {
	struct sof_tplg_ctl_hdr hdr;
	uint32_t size;
	uint32_t max;
	uint32_t mask;
	uint32_t base;
	uint32_t num_regs;
	struct sof_tplg_io_ops ext_ops;
	struct sof_tplg_private priv;
} __attribute__((packed));

struct sof_tplg_dapm_graph_elem {
	char sink[TPLG_CTL_ELEM_ID_NAME_MAXLEN];
	char control[TPLG_CTL_ELEM_ID_NAME_MAXLEN];
	char source[TPLG_CTL_ELEM_ID_NAME_MAXLEN];
} __attribute__((packed));

struct sof_tplg_dapm_widget {
	uint32_t size;
	uint32_t id;
	char name[TPLG_CTL_ELEM_ID_NAME_MAXLEN];
	char sname[TPLG_CTL_ELEM_ID_NAME_MAXLEN];

	uint32_t reg;
	uint32_t shift;
	uint32_t mask;
	uint32_t subseq;
	uint32_t invert;
	uint32_t ignore_suspend;
	uint16_t event_flags;
	uint16_t event_type;
	uint32_t num_kcontrols;
	struct sof_tplg_private priv;
} __attribute__((packed));

/* EQ parsing related */
#define SOF_EFFECT_DATA_SIZE 156
#define SOF_EQIIR_DATA_SIZE 8

struct comp_info {
	char *name;
	int id;
	int type;
	int pipeline_id;
};

struct frame_types {
	char *name;
	enum sof_ipc_frame frame;
};

static const struct frame_types sof_frames[] = {
	/* TODO: fix topology to use ALSA formats */
	{"s16le", SOF_IPC_FRAME_S16_LE},
	{"s24le", SOF_IPC_FRAME_S24_4LE},
	{"s32le", SOF_IPC_FRAME_S32_LE},
	{"float", SOF_IPC_FRAME_FLOAT},
	/* ALSA formats */
	{"S16_LE", SOF_IPC_FRAME_S16_LE},
	{"S24_LE", SOF_IPC_FRAME_S24_4LE},
	{"S32_LE", SOF_IPC_FRAME_S32_LE},
	{"FLOAT_LE", SOF_IPC_FRAME_FLOAT},
};

struct sof_topology_token {
	uint32_t token;
	uint32_t type;
	int (*get_token)(void *elem, void *object, uint32_t offset,
			 uint32_t size);
	uint32_t offset;
	uint32_t size;
};

#endif /* defined(CONFIG_TOPOLOGY) */
#endif /* _TOPOLOGY_PRIV_H */
