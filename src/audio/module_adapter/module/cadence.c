// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 - 2026 Intel Corporation. All rights reserved.
//

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/module_adapter/module/cadence.h>
#include <ipc/compress_params.h>
#include <rtos/init.h>

LOG_MODULE_REGISTER(cadence_codec, CONFIG_SOF_LOG_LEVEL);

/*****************************************************************************/
/* Cadence API functions array						     */
/*****************************************************************************/
struct cadence_api cadence_api_table[] = {
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

static int cadence_codec_get_api_id(uint32_t compress_id, uint32_t direction)
{
	/* convert compress id to SOF cadence SOF id */
	switch (compress_id) {
	case SND_AUDIOCODEC_MP3:
		if (direction == SOF_IPC_STREAM_PLAYBACK)
			return CADENCE_CODEC_MP3_DEC_ID;

		return CADENCE_CODEC_MP3_ENC_ID;
	case SND_AUDIOCODEC_AAC:
		return CADENCE_CODEC_AAC_DEC_ID;
	case SND_AUDIOCODEC_VORBIS:
		return CADENCE_CODEC_VORBIS_DEC_ID;
	default:
		return -EINVAL;
	}
}

void cadence_codec_free_memory_tables(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	int i;

	if (cd->mem_to_be_freed)
		for (i = 0; i < cd->mem_to_be_freed_len; i++)
			mod_free(mod, cd->mem_to_be_freed[i]);

	mod_free(mod, cd->mem_to_be_freed);
	cd->mem_to_be_freed = NULL;
	cd->mem_to_be_freed_len = 0;
}

int cadence_codec_init_memory_tables(struct processing_module *mod)
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
		comp_err(dev, "error %x: failed to calculate memory blocks size",
			 ret);
		return ret;
	}

	/* Get number of memory tables */
	API_CALL(cd, XA_API_CMD_GET_N_MEMTABS, 0, &no_mem_tables, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "error %x: failed to get number of memory tables",
			 ret);
		return ret;
	}

	cd->mem_to_be_freed = mod_zalloc(mod, no_mem_tables * sizeof(*cd->mem_to_be_freed));
	if (!cd->mem_to_be_freed)
		return -ENOMEM;
	cd->mem_to_be_freed_len = no_mem_tables;

	/* Initialize each memory table */
	for (i = 0; i < no_mem_tables; i++) {
		/* Get type of memory - it specifies how the memory will be used */
		API_CALL(cd, XA_API_CMD_GET_MEM_INFO_TYPE, i, &mem_type, ret);
		if (ret != LIB_NO_ERROR) {
			comp_err(dev, "error %x: failed to get mem. type info of id %d out of %d",
				 ret, i, no_mem_tables);
			goto err;
		}
		/* Get size of memory needed for memory allocation for this
		 * particular memory type.
		 */
		API_CALL(cd, XA_API_CMD_GET_MEM_INFO_SIZE, i, &mem_size, ret);
		if (ret != LIB_NO_ERROR) {
			comp_err(dev, "error %x: failed to get mem. size for mem. type %d",
				 ret, mem_type);
			goto err;
		}
		/* Get alignment constrains */
		API_CALL(cd, XA_API_CMD_GET_MEM_INFO_ALIGNMENT, i, &mem_alignment, ret);
		if (ret != LIB_NO_ERROR) {
			comp_err(dev, "error %x: failed to get mem. alignment of mem. type %d",
				 ret, mem_type);
			goto err;
		}
		/* Allocate memory for this type, taking alignment into account */
		ptr = mod_alloc_align(mod, mem_size, mem_alignment);
		if (!ptr) {
			comp_err(dev, "error %x: failed to allocate memory for %d",
				 ret, mem_type);
			ret = -EINVAL;
			goto err;
		}
		cd->mem_to_be_freed[i] = ptr;
		/* Finally, provide this memory for codec */
		API_CALL(cd, XA_API_CMD_SET_MEM_PTR, i, ptr, ret);
		if (ret != LIB_NO_ERROR) {
			comp_err(dev, "error %x: failed to set memory pointer for %d",
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
			comp_err(dev, "error %x: unrecognized memory type!",
				 mem_type);
			ret = -EINVAL;
			goto err;
		}

		comp_dbg(dev, "allocated memory of %d bytes and alignment %d for mem. type %d",
			 mem_size, mem_alignment, mem_type);
	}

	return 0;
err:
	cadence_codec_free_memory_tables(mod);

	return ret;
}

size_t cadence_api_table_size(void)
{
	return ARRAY_SIZE(cadence_api_table);
}

int cadence_codec_get_samples(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;

	comp_dbg(dev, "start");

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

int cadence_codec_init_process(struct processing_module *mod)
{
	int ret;
	struct module_data *codec = &mod->priv;
	struct cadence_codec_data *cd = codec->private;
	struct comp_dev *dev = mod->dev;

	codec->mpd.eos_reached = false;
	codec->mpd.eos_notification_sent = false;

	API_CALL(cd, XA_API_CMD_SET_INPUT_BYTES, 0, &codec->mpd.avail, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "error %x: failed to set size of input data",
			 ret);
		return ret;
	}

	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_PROCESS, NULL, ret);
	if (LIB_IS_FATAL_ERROR(ret)) {
		comp_err(dev, "error %x: failed to initialize codec",
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
		comp_warn(dev, "returned non-fatal error: 0x%x",
			  ret);
	}

	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_DONE_QUERY,
		 &codec->mpd.init_done, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "error %x: failed to get lib init status",
			 ret);
		return ret;
	}

	API_CALL(cd, XA_API_CMD_GET_CURIDX_INPUT_BUF, 0, &codec->mpd.consumed, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "error %x: could not get consumed bytes",
			 ret);
		return ret;
	}

	return 0;
}

int cadence_codec_free(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);

	mod_free(mod, cd->setup_cfg.data);

	cadence_codec_free_memory_tables(mod);
	mod_free(mod, cd->mem_tabs);

	mod_free(mod, cd->self);
	mod_free(mod, cd);
	return 0;
}

int cadence_codec_set_configuration(struct processing_module *mod, uint32_t config_id,
				    enum module_cfg_fragment_position pos,
				    uint32_t data_offset_size, const uint8_t *fragment,
				    size_t fragment_size, uint8_t *response, size_t response_size)
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
		comp_err(dev, "runtime config apply failed with error %x: ", ret);
		return ret;
	}

	comp_dbg(dev, "config applied");

	return 0;
}

int cadence_codec_apply_params(struct processing_module *mod, int size, void *data)
{
	struct module_data *codec = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct cadence_codec_data *cd = codec->private;
	struct module_param *param;
	uint16_t param_id;
	uint16_t codec_id;
	int ret;

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
				comp_err(dev, "failed to apply parameter: %d value: %d error: %#x",
					 param->id, *(int32_t *)param->data, ret);

				return ret;
			}
			comp_warn(dev, "applied parameter %d value %d with return code: %#x",
				  param->id, *(int32_t *)param->data, ret);
		}
		/* Obtain next parameter, it starts right after the preceding one */
		data = (char *)data + param->size;
		size -= param->size;
	}

	return 0;
}

int cadence_init_codec_object(struct processing_module *mod)
{
	int ret;
	struct comp_dev *dev = mod->dev;
	struct cadence_codec_data *cd = module_get_private_data(mod);
	uint32_t obj_size;

	ret = cadence_codec_resolve_api(mod);
	if (ret < 0)
		return ret;

	/* Obtain codec name */
	API_CALL(cd, XA_API_CMD_GET_LIB_ID_STRINGS,
		 XA_CMD_TYPE_LIB_NAME, cd->name, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "failed to get lib name error: %x: ", ret);
		return ret;
	}
	/* Get codec object size */
	API_CALL(cd, XA_API_CMD_GET_API_SIZE, 0, &obj_size, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "failed to get lib object size error %x:", ret);
		return ret;
	}
	/* Allocate space for codec object */
	cd->self = mod_balloc(mod, obj_size);
	if (!cd->self) {
		comp_err(dev, "failed to allocate space for lib object");
		return -ENOMEM;
	}

	comp_dbg(dev, "allocated %d bytes for lib object", obj_size);

	/* Set all params to their default values */
	API_CALL(cd, XA_API_CMD_INIT, XA_CMD_TYPE_INIT_API_PRE_CONFIG_PARAMS,
		 NULL, ret);
	if (ret != LIB_NO_ERROR) {
		mod_free(mod, cd->self);
		return ret;
	}

	return 0;
}

int cadence_codec_resolve_api_with_id(struct processing_module *mod, uint32_t codec_id,
				      uint32_t direction)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	uint32_t api_id;
	uint32_t n_apis = cadence_api_table_size();
	xa_codec_func_t *api = NULL;
	int i;

	api_id = cadence_codec_get_api_id(codec_id, direction);
	if (api_id < 0)
		return api_id;

	/* Find and assign API function */
	for (i = 0; i < n_apis; i++) {
		if (cadence_api_table[i].id == api_id) {
			api = cadence_api_table[i].api;
			break;
		}
	}

	/* Verify API assignment */
	if (!api) {
		comp_err(dev, "could not find API function for id %x",
			 api_id);
		return -EINVAL;
	}
	cd->api = api;
	cd->api_id = api_id;

	return 0;
}

int cadence_codec_process_data(struct processing_module *mod)
{
	struct cadence_codec_data *cd = module_get_private_data(mod);
	struct module_data *codec = &mod->priv;
	struct comp_dev *dev = mod->dev;
	int ret;

	if (codec->mpd.eos_reached) {
		codec->mpd.produced = 0;
		codec->mpd.consumed = 0;

		return 0;
	}

	API_CALL(cd, XA_API_CMD_SET_INPUT_BYTES, 0, &codec->mpd.avail, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "failed to set size of input data with error: %x:", ret);
		return ret;
	}

	API_CALL(cd, XA_API_CMD_EXECUTE, XA_CMD_TYPE_DO_EXECUTE, NULL, ret);
	if (ret != LIB_NO_ERROR) {
		if (LIB_IS_FATAL_ERROR(ret)) {
			comp_err(dev, "processing failed with error: %x", ret);
			return ret;
		}
		comp_warn(dev, "processing failed with nonfatal error: %x", ret);
	}

	API_CALL(cd, XA_API_CMD_GET_OUTPUT_BYTES, 0, &codec->mpd.produced, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "could not get produced bytes, error %x:",
			 ret);
		return ret;
	}

	API_CALL(cd, XA_API_CMD_GET_CURIDX_INPUT_BUF, 0, &codec->mpd.consumed, ret);
	if (ret != LIB_NO_ERROR) {
		comp_err(dev, "could not get consumed bytes, error: %x", ret);
		return ret;
	}

	if (!codec->mpd.produced && dev->pipeline->expect_eos)
		codec->mpd.eos_reached = true;

	return 0;
}
