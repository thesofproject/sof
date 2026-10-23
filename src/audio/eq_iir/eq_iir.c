// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017-2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include "eq_iir.h"
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/sink_source_utils.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/iir_df1.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(eq_iir, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(eq_iir);

static int eq_iir_blob_words_max(struct comp_dev *dev,
				 const struct sof_eq_iir_config *config,
				 size_t blob_size,
				 uint32_t *coef_words_max)
{
	size_t payload_bytes;

	/* Compute the size of the coefficient area in int32_t words from the
	 * framework-reported blob size. The blob layout is:
	 *   sizeof(*config) header bytes
	 *   channels_in_config int32_t assign_response[]
	 *   coefficient data[]
	 * channels_in_config is bounded above, so the multiply fits in size_t.
	 * The blob's self-declared config->size is cross-checked against the
	 * authoritative blob_size so all later parsing stays within the buffer.
	 */
	if (blob_size < sizeof(*config) || config->size != blob_size) {
		comp_err(dev, "blob size %zu / header size %u mismatch or too small",
			 blob_size, config->size);
		return -EINVAL;
	}
	payload_bytes = blob_size - sizeof(*config);
	if (payload_bytes % sizeof(int32_t) ||
	    payload_bytes < (size_t)config->channels_in_config * sizeof(int32_t)) {
		comp_err(dev, "blob size %zu misaligned or too small", blob_size);
		return -EINVAL;
	}
	*coef_words_max = payload_bytes / sizeof(int32_t) - config->channels_in_config;
	return 0;
}

/**
 * @brief Parse one response header from the coefficient blob.
 *
 * Reads the response at word offset *j in @p coef_data, checks that both
 * the fixed header and the following biquad sections fit inside the
 * @p coef_words_max blob budget, and advances *j past this response so
 * the next call continues where this one left off. On success @p *eq_out
 * is set to point at the in-place header; callers that only want to walk
 * the blob for validation can discard that pointer.
 *
 * @param dev            Component device, used for error logging.
 * @param idx            Response index, used in error messages.
 * @param coef_data      Base of the coefficient word array in the blob.
 * @param coef_words_max Total words available in @p coef_data.
 * @param j              In/out cursor in words; advanced past this response.
 * @param eq_out         Out; pointer to the parsed response header.
 *
 * @return 0 on success, -EINVAL if the header or sections would run off
 *         the end of the blob or num_sections exceeds the platform limit.
 */
static int eq_iir_init_response(struct comp_dev *dev, int idx,
				int32_t *coef_data, uint32_t coef_words_max,
				uint32_t *j, struct sof_eq_iir_header **eq_out)
{
	struct sof_eq_iir_header *eq;
	uint32_t header_end = *j + SOF_EQ_IIR_NHEADER;
	uint32_t section_end;

	/* Header must fit before reading num_sections */
	if (header_end > coef_words_max) {
		comp_err(dev, "response %d header out of bounds", idx);
		return -EINVAL;
	}
	eq = (struct sof_eq_iir_header *)&coef_data[*j];
	/* Bound num_sections so the multiply cannot overflow and the section
	 * data stays within the blob.
	 */
	section_end = header_end + (uint32_t)SOF_EQ_IIR_NBIQUAD * eq->num_sections;
	if (eq->num_sections > SOF_EQ_IIR_BIQUADS_MAX || section_end > coef_words_max) {
		comp_err(dev, "response %d num_sections %u out of bounds",
			 idx, eq->num_sections);
		return -EINVAL;
	}
	*eq_out = eq;
	*j = section_end;
	return 0;
}

/* Validate the config blob layout and, if lookup is non-NULL, populate it
 * with pointers to each response header. Pass lookup = NULL to validate only.
 */
static int eq_iir_walk_config(struct comp_dev *dev,
			      struct sof_eq_iir_config *config,
			      size_t config_size,
			      struct sof_eq_iir_header **lookup)
{
	struct sof_eq_iir_header *eq;
	uint32_t coef_words_max;
	int32_t *coef_data;
	int ret;
	int i;
	uint32_t j;

	if (config->channels_in_config > CONFIG_PLATFORM_MAX_CHANNELS ||
	    !config->channels_in_config) {
		comp_err(dev, "invalid channels_in_config %u", config->channels_in_config);
		return -EINVAL;
	}
	if (config->number_of_responses > SOF_EQ_IIR_MAX_RESPONSES) {
		comp_err(dev, "# of resp %u exceeds max", config->number_of_responses);
		return -EINVAL;
	}

	ret = eq_iir_blob_words_max(dev, config, config_size, &coef_words_max);
	if (ret < 0)
		return ret;

	j = 0;
	coef_data = ASSUME_ALIGNED(&config->data[config->channels_in_config], 4);
	if (lookup)
		memset(lookup, 0, SOF_EQ_IIR_MAX_RESPONSES * sizeof(*lookup));

	for (i = 0; i < config->number_of_responses; i++) {
		/* Bounds-check response i, advance the walk cursor j past it,
		 * and get a pointer to its in-place header. Stored in lookup[]
		 * for later use, or discarded when we are walking the blob
		 * only to validate it.
		 */
		ret = eq_iir_init_response(dev, i, coef_data, coef_words_max, &j, &eq);
		if (ret < 0)
			return ret;
		if (lookup)
			lookup[i] = eq;
	}

	return 0;
}

int eq_iir_validate_config(struct comp_dev *dev, void *new_data, uint32_t new_data_size)
{
	struct sof_eq_iir_config *config = new_data;
	int32_t *assign_response;
	int32_t resp;
	int ret;
	int i;

	if (new_data_size < sizeof(struct sof_eq_iir_config) ||
	    new_data_size > SOF_EQ_IIR_MAX_SIZE) {
		comp_err(dev, "invalid configuration blob, size %u", new_data_size);
		return -EINVAL;
	}

	ret = eq_iir_walk_config(dev, config, new_data_size, NULL);
	if (ret < 0)
		return ret;

	/* Validate every assign_response[] entry that the per-channel loop in
	 * eq_iir_init_coef() could pick up. Entries beyond channels_in_config
	 * reuse the last assigned value, so checking [0, channels_in_config)
	 * covers all reachable nch.
	 */
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	for (i = 0; i < config->channels_in_config; i++) {
		resp = assign_response[i];
		if (resp >= 0 && resp >= config->number_of_responses) {
			comp_err(dev, "assign_response[%d] = %d exceeds %u",
				 i, resp, config->number_of_responses);
			return -EINVAL;
		}
	}

	return 0;
}

static int eq_iir_init_coef(struct processing_module *mod, unsigned int nch,
			    size_t *delay_size)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_eq_iir_config *config = cd->config;
	struct iir_state_df1 *iir = cd->iir;
	struct sof_eq_iir_header *lookup[SOF_EQ_IIR_MAX_RESPONSES];
	struct sof_eq_iir_header *eq;
	int32_t *assign_response;
	size_t size_sum = 0;
	int resp = 0;
	unsigned int i;
	int s;
	int ret;

	comp_info(mod->dev, "%u responses, %u channels, stream %u channels",
		  config->number_of_responses, config->channels_in_config, nch);

	if (nch > CONFIG_PLATFORM_MAX_CHANNELS) {
		comp_err(mod->dev, "invalid stream channels %u", nch);
		return -EINVAL;
	}

	ret = eq_iir_walk_config(mod->dev, config, cd->config_size, lookup);
	if (ret < 0)
		return ret;

	/* Initialize 1st phase */
	assign_response = ASSUME_ALIGNED(&config->data[0], 4);
	for (i = 0; i < nch; i++) {
		/* Check for not reading past blob response to channel assign
		 * map. The previous channel response is assigned for any
		 * additional channels in the stream. It allows to use single
		 * channel configuration to setup multi channel equalization
		 * with the same response.
		 */
		if (i < config->channels_in_config)
			resp = assign_response[i];

		if (resp < 0) {
			/* Initialize EQ channel to bypass and continue with
			 * next channel response.
			 */
			comp_info(mod->dev, "ch %u is set to bypass", i);
			iir_reset_df1(&iir[i]);
			continue;
		}

		if (resp >= config->number_of_responses) {
			comp_err(mod->dev, "requested response %d exceeds defined",
				 resp);
			return -EINVAL;
		}

		/* Initialize EQ coefficients */
		eq = lookup[resp];
		s = iir_delay_size_df1(eq);
		if (s > 0) {
			size_sum += (size_t)s;
		} else {
			comp_err(mod->dev, "sections count %d exceeds max",
				 eq->num_sections);
			return -EINVAL;
		}

		iir_init_coef_df1(&iir[i], eq);
		comp_info(mod->dev, "ch %u is set to response %d", i, resp);
	}

	*delay_size = size_sum;
	return 0;
}

static void eq_iir_init_delay(struct iir_state_df1 *iir,
			      int32_t *delay_start, unsigned int nch)
{
	int32_t *delay = delay_start;
	unsigned int i;

	/* Initialize second phase to set EQ delay lines pointers. A
	 * bypass mode filter is indicated by biquads count of zero.
	 */
	for (i = 0; i < nch; i++) {
		if (iir[i].biquads > 0)
			iir_init_delay_df1(&iir[i], &delay);
	}
}

void eq_iir_free_delaylines(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct iir_state_df1 *iir = cd->iir;
	unsigned int i;

	/* Free the common buffer for all EQs and point then
	 * each IIR channel delay line to NULL.
	 */
	mod_free(mod, cd->iir_delay);
	cd->iir_delay = NULL;
	cd->iir_delay_size = 0;
	for (i = 0; i < CONFIG_PLATFORM_MAX_CHANNELS; i++)
		iir[i].delay = NULL;
}

void eq_iir_pass(struct processing_module *mod, const struct cir_buf_source *source,
		 struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	const uint8_t *src = source->ptr;
	uint8_t *dst = sink->ptr;
	size_t bytes = frames * cd->frame_bytes;

	while (bytes) {
		size_t src_contiguous = (const uint8_t *)source->buf_end - src;
		size_t dst_contiguous = (uint8_t *)sink->buf_end - dst;
		size_t copy = MIN(bytes, MIN(src_contiguous, dst_contiguous));

		if (!copy) {
			src = source_cir_buf_wrap(src, source->buf_start, source->buf_end);
			dst = cir_buf_wrap(dst, sink->buf_start, sink->buf_end);
			continue;
		}

		assert(!memcpy_s(dst, dst_contiguous, src, copy));
		bytes -= copy;
		src = source_cir_buf_wrap(src + copy, source->buf_start, source->buf_end);
		dst = cir_buf_wrap(dst + copy, sink->buf_start, sink->buf_end);
	}
}

int eq_iir_setup(struct processing_module *mod, unsigned int nch)
{
	struct comp_data *cd = module_get_private_data(mod);
	size_t delay_size = 0;
	int ret;

	cd->channels = nch;

	/* Free existing IIR channels data if it was allocated */
	eq_iir_free_delaylines(mod);

	/* Set coefficients for each channel EQ from coefficient blob.
	 * eq_iir_init_coef() / eq_iir_blob_words_max() perform all blob size
	 * sanity checks, including config->size vs cd->config_size.
	 */
	ret = eq_iir_init_coef(mod, nch, &delay_size);
	if (ret < 0)
		return ret;

	/* If all channels were set to bypass there's no need to
	 * allocate delay. Just return with success.
	 */
	if (!delay_size)
		return 0;

	/* Allocate all IIR channels data in a big chunk and clear it */
	cd->iir_delay = mod_zalloc(mod, delay_size);
	if (!cd->iir_delay) {
		comp_err(mod->dev, "delay allocation fail");
		return -ENOMEM;
	}

	cd->iir_delay_size = delay_size;

	/* Assign delay line to each channel EQ */
	eq_iir_init_delay(cd->iir, cd->iir_delay, nch);
	return 0;
}

/*
 * End of EQ setup code. Next the standard component methods.
 */
static int eq_iir_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct comp_data *cd;
	unsigned int i;

	comp_info(dev, "entry");

	cd = mod_zalloc(mod, sizeof(*cd));
	if (!cd)
		return -ENOMEM;

	md->private = cd;

	/* component model data handler */
	cd->model_handler = mod_data_blob_handler_new(mod);
	if (!cd->model_handler) {
		comp_err(dev, "mod_data_blob_handler_new() failed.");
		mod_free(mod, cd);
		return -ENOMEM;
	}

	/* Reject malformed blobs at IPC time so a bad run-time update cannot
	 * replace the working configuration.
	 */
	comp_data_blob_set_validator(cd->model_handler, eq_iir_validate_config);

	for (i = 0; i < CONFIG_PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df1(&cd->iir[i]);

	return 0;
}

static int eq_iir_free(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);

	eq_iir_free_delaylines(mod);
	mod_data_blob_handler_free(mod, cd->model_handler);

	mod_free(mod, cd);
	return 0;
}


/* used to pass standard and bespoke commands (with data) to component */
static int eq_iir_set_config(struct processing_module *mod, uint32_t config_id,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size, fragment,
				  fragment_size);
}

static int eq_iir_get_config(struct processing_module *mod,
			     uint32_t config_id, uint32_t *data_offset_size,
			     uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct comp_data *cd = module_get_private_data(mod);

	comp_info(mod->dev, "entry");

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static int eq_iir_process(struct processing_module *mod,
			  struct sof_source **sources, int num_input_buffers,
			  struct sof_sink **sinks, int num_output_buffers)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct sof_source *source;
	struct sof_sink *sink;
	struct cir_buf_source source_buf;
	struct cir_buf_sink sink_buf;
	size_t source_frame_bytes;
	size_t sink_frame_bytes;
	size_t source_bytes;
	size_t sink_bytes;
	size_t source_buf_size;
	size_t sink_buf_size;
	size_t frame_count;
	int ret;

	if (num_input_buffers != 1 || num_output_buffers != 1) {
		comp_err(mod->dev, "EQ IIR supports one source and one sink");
		return -EINVAL;
	}

	source = sources[0];
	sink = sinks[0];
	cd->frame_bytes = source_get_frame_bytes(source);

	/* Check for changed configuration. The IPC-time validator installed
	 * in eq_iir_init() has already structurally validated the blob, so
	 * only NULL needs to be guarded here.
	 */
	if (comp_is_new_data_blob_available(cd->model_handler)) {
		cd->config = comp_get_data_blob(cd->model_handler, &cd->config_size, NULL);
		if (!cd->config)
			return -EINVAL;

		ret = eq_iir_new_blob(mod, source_get_frm_fmt(source),
				      sink_get_frm_fmt(sink),
				      source_get_channels(source));
		if (ret)
			return ret;
	}

	if (!cd->eq_iir_func)
		return -EINVAL;

	frame_count = source_sink_avail_frames_aligned(source, sink);
	if (!frame_count)
		return 0;

	source_frame_bytes = source_get_frame_bytes(source);
	sink_frame_bytes = sink_get_frame_bytes(sink);
	source_bytes = frame_count * source_frame_bytes;
	sink_bytes = frame_count * sink_frame_bytes;

	ret = source_get_data(source, source_bytes, &source_buf.ptr,
			      &source_buf.buf_start, &source_buf_size);
	if (ret < 0)
		return ret;
	source_buf.buf_end = (const char *)source_buf.buf_start + source_buf_size;

	ret = sink_get_buffer(sink, sink_bytes, &sink_buf.ptr,
			      &sink_buf.buf_start, &sink_buf_size);
	if (ret < 0) {
		source_release_data(source, 0);
		return ret;
	}
	sink_buf.buf_end = (char *)sink_buf.buf_start + sink_buf_size;

	cd->eq_iir_func(mod, &source_buf, &sink_buf, frame_count);

	ret = source_release_data(source, source_bytes);
	if (ret < 0) {
		sink_commit_buffer(sink, 0);
		return ret;
	}

	return sink_commit_buffer(sink, sink_bytes);
}

/**
 * \brief Set EQ IIR frames alignment limit.
 * \param[in,out] source Structure pointer of source.
 * \param[in,out] sink Structure pointer of sink.
 */
static int eq_iir_set_alignment(struct sof_source *source,
				struct sof_sink *sink)
{
	const uint32_t byte_align = SOF_FRAME_BYTE_ALIGN;
	const uint32_t frame_align_req = 2;
	int ret;

	ret = source_set_alignment_constants(source, byte_align, frame_align_req);
	if (ret < 0)
		return ret;

	return sink_set_alignment_constants(sink, byte_align, frame_align_req);
}

static int eq_iir_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct comp_dev *dev = mod->dev;
	struct sof_source *source;
	struct sof_sink *sink;
	enum sof_ipc_frame source_format;
	enum sof_ipc_frame sink_format;
	unsigned int channels;
	int ret = 0;

	comp_dbg(dev, "entry");

	/* EQ component will only ever have 1 source and 1 sink buffer. */
	if (num_of_sources != 1 || num_of_sinks != 1) {
		comp_err(dev, "EQ IIR supports one source and one sink");
		return -ENOTCONN;
	}

	source = sources[0];
	sink = sinks[0];

	ret = eq_iir_prepare_sub(mod);
	if (ret < 0)
		return ret;

	ret = eq_iir_set_alignment(source, sink);
	if (ret < 0)
		return ret;

	/* get source and sink data format */
	channels = source_get_channels(source);
	source_format = source_get_frm_fmt(source);
	sink_format = sink_get_frm_fmt(sink);
	if (channels != sink_get_channels(sink)) {
		comp_err(dev, "source and sink channel counts do not match");
		return -EINVAL;
	}
	cd->channels = channels;
	cd->frame_bytes = source_get_frame_bytes(source);

	cd->config = comp_get_data_blob(cd->model_handler, &cd->config_size, NULL);

	/* Initialize EQ */
	comp_info(dev, "source_format=%d, sink_format=%d",
		  source_format, sink_format);

	eq_iir_set_passthrough_func(cd, source_format, sink_format);

	/* Initialize EQ */
	if (cd->config) {
		ret = eq_iir_new_blob(mod, source_format, sink_format, channels);
		if (ret)
			return ret;
	}

	if (!cd->eq_iir_func) {
		comp_err(dev, "No processing function found");
		ret = -EINVAL;
	}

	return ret;
}

static int eq_iir_reset(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	int i;

	eq_iir_free_delaylines(mod);

	cd->eq_iir_func = NULL;
	cd->channels = 0;
	cd->frame_bytes = 0;
	for (i = 0; i < CONFIG_PLATFORM_MAX_CHANNELS; i++)
		iir_reset_df1(&cd->iir[i]);

	return 0;
}

static const struct module_interface eq_iir_interface = {
	.init = eq_iir_init,
	.prepare = eq_iir_prepare,
	.process = eq_iir_process,
	.set_configuration = eq_iir_set_config,
	.get_configuration = eq_iir_get_config,
	.reset = eq_iir_reset,
	.free = eq_iir_free
};

#if CONFIG_COMP_IIR_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("EQIIR", &eq_iir_interface, 1, SOF_REG_UUID(eq_iir), 40);

SOF_LLEXT_BUILDINFO;

#else

/* unused with Zephyr, generates no output */
DECLARE_TR_CTX(eq_iir_tr, SOF_UUID(eq_iir_uuid), LOG_LEVEL_INFO);
DECLARE_MODULE_ADAPTER(eq_iir_interface, eq_iir_uuid, eq_iir_tr);
SOF_MODULE_INIT(eq_iir, sys_comp_module_eq_iir_interface_init);

#if CONFIG_STATIC_PIPELINE
#include <sof/audio/pipeline/static_pipeline.h>

static int eq_iir_static_apply_switch(struct comp_dev *dev, uint32_t channels, int32_t val)
{
	struct processing_module *mod = comp_mod(dev);

	if (!mod) {
		comp_err(dev, "eq_iir_static_apply_switch: NULL processing module");
		return -EINVAL;
	}

	struct comp_data *cd = module_get_private_data(mod);

	if (!cd) {
		comp_err(dev, "eq_iir_static_apply_switch: NULL private data");
		return -EINVAL;
	}

	if (val == 0) {
		/* Disable EQ by engaging pass-through routine */
		cd->eq_iir_func = eq_iir_pass;
	} else if (cd->iir_delay_size) {
		/* Re-enable EQ processing matching configured valid bit depth */
		unsigned int valid_bit_depth = mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth;

		switch (valid_bit_depth) {
#if CONFIG_FORMAT_S16LE
		case IPC4_DEPTH_16BIT:
			cd->eq_iir_func = eq_iir_s16_default;
			break;
#endif
#if CONFIG_FORMAT_S24LE
		case IPC4_DEPTH_24BIT:
			cd->eq_iir_func = eq_iir_s24_default;
			break;
#endif
#if CONFIG_FORMAT_S32LE
		case IPC4_DEPTH_32BIT:
			cd->eq_iir_func = eq_iir_s32_default;
			break;
#endif
		default:
			/* Fallback inspecting producer stream frame format */
			{
				struct comp_buffer *sourceb = comp_dev_get_first_data_producer(dev);
				enum sof_ipc_frame fmt = sourceb ?
					audio_stream_get_frm_fmt(&sourceb->stream) :
					SOF_IPC_FRAME_S32_LE;

				switch (fmt) {
#if CONFIG_FORMAT_S16LE
				case SOF_IPC_FRAME_S16_LE:
					cd->eq_iir_func = eq_iir_s16_default;
					break;
#endif
#if CONFIG_FORMAT_S24LE
				case SOF_IPC_FRAME_S24_4LE:
				case SOF_IPC_FRAME_S24_3LE:
					cd->eq_iir_func = eq_iir_s24_default;
					break;
#endif
				default:
#if CONFIG_FORMAT_S32LE
					cd->eq_iir_func = eq_iir_s32_default;
#elif CONFIG_FORMAT_S16LE
					cd->eq_iir_func = eq_iir_s16_default;
#else
					cd->eq_iir_func = eq_iir_pass;
#endif
					break;
				}
			}
			break;
		}
	}
	return 0;
}

static struct sof_static_module_ops eq_iir_static_ops = {
	.uuid = &eq_iir_uuid,
	.apply_switch = eq_iir_static_apply_switch,
};

DECLARE_STATIC_MODULE_OPS(eq_iir, &eq_iir_static_ops);
#endif /* CONFIG_STATIC_PIPELINE */

#endif
