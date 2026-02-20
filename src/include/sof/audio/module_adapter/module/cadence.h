/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>
 */

#ifndef __SOF_AUDIO_CADENCE_CODEC__
#define __SOF_AUDIO_CADENCE_CODEC__

#include <sof/audio/cadence/xa_type_def.h>
#include <sof/audio/cadence/xa_apicmd_standards.h>
#include <sof/audio/cadence/xa_error_standards.h>
#include <sof/audio/cadence/xa_memory_standards.h>

#define LIB_NAME_MAX_LEN 30
#define LIB_NO_ERROR XA_NO_ERROR
#define LIB_IS_FATAL_ERROR(e) ((e) & XA_FATAL_ERROR)
#define CODEC_GET_API_ID(id) ((id) & 0xFF)
#define CADENCE_MP3_ENCODER_DEFAULT_BITRATE 320

/*****************************************************************************/
/* Cadence API functions							     */
/*****************************************************************************/
extern xa_codec_func_t cadence_api_function;
extern xa_codec_func_t xa_aac_dec;
extern xa_codec_func_t xa_bsac_dec;
extern xa_codec_func_t xa_dabplus_dec;
extern xa_codec_func_t xa_drm_dec;
extern xa_codec_func_t xa_mp3_dec;
extern xa_codec_func_t xa_mp3_enc;
extern xa_codec_func_t xa_sbc_dec;
extern xa_codec_func_t xa_vorbis_dec;
extern xa_codec_func_t xa_src_pp;
extern xa_codec_func_t xa_pcm_dec;

#define DEFAULT_CODEC_ID CADENCE_CODEC_WRAPPER_ID

#define API_CALL(cd, cmd, sub_cmd, value, ret) \
	do { \
		ret = (cd)->api((cd)->self, \
				(cmd), \
				(sub_cmd), \
				(value)); \
	} while (0)

/*****************************************************************************/
/* Cadence private data types						     */
/*****************************************************************************/
struct cadence_api {
	uint32_t id;
	xa_codec_func_t *api;
};

struct cadence_codec_data {
#if CONFIG_IPC_MAJOR_4
	struct ipc4_base_module_cfg base_cfg;
	uint32_t direction;
#endif
	char name[LIB_NAME_MAX_LEN];
	void *self;
	xa_codec_func_t *api;
	void *mem_tabs;
	size_t mem_to_be_freed_len;
	void **mem_to_be_freed;
	uint32_t api_id;
	struct module_config setup_cfg;
};

enum cadence_api_id {
	CADENCE_CODEC_WRAPPER_ID	= 0x01,
	CADENCE_CODEC_AAC_DEC_ID	= 0x02,
	CADENCE_CODEC_BSAC_DEC_ID	= 0x03,
	CADENCE_CODEC_DAB_DEC_ID	= 0x04,
	CADENCE_CODEC_DRM_DEC_ID	= 0x05,
	CADENCE_CODEC_MP3_DEC_ID	= 0x06,
	CADENCE_CODEC_SBC_DEC_ID	= 0x07,
	CADENCE_CODEC_VORBIS_DEC_ID	= 0x08,
	CADENCE_CODEC_SRC_PP_ID		= 0x09,
	CADENCE_CODEC_MP3_ENC_ID	= 0x0A,
	SOF_COMPRESS_CODEC_PCM_DEC_ID	= 0xC0,
};

#if CONFIG_IPC_MAJOR_4
struct ipc4_cadence_module_cfg {
	struct ipc4_base_module_cfg base_cfg;
	uint32_t param_size;
	struct module_param param[];
} __packed __aligned(4);
#endif

extern struct cadence_api cadence_api_table[];

int cadence_codec_set_configuration(struct processing_module *mod, uint32_t config_id,
				    enum module_cfg_fragment_position pos,
				    uint32_t data_offset_size, const uint8_t *fragment,
				    size_t fragment_size, uint8_t *response, size_t response_size);
int cadence_codec_resolve_api_with_id(struct processing_module *mod, uint32_t codec_id,
				      uint32_t direction);
int cadence_codec_apply_params(struct processing_module *mod, int size, void *data);
int cadence_codec_process_data(struct processing_module *mod);
int cadence_codec_apply_config(struct processing_module *mod);
void cadence_codec_free_memory_tables(struct processing_module *mod);
int cadence_codec_init_memory_tables(struct processing_module *mod);
int cadence_codec_get_samples(struct processing_module *mod);
int cadence_codec_init_process(struct processing_module *mod);
int cadence_init_codec_object(struct processing_module *mod);
int cadence_codec_resolve_api(struct processing_module *mod);
int cadence_codec_free(struct processing_module *mod);
size_t cadence_api_table_size(void);

#endif /* __SOF_AUDIO_CADENCE_CODEC__ */
