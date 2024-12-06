// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Marcin Rajwa <marcin.rajwa@linux.intel.com>

/*
 * \file cadence.c
 * \brief Cadence Codec API
 * \author Marcin Rajwa <marcin.rajwa@linux.intel.com>
 *
 */

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/cadence.h>
#include <ipc/compress_params.h>
#include <rtos/init.h>

LOG_MODULE_REGISTER(cadence_codec, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(cadence_codec);

DECLARE_TR_CTX(cadence_codec_tr, SOF_UUID(cadence_codec_uuid), LOG_LEVEL_INFO);

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
};

#define DEFAULT_CODEC_ID CADENCE_CODEC_WRAPPER_ID

/*****************************************************************************/
/* Cadence API functions array						     */
/*****************************************************************************/
static struct cadence_api cadence_api_table[] = {
#ifdef CONFIG_CADENCE_CODEC_WRAPPER
	{
		.id = CADENCE_CODEC_WRAPPER_ID,
		.api = cadence_api_function
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_AAC_DEC
	{
		.id = CADENCE_CODEC_AAC_DEC_ID,
		.api = xa_aac_dec,
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_BSAC_DEC
	{
		.id = CADENCE_CODEC_BSAC_DEC_ID,
		.api = xa_bsac_dec,
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_DAB_DEC
	{
		.id = CADENCE_CODEC_DAB_DEC_ID,
		.api = xa_dabplus_dec,
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_DRM_DEC
	{
		.id = CADENCE_CODEC_DRM_DEC_ID,
		.api = xa_drm_dec,
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_MP3_DEC
	{
		.id = CADENCE_CODEC_MP3_DEC_ID,
		.api = xa_mp3_dec,
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_MP3_ENC
	{
		.id = CADENCE_CODEC_MP3_ENC_ID,
		.api = xa_mp3_enc,
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_SBC_DEC
	{
		.id = CADENCE_CODEC_SBC_DEC_ID,
		.api = xa_sbc_dec,
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_VORBIS_DEC
	{
		.id = CADENCE_CODEC_VORBIS_DEC_ID,
		.api = xa_vorbis_dec,
	},
#endif
#ifdef CONFIG_CADENCE_CODEC_SRC_PP
	{
		.id = CADENCE_CODEC_SRC_PP_ID,
		.api = xa_src_pp,
	},
#endif
};

static int cadence_code_get_api_id(uint32_t compress_id)
{
	/* convert compress id to SOF cadence SOF id */
	switch (compress_id) {
	case SND_AUDIOCODEC_MP3:
		return CADENCE_CODEC_MP3_DEC_ID;
	case SND_AUDIOCODEC_AAC:
		return CADENCE_CODEC_AAC_DEC_ID;
	case SND_AUDIOCODEC_VORBIS:
		return CADENCE_CODEC_VORBIS_DEC_ID;
	default:
		return -EINVAL;
	}
}

#if CONFIG_IPC_MAJOR_4
static int cadence_codec_resolve_api(struct processing_module *mod)
{
	int ret;
	struct snd_codec codec_params;
	struct comp_dev *dev = mod->dev;
	struct cadence_codec_data *cd = module_get_private_data(mod);
	uint32_t api_id = CODEC_GET_API_ID(DEFAULT_CODEC_ID);
	uint32_t n_apis = ARRAY_SIZE(cadence_api_table);
	struct module_data *codec = &mod->priv;
	struct module_param *param;
	int i;
	xa_codec_func_t *api = NULL;

	/* For ipc4 protocol codec parameters has to be retrieved from configuration */
	if (!codec->cfg.data) {
		comp_err(dev, "cadence_codec_resolve_api(): could not find cadence config");
		return -EINVAL;
	}
	param = codec->cfg.data;
	api_id = param->id >> 16;

	/* Find and assign API function */
	for (i = 0; i < n_apis; i++) {
		if (cadence_api_table[i].id == api_id) {
			api = cadence_api_table[i].api;
			break;
		}
	}

	/* Verify API assignment */
	if (!api) {
		comp_err(dev, "cadence_codec_resolve_api(): could not find API function for id %x",
			 api_id);
		return -EINVAL;
	}
	cd->api = api;
	cd->api_id = api_id;

	return 0;
}

#elif CONFIG_IPC_MAJOR_3
static int cadence_codec_resolve_api(struct processing_module *mod)
{
	int ret;
	struct snd_codec codec_params;
	struct comp_dev *dev = mod->dev;
	struct cadence_codec_data *cd = module_get_private_data(mod);
	uint32_t api_id = CODEC_GET_API_ID(DEFAULT_CODEC_ID);
	uint32_t n_apis = ARRAY_SIZE(cadence_api_table);
	int i;
	xa_codec_func_t *api = NULL;

	if (mod->stream_params->ext_data_length) {
		ret = memcpy_s(&codec_params, mod->stream_params->ext_data_length,
			       (uint8_t *)mod->stream_params + sizeof(*mod->stream_params),
			       mod->stream_params->ext_data_length);
		if (ret < 0)
			return ret;

		ret = cadence_code_get_api_id(codec_params.id);
		if (ret < 0)
			return ret;

		api_id = ret;
	}

	/* Find and assign API function */
	for (i = 0; i < n_apis; i++) {
		if (cadence_api_table[i].id == api_id) {
			api = cadence_api_table[i].api;
			break;
		}
	}

	/* Verify API assignment */
	if (!api) {
		comp_err(dev, "cadence_codec_resolve_api(): could not find API function for id %x",
			 api_id);
		return -EINVAL;
	}
	cd->api = api;
	cd->api_id = api_id;

	return 0;
}
#else
#error Unknown IPC major version
#endif

static int cadence_codec_post_init(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct cadence_codec_data *cd = module_get_private_data(mod);
	uint32_t obj_size;

	comp_dbg(dev, "cadence_codec_post_init() start");

	ret = cadence_codec_resolve_api(mod);
	if (ret < 0)
		return ret;

	/* Obtain codec name */
	API_CALL(cd, XA_API_CMD_GET_LIB_ID_STRINGS,
		 XA_CMD_TYPE_LIB_NAME, cd->name, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_init() error %x: failed to get lib name",
			 ret);
		return ret;
	}
	/* Get codec object size */
	API_CALL(cd, XA_API_CMD_GET_API_SIZE, 0, &obj_size, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_init() error %x: failed to get lib object size",
			 ret);
		return ret;
	}
	/* Allocate space for codec object */
	cd->self = rballoc(0, SOF_MEM_CAPS_RAM, obj_size);
	if (!cd->self) {
		comp_err(dev, "cadence_codec_init(): failed to allocate space for lib object");
		return -ENOMEM;
	}

	comp_dbg(dev, "cadence_codec_post_init(): allocated %d bytes for lib object", obj_size);

	/* Set all params to their default values */
	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_API_PRE_CONFIG_PARAMS,
		 NULL, ret);
	if (ret != LIB_NO_ERROR) {
		rfree(cd->self);
		return ret;
	}

	comp_dbg(dev, "cadence_codec_post_init() done");

	return 0;
}

#if CONFIG_IPC_MAJOR_4
static int cadence_codec_init(struct processing_module *mod)
{
	const struct ipc4_cadence_module_cfg *cfg;
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd;
	struct module_config *setup_cfg;
	struct comp_dev *dev = mod->dev;
	int ret;

	comp_dbg(dev, "cadence_codec_init() start");

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(struct cadence_codec_data));
	if (!cd) {
		comp_err(dev, "cadence_codec_init(): failed to allocate memory for cadence codec data");
		return -ENOMEM;
	}

	codec->private = cd;
	codec->mpd.init_done = 0;

	/* copy the setup config only for the first init */
	if (codec->state == MODULE_DISABLED && codec->cfg.avail) {
		setup_cfg = &cd->setup_cfg;

		cfg = (const struct ipc4_cadence_module_cfg *)codec->cfg.init_data;

		/* allocate memory for set up config */
		setup_cfg->data = rmalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
					  cfg->param_size);
		if (!setup_cfg->data) {
			comp_err(dev, "cadence_codec_init(): failed to alloc setup config");
			ret = -ENOMEM;
			goto free;
		}

		/* allocate memory for runtime set up config */
		codec->cfg.data = rmalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
					  cfg->param_size);
		if (!codec->cfg.data) {
			comp_err(dev, "cadence_codec_init(): failed to alloc runtime setup config");
			ret = -ENOMEM;
			goto free_cfg;
		}

		codec->cfg.size = cfg->param_size;
		ret = memcpy_s(codec->cfg.data, codec->cfg.size,
			       cfg->param, cfg->param_size);
		if (ret) {
			comp_err(dev, "cadence_codec_init(): failed to init runtime config %d",
				 ret);
			goto free_cfg2;
		}
		codec->cfg.avail = true;

		setup_cfg->size = cfg->param_size;
		ret = memcpy_s(setup_cfg->data, setup_cfg->size,
			       cfg->param, cfg->param_size);
		if (ret) {
			comp_err(dev, "cadence_codec_init(): failed to copy setup config %d", ret);
			goto free_cfg2;
		}
		setup_cfg->avail = true;
	}

	comp_dbg(dev, "cadence_codec_init() done");

	return 0;

free_cfg2:
	rfree(codec->cfg.data);
free_cfg:
	rfree(setup_cfg->data);
free:
	rfree(cd);
	return ret;
}

#elif CONFIG_IPC_MAJOR_3
static int cadence_codec_init(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd;
	struct comp_dev *dev = mod->dev;
	struct module_config *setup_cfg;
	int ret;

	comp_dbg(dev, "cadence_codec_init() start");

	cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(struct cadence_codec_data));
	if (!cd) {
		comp_err(dev, "cadence_codec_init(): failed to allocate memory for cadence codec data");
		return -ENOMEM;
	}

	codec->private = cd;
	codec->mpd.init_done = 0;

	/* copy the setup config only for the first init */
	if (codec->state == MODULE_DISABLED && codec->cfg.avail) {
		setup_cfg = &cd->setup_cfg;

		/* allocate memory for set up config */
		setup_cfg->data = rmalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
					  codec->cfg.size);
		if (!setup_cfg->data) {
			comp_err(dev, "cadence_codec_init(): failed to alloc setup config");
			ret = -ENOMEM;
			goto free;
		}

		/* copy the setup config */
		setup_cfg->size = codec->cfg.size;
		ret = memcpy_s(setup_cfg->data, setup_cfg->size,
			       codec->cfg.init_data, setup_cfg->size);
		if (ret) {
			comp_err(dev, "cadence_codec_init(): failed to copy setup config %d", ret);
			goto free_cfg;
		}
		setup_cfg->avail = true;
	}

	comp_dbg(dev, "cadence_codec_init() done");

	return 0;

free_cfg:
	rfree(setup_cfg->data);
free:
	rfree(cd);
	return ret;
}

#else
#error Unknown IPC major version
#endif

static int cadence_codec_apply_config(struct processing_module *mod)
{
	int ret = 0;
	int size;
	uint16_t param_id;
	uint16_t codec_id;
	struct module_config *cfg;
	void *data;
	struct module_param *param;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;

	comp_dbg(dev, "cadence_codec_apply_config() start");

	cfg = &codec->cfg;

	/* use setup config if no runtime config available. This will be true during reset */
	if (!cfg->avail)
		cfg = &cd->setup_cfg;

	data = cfg->data;
	size = cfg->size;

	if (!cfg->avail || !size) {
		comp_err(dev, "cadence_codec_apply_config() error: no config available");
		return -EIO;
	}

	/* Read parameters stored in `data` - it may keep plenty of
	 * parameters. The `size` variable is equal to param->size * count,
	 * where count is number of parameters stored in `data`.
	 */
	while (size > 0) {
		param = data;
		comp_dbg(dev, "cadence_codec_apply_config() applying param %d value %d",
			 param->id, param->data[0]);

		param_id = param->id & 0xFF;
		codec_id = param->id >> 16;

		/* if the parameter is not for current codec skip it! */
		if (codec_id && codec_id != cd->api_id) {
			/* Obtain next parameter */
			data = (char *)data + param->size;
			size -= param->size;
			continue;
		}

		/* Set read parameter */
		API_CALL(cd, XA_API_CMD_SET_CONFIG_PARAM, param_id,
			 param->data, ret);
		if (ret != LIB_NO_ERROR) {
			if (LIB_IS_FATAL_ERROR(ret)) {
				comp_err(dev, "cadence_codec_apply_config(): failed to apply parameter: %d value: %d error: %#x",
					 param->id, *(int32_t *)param->data, ret);

				return ret;
			}
			comp_warn(dev, "cadence_codec_apply_config(): applied parameter %d value %d with return code: %#x",
				  param->id, *(int32_t *)param->data, ret);
		}
		/* Obtain next parameter, it starts right after the preceding one */
		data = (char *)data + param->size;
		size -= param->size;
	}

	comp_dbg(dev, "cadence_codec_apply_config() done");

	return 0;
}

static int init_memory_tables(struct processing_module *mod)
{
	int ret, no_mem_tables, i, mem_type, mem_size, mem_alignment;
	void *ptr, *scratch, *persistent;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;

	scratch = NULL;
	persistent = NULL;

	/* Calculate the size of all memory blocks required */
	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_API_POST_CONFIG_PARAMS,
		 NULL, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "init_memory_tables() error %x: failed to calculate memory blocks size",
			 ret);
		return ret;
	}

	/* Get number of memory tables */
	API_CALL(cd, XA_API_CMD_GET_N_MEMTABS, 0, &no_mem_tables, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "init_memory_tables() error %x: failed to get number of memory tables",
			 ret);
		return ret;
	}

	/* Initialize each memory table */
	for (i = 0; i < no_mem_tables; i++) {
		/* Get type of memory - it specifies how the memory will be used */
		API_CALL(cd, XA_API_CMD_GET_MEM_INFO_TYPE, i, &mem_type, ret);
		if (ret != LIB_NO_ERROR) {
			comp_err(dev, "init_memory_tables() error %x: failed to get mem. type info of id %d out of %d",
				 ret, i, no_mem_tables);
			goto err;
		}
		/* Get size of memory needed for memory allocation for this
		 * particular memory type.
		 */
		API_CALL(cd, XA_API_CMD_GET_MEM_INFO_SIZE, i, &mem_size, ret);
		if (ret != LIB_NO_ERROR) {
			comp_err(dev, "init_memory_tables() error %x: failed to get mem. size for mem. type %d",
				 ret, mem_type);
			goto err;
		}
		/* Get alignment constrains */
		API_CALL(cd, XA_API_CMD_GET_MEM_INFO_ALIGNMENT, i, &mem_alignment, ret);
		if (ret != LIB_NO_ERROR) {
			comp_err(dev, "init_memory_tables() error %x: failed to get mem. alignment of mem. type %d",
				 ret, mem_type);
			goto err;
		}
		/* Allocate memory for this type, taking alignment into account */
		ptr = module_allocate_memory(mod, mem_size, mem_alignment);
		if (!ptr) {
			comp_err(dev, "init_memory_tables() error %x: failed to allocate memory for %d",
				 ret, mem_type);
			ret = -EINVAL;
			goto err;
		}
		/* Finally, provide this memory for codec */
		API_CALL(cd, XA_API_CMD_SET_MEM_PTR, i, ptr, ret);
		if (ret != LIB_NO_ERROR) {
			comp_err(dev, "init_memory_tables() error %x: failed to set memory pointer for %d",
				 ret, mem_type);
			goto err;
		}

		switch ((unsigned int)mem_type) {
		case XA_MEMTYPE_SCRATCH:
			scratch = ptr;
			break;
		case XA_MEMTYPE_PERSIST:
			persistent = ptr;
			break;
		case XA_MEMTYPE_INPUT:
			codec->mpd.in_buff = ptr;
			codec->mpd.in_buff_size = mem_size;
			break;
		case XA_MEMTYPE_OUTPUT:
			codec->mpd.out_buff = ptr;
			codec->mpd.out_buff_size = mem_size;
			break;
		default:
			comp_err(dev, "init_memory_tables() error %x: unrecognized memory type!",
				 mem_type);
			ret = -EINVAL;
			goto err;
		}

		comp_dbg(dev, "init_memory_tables: allocated memory of %d bytes and alignment %d for mem. type %d",
			 mem_size, mem_alignment, mem_type);
	}

	return 0;
err:
	if (scratch)
		module_free_memory(mod, scratch);
	if (persistent)
		module_free_memory(mod, persistent);
	if (codec->mpd.in_buff)
		module_free_memory(mod, codec->mpd.in_buff);
	if (codec->mpd.out_buff)
		module_free_memory(mod, codec->mpd.out_buff);
	return ret;
}

static int cadence_codec_get_samples(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "cadence_codec_get_samples() start");

	switch (cd->api_id) {
	case CADENCE_CODEC_WRAPPER_ID:
		return 0;
	case CADENCE_CODEC_MP3_DEC_ID:
		/* MPEG-1 Layer 3 */
		return 1152;
	case CADENCE_CODEC_AAC_DEC_ID:
		return 1024;
	default:
		break;
	}

	return 0;
}

static int cadence_codec_deep_buff_allowed(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);

	switch (cd->api_id) {
	case CADENCE_CODEC_MP3_ENC_ID:
		return 0;
	default:
		return 1;
	}
}

static int cadence_codec_init_process(struct processing_module *mod)
{
	int ret;
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;
	struct comp_dev *dev = mod->dev;

	API_CALL(cd, XA_API_CMD_SET_INPUT_BYTES, 0, &codec->mpd.avail, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_init_process() error %x: failed to set size of input data",
			 ret);
		return ret;
	}

	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_PROCESS, NULL, ret);
	if (LIB_IS_FATAL_ERROR(ret)) {
		comp_err(dev, "cadence_codec_init_process() error %x: failed to initialize codec",
			 ret);
		return ret;
	} else if (ret != LIB_NO_ERROR) {
		/* for SOF with native Zephyr, the first chunk of data will be
		 * 0s since data is first transferred from host to the next
		 * component and **then** from Linux to host. Because of this, the
		 * above API call will return `...NONFATAL_NEXT_SYNC_NOT_FOUND`
		 * since the API seems to expect useful data in the first chunk.
		 * To avoid this, print a warning if the above call returns
		 * a non-fatal error and let the init process continue. Next
		 * chunk will contain the useful data.
		 */
		comp_warn(dev, "cadence_codec_init_process() returned non-fatal error: 0x%x",
			  ret);
	}

	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_DONE_QUERY,
		 &codec->mpd.init_done, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_init_process() error %x: failed to get lib init status",
			 ret);
		return ret;
	}

	API_CALL(cd, XA_API_CMD_GET_CURIDX_INPUT_BUF, 0, &codec->mpd.consumed, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_init_process() error %x: could not get consumed bytes",
			 ret);
		return ret;
	}

	return 0;
}

static int cadence_codec_prepare(struct processing_module *mod,
				 struct sof_source **sources, int num_of_sources,
				 struct sof_sink **sinks, int num_of_sinks)
{
	int ret = 0, mem_tabs_size;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;

	comp_dbg(dev, "cadence_codec_prepare() start");

	ret = cadence_codec_post_init(mod);
	if (ret)
		return ret;

	ret = cadence_codec_apply_config(mod);
	if (ret) {
		comp_err(dev, "cadence_codec_prepare() error %x: failed to apply config",
			 ret);
		return ret;
	}

	/* Allocate memory for the codec */
	API_CALL(cd, XA_API_CMD_GET_MEMTABS_SIZE, 0, &mem_tabs_size, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_prepare() error %x: failed to get memtabs size",
			 ret);
		return ret;
	}

	cd->mem_tabs = module_allocate_memory(mod, mem_tabs_size, 4);
	if (!cd->mem_tabs) {
		comp_err(dev, "cadence_codec_prepare() error: failed to allocate space for memtabs");
		return -ENOMEM;
	}

	comp_dbg(dev, "cadence_codec_prepare(): allocated %d bytes for memtabs", mem_tabs_size);

	API_CALL(cd, XA_API_CMD_SET_MEMTABS_PTR, 0, cd->mem_tabs, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_prepare() error %x: failed to set memtabs",
			 ret);
		goto free;
	}

	ret = init_memory_tables(mod);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_prepare() error %x: failed to init memory tables",
			 ret);
		goto free;
	}
	/* Check init done status. Note, it may happen that init_done flag will return
	 * false value, this is normal since some codec variants needs input in order to
	 * fully finish initialization. That's why at codec_adapter_copy() we call
	 * codec_init_process() base on result obtained below.
	 */
#ifdef CONFIG_CADENCE_CODEC_WRAPPER
	/* TODO: remove the "#ifdef CONFIG_CADENCE_CODEC_WRAPPER" once cadence fixes the bug
	 * in the init/prepare sequence. Basically below API_CALL shall return 1 for
	 * PCM streams and 0 for compress ones. As it turns out currently it returns 1
	 * in both cases so in turn compress stream won't finish its prepare during first copy
	 * in codec_adapter_copy().
	 */
	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_DONE_QUERY,
		 &codec->mpd.init_done, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_init_process() error %x: failed to get lib init status",
			 ret);
		return ret;
	}
#endif
	comp_dbg(dev, "cadence_codec_prepare() done");
	return 0;
free:
	module_free_memory(mod, cd->mem_tabs);
	return ret;
}

static int
cadence_codec_process(struct processing_module *mod,
		      struct input_stream_buffer *input_buffers, int num_input_buffers,
		      struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_buffer *local_buff;
	struct comp_dev *dev = mod->dev;
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;
	int free_bytes, output_bytes = cadence_codec_get_samples(mod) *
				mod->stream_params->sample_container_bytes *
				mod->stream_params->channels;
	uint32_t remaining = input_buffers[0].size;
	int ret;

	if (!cadence_codec_deep_buff_allowed(mod)) {
		mod->deep_buff_bytes = 0;
	}

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (input_buffers[0].size < codec->mpd.in_buff_size) {
		comp_dbg(dev, "cadence_codec_process(): not enough data to process");
		return -ENODATA;
	}

	if (!codec->mpd.init_done) {
		memcpy_s(codec->mpd.in_buff, codec->mpd.in_buff_size, input_buffers[0].data,
			 codec->mpd.in_buff_size);
		codec->mpd.avail = codec->mpd.in_buff_size;

		ret = cadence_codec_init_process(mod);
		if (ret)
			return ret;

		remaining -= codec->mpd.consumed;
		input_buffers[0].consumed = codec->mpd.consumed;
	}

	/* do not proceed with processing if not enough free space left in the local buffer */
	local_buff = list_first_item(&mod->raw_data_buffers_list, struct comp_buffer, buffers_list);
	free_bytes = audio_stream_get_free(&local_buff->stream);
	if (free_bytes < output_bytes)
		return -ENOSPC;

	/* Proceed only if we have enough data to fill the module buffer completely */
	if (remaining < codec->mpd.in_buff_size)
		return -ENODATA;

	memcpy_s(codec->mpd.in_buff, codec->mpd.in_buff_size,
		 (uint8_t *)input_buffers[0].data + input_buffers[0].consumed,
		 codec->mpd.in_buff_size);
	codec->mpd.avail = codec->mpd.in_buff_size;

	comp_dbg(dev, "cadence_codec_process() start");

	API_CALL(cd, XA_API_CMD_SET_INPUT_BYTES, 0, &codec->mpd.avail, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_process() error %x: failed to set size of input data",
			 ret);
		return ret;
	}

	API_CALL(cd, XA_API_CMD_EXECUTE, XA_CMD_TYPE_DO_EXECUTE, NULL, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "cadence_codec_process() error %x: processing failed",
				 ret);
			return ret;
		}
		comp_warn(dev, "cadence_codec_process() nonfatal error %x", ret);
	}

	API_CALL(cd, XA_API_CMD_GET_OUTPUT_BYTES, 0, &codec->mpd.produced, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_process() error %x: could not get produced bytes",
			 ret);
		return ret;
	}

	API_CALL(cd, XA_API_CMD_GET_CURIDX_INPUT_BUF, 0, &codec->mpd.consumed, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "cadence_codec_process() error %x: could not get consumed bytes",
			 ret);
		return ret;
	}

	/* update consumed with the number of samples consumed during init */
	input_buffers[0].consumed += codec->mpd.consumed;
	codec->mpd.consumed = input_buffers[0].consumed;

	/* copy the produced samples into the output buffer */
	memcpy_s(output_buffers[0].data, codec->mpd.produced, codec->mpd.out_buff,
		 codec->mpd.produced);
	output_buffers[0].size = codec->mpd.produced;

	comp_dbg(dev, "cadence_codec_process() done");

	return 0;
}

static int cadence_codec_reset(struct processing_module *mod)
{
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;
	int ret;

	/*
	 * Current CADENCE API doesn't support reset of codec's runtime parameters.
	 * So, free all memory associated with runtime params. These will be reallocated during
	 * prepare.
	 */
	module_free_all_memory(mod);

	/* reset to default params */
	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_API_PRE_CONFIG_PARAMS, NULL, ret);
	if (ret != LIB_NO_ERROR)
		return ret;

	codec->mpd.init_done = 0;

	rfree(cd->self);
	cd->self = NULL;

	return ret;
}

static int cadence_codec_free(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);

	rfree(cd->setup_cfg.data);
	module_free_all_memory(mod);
	rfree(cd->self);
	rfree(cd);
	return 0;
}

static int
cadence_codec_set_configuration(struct processing_module *mod, uint32_t config_id,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				size_t response_size)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	ret = module_set_configuration(mod, config_id, pos, data_offset_size, fragment,
				       fragment_size, response, response_size);
	if (ret < 0)
		return ret;

	/* return if more fragments are expected or if the module is not prepared */
	if ((pos != MODULE_CFG_FRAGMENT_LAST && pos != MODULE_CFG_FRAGMENT_SINGLE) ||
	    md->state < MODULE_IDLE)
		return 0;

	/* whole configuration received, apply it now */
	ret = cadence_codec_apply_config(mod);
	if (ret) {
		comp_err(dev, "cadence_codec_set_configuration(): error %x: runtime config apply failed",
			 ret);
		return ret;
	}

	comp_dbg(dev, "cadence_codec_set_configuration(): config applied");

	return 0;
}

static const struct module_interface cadence_codec_interface = {
	.init = cadence_codec_init,
	.prepare = cadence_codec_prepare,
	.process_raw_data = cadence_codec_process,
	.set_configuration = cadence_codec_set_configuration,
	.reset = cadence_codec_reset,
	.free = cadence_codec_free
};

DECLARE_MODULE_ADAPTER(cadence_codec_interface, cadence_codec_uuid, cadence_codec_tr);
SOF_MODULE_INIT(cadence_codec, sys_comp_module_cadence_codec_interface_init);
