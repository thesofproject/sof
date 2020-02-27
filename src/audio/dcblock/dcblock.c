#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/dcblock/dcblock.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/drivers/ipc.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

/* tracing */
#define trace_dcblock(__e, ...) trace_event(TRACE_CLASS_DCBLOCK, __e, ##__VA_ARGS__)
#define trace_dcblock_with_ids(comp_ptr, __e, ...)			\
	trace_event_comp(TRACE_CLASS_DCBLOCK, comp_ptr,		\
			 __e, ##__VA_ARGS__)

#define tracev_dcblock(__e, ...) tracev_event(TRACE_CLASS_DCBLOCK, __e, ##__VA_ARGS__)
#define tracev_dcblock_with_ids(comp_ptr, __e, ...)			\
	tracev_event_comp(TRACE_CLASS_DCBLOCK, comp_ptr,		\
			  __e, ##__VA_ARGS__)

#define trace_dcblock_error(__e, ...)				\
	trace_error(TRACE_CLASS_DCBLOCK, __e, ##__VA_ARGS__)
#define trace_dcblock_error_with_ids(comp_ptr, __e, ...)		\
	trace_error_comp(TRACE_CLASS_DCBLOCK, comp_ptr,		\
			 __e, ##__VA_ARGS__)

#define FLOAT_MACRO(_x) Q_CONVERT_QTOF(_x,31), Q_CONVERT_QTOF(_x&0x7FFFFFFF, 31) * 1000


static void dcblock_free_config(struct sof_dcblock_config **config)
{
	rfree(*config);
	*config = NULL;
}

static int dcblock_set_coeffs(struct comp_data *cd)
{
	int i;

	/* Verify that the configuration values are set */
	if (!cd->config) {
		return -EINVAL;
	}

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		cd->dcblock[i].R_coeff = cd->config->R_coeffs[i];
	}

	return 0;
}

/*
 * Sets the DC Blocking filter in pass through mode.
 * The frequency response of a DCB filter is:
 * H(z) = (1 - z^-1)/(1-Rz^-1).
 * Setting R to 1 makes the filter acts as a passthrough component.
 */
static int dcblock_set_passthrough(struct comp_data *cd)
{
	trace_dcblock("dcblock_set_passthrough()");

	int i;
	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		cd->dcblock[i].R_coeff = Q_CONVERT_FLOAT(0.98,31);
	}

	return 0;
}

static int dcblock_set_state(struct comp_data *cd,
						struct audio_stream *src_stream)
{
	int32_t *x0;
	int i;

	for (i = 0; i < PLATFORM_MAX_CHANNELS; i++) {
		x0 = audio_stream_read_frag_s32(src_stream, 0);
		cd->dcblock[i].y_prev = 0;

		switch(src_stream->frame_fmt) {
		case SOF_IPC_FRAME_S16_LE:
			cd->dcblock[i].x_prev = *x0 << 16;
			break;

		case SOF_IPC_FRAME_S24_4LE:
			cd->dcblock[i].x_prev = *x0 << 8;
			break;

		case SOF_IPC_FRAME_S32_LE:
			cd->dcblock[i].x_prev = *x0;
			break;

		default:
			trace_dcblock_error("dcblock_set_state(), "
								"Invalid frame format: %i", src_stream->frame_fmt);
			return -EINVAL;
		}
	}

	return 0;
}


/**
 * \brief Creates DC Blocking Filter component.
 * \return Pointer to DC Blocking Filter component device.
 */
static struct comp_dev *dcblock_new(struct sof_ipc_comp *comp)
{
  struct comp_dev *dev;
  struct comp_data *cd;
  struct sof_ipc_comp_process *dcblock;
  struct sof_ipc_comp_process *ipc_dcblock =
    (struct sof_ipc_comp_process *) comp;
	size_t bs = ipc_dcblock->size;
  int ret;

  trace_dcblock("dcblock_new()");

  if (IPC_IS_SIZE_INVALID(ipc_dcblock->config)) {
    IPC_SIZE_ERROR_TRACE(TRACE_CLASS_DCBLOCK, ipc_dcblock->config);
    return NULL;
  }

	/* The blob size is fixed. Verify that it matches the expected
	 * value before proceeding
	 */
	 if(bs > sizeof(struct sof_dcblock_config)) {
		 trace_dcblock_error("dcblock_new(), size of config binary data %i "
	 								"does not match the expected value %i",
									bs, sizeof(struct sof_dcblock_config));
		 return NULL;
	 }

  dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
          COMP_SIZE(struct sof_ipc_comp_process));
  if(!dev)
    return NULL;

  dcblock = (struct sof_ipc_comp_process *)&dev->comp;
  ret = memcpy_s(dcblock, sizeof(*dcblock), ipc_dcblock,
            sizeof(struct sof_ipc_comp_process));
  assert(!ret);

  cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
  if (!cd) {
    rfree(dev);
    return NULL;
  }

  comp_set_drvdata(dev, cd);

  cd->dcblock_func = NULL;
	cd->config = NULL;
	cd->config_new = NULL;
  cd->source_format = (int) NULL;
  cd->sink_format = (int) NULL;

	/* Allocate and make a copy of the configu blob. */
	if(bs) {
		cd->config = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, bs);
		if(!cd->config) {
			rfree(dev);
			rfree(cd);
			return NULL;
		}

		ret = memcpy_s(cd->config, bs, ipc_dcblock->data, bs);
		assert(!ret);
	}

  dev->state = COMP_STATE_READY;
  return dev;
}

/**
 * \brief Frees DC Blocking Filter component.
 * \param[in,out] dev DC Blocking Filter base component device.
 */
static void dcblock_free(struct comp_dev *dev)
{
	struct comp_data *cd = comp_get_drvdata(dev);

	trace_dcblock_with_ids(dev, "dcblock_free()");
	dcblock_free_config(&cd->config);
	dcblock_free_config(&cd->config_new);
	rfree(cd);
	rfree(dev);
}

/**
 * \brief Sets DC Blocking Filter component audio stream parameters.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 *
 * All done in prepare() since we need to know source and sink component params.
 */
static int dcblock_params(struct comp_dev *dev,
			struct sof_ipc_stream_params *params)
{
	trace_dcblock_with_ids(dev, "dcblock_params()");

	return 0;
}

static int dcblock_cmd_get_data(struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata, size_t max_size)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	size_t resp_size;
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_dcblock_with_ids(dev, "dcblock_cmd_get_data(), SOF_CTRL_CMD_BINARY");

		if (!cd->config) {
			trace_dcblock_error_with_ids(dev, "dcblock_cmd_get_data(), no config");
			ret = -EINVAL;
			break;
		}

		/* Copy config back to user space */
		resp_size = sizeof(struct sof_dcblock_config);
		trace_dcblock_with_ids(dev, "dcblock_cmd_get_data(), resp_size %u",
					resp_size);

		if (resp_size > max_size) {
			trace_dcblock_error_with_ids(dev, "response size %i "
							"exceeds maximum size %i ", resp_size, max_size);
			ret = -EINVAL;
			break;
		}

		ret = memcpy_s(cdata->data->data, cdata->data->size,
									 cd->config, resp_size);
		assert(!ret);

		cdata->data->abi = SOF_ABI_VERSION;
		cdata->data->size = resp_size;
		break;
	default:
		trace_dcblock_error_with_ids(dev,
						"dcblock_cmd_get_data(), invalid command");
		ret = -EINVAL;
	}

	return ret;
}

static int dcblock_cmd_set_data(struct comp_dev *dev,
			struct sof_ipc_ctrl_data *cdata)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	struct sof_dcblock_config *request;
	size_t req_size = sizeof(struct sof_dcblock_config);
	int ret = 0;

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		trace_dcblock_with_ids(dev, "dcblock_cmd_set_data(), SOF_CTRL_CMD_BINARY");

		/* Retrieve the binary controls from the packet */
		request = (struct sof_dcblock_config *)cdata->data->data;

		/* Check that there is no work-in-progress previous request */
		if (!cd->config_new) {
			trace_dcblock_error_with_ids(dev, "dcblock_cmd_set_data(), "
								"busy processing previous request");
			ret = -EBUSY;
			break;
		}

		/* Allocate memory and copy the new configuration */
		cd->config_new = rzalloc(SOF_MEM_ZONE_RUNTIME, 0,
						SOF_MEM_CAPS_RAM, req_size);
		if (!cd->config_new) {
			trace_dcblock_error_with_ids(dev, "dcblock_cmd_set_data(), "
								"could not allocate memory for new configuration");
			ret = -ENOMEM;
			break;
		}

		ret = memcpy_s(cd->config_new, req_size, request, req_size);
		assert(!ret);

		/* If component state is READY we can omit old configuration
		 * immediately. When in playback/capture the new configuration
		 * presence is checked in copy().
		 */
		 if (dev->state == COMP_STATE_READY)
		 	 dcblock_free_config(&cd->config);

		 /* If there is no existing configuration the received can
 		 * be set to current immediately. It will be applied in
 		 * prepare() when streaming starts.
 		 */
		 if (!cd->config) {
			 cd->config = cd->config_new;
			 cd->config_new = NULL;
		 }
		 break;
	default:
		trace_dcblock_with_ids(dev, "dcblock_set_data(), "
							"invalid command %i", cdata->cmd);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * \brief Handles incoming IPC commands for DC Blocking Filter component.
 */
static int dcblock_cmd(struct comp_dev *dev, int cmd, void *data,
									int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	int ret = 0;

	trace_dcblock_with_ids(dev, "dcblock_cmd()");

	switch(cmd) {
	case COMP_CMD_SET_DATA:
		ret = dcblock_cmd_set_data(dev, cdata);
		break;
	case COMP_CMD_GET_DATA:
		ret = dcblock_cmd_get_data(dev, cdata, max_data_size);
		break;
	default:
		trace_dcblock("dcblock_cmd(), invalid command (%i)", cmd);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * \brief Sets DC Blocking Filter component state.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \param[in] cmd Command type.
 * \return Error code.
 */
static int dcblock_trigger(struct comp_dev *dev, int cmd)
{
	trace_dcblock_with_ids(dev, "dcblock_trigger()");

	return comp_set_state(dev, cmd);
}

/**
 * \brief Copies and processes stream data.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 */
static int dcblock_copy(struct comp_dev *dev)
{
	struct comp_copy_limits cl;
	struct comp_data *cd = comp_get_drvdata(dev);
	int ret;

  tracev_dcblock_with_ids(dev, "dcblock_copy()");

	/* Check for changed configration */
	if (cd->config_new) {
		tracev_dcblock_with_ids(dev, "dcblock_copy(), updating new config");
		dcblock_free_config(&cd->config);
		cd->config = cd->config_new;
		cd->config_new = NULL;

		ret = dcblock_set_coeffs(cd);
		if (ret < 0) {
			trace_dcblock_with_ids(dev, "dcblock_copy(), failed DC Block setup");
			return ret;
		}
	}

  ret = comp_get_copy_limits(dev, &cl);
  if (ret < 0) {
    trace_dcblock_error_with_ids(dev,
      "dcblock_copy(), failed comp_get_copy_limits()");
      return ret;
  }

  /* Run DC Filtering function */
  cd->dcblock_func(dev, &cl.source->stream, &cl.sink->stream, cl.frames);

  /* calc new free and available */
	comp_update_buffer_consume(cl.source, cl.source_bytes);
	comp_update_buffer_produce(cl.sink, cl.sink_bytes);

	return 0;
}

/**
 * \brief Prepares DC Blocking Filter component for processing.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 */
static int dcblock_prepare(struct comp_dev *dev)
{
  struct comp_data *cd = comp_get_drvdata(dev);
  struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
  struct comp_buffer *sourceb;
  struct comp_buffer *sinkb;
  uint32_t sink_period_bytes;
  int ret;

  trace_dcblock_with_ids(dev, "dcblock_prepare()");

  ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
  if (ret < 0)
    return ret;

  if (ret == COMP_STATUS_STATE_ALREADY_SET)
    return PPL_STATUS_PATH_STOP;

  /* DC Filter component will only ever have one source and sink buffer */
  sourceb = list_first_item(&dev->bsource_list,
          struct comp_buffer, sink_list);
  sinkb = list_first_item(&dev->bsink_list,
        struct comp_buffer, source_list);

  /* get source data format */
  cd->source_format = sourceb->stream.frame_fmt;

  /* get sink data format and period bytes */
  cd->sink_format = sinkb->stream.frame_fmt;
  sink_period_bytes = audio_stream_period_bytes(&sinkb->stream, dev->frames);

	/* Rewrite params format for this component to match the host side. */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sourceb->stream.frame_fmt = cd->source_format;
	else
		sinkb->stream.frame_fmt = cd->sink_format;

  if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		trace_dcblock_error_with_ids(dev,
						"dcblock_prepare(), "
						"sink buffer size %d is insufficient",
						sinkb->stream.size);
		ret = -ENOMEM;
		goto err;
	}

	/* Initialize the coefficients and state variables for the DC filter.
	 * If the config has not been set then the filter is set to
	 * passthrough mode.
	 */
	ret = cd->config ? dcblock_set_coeffs(cd) : dcblock_set_passthrough(cd);
	if (ret < 0) {
		trace_dcblock_error_with_ids(dev, "dcblock_prepare(), "
							"failed to set coefficients");
		goto err;
	}

	ret = dcblock_set_state(cd, &sourceb->stream);
	if (ret < 0) {
		trace_dcblock_error_with_ids(dev, "dcblock_prepare(), "
							"failed to set state");
		goto err;
	}

	cd->dcblock_func = dcblock_find_func(cd->source_format, cd->sink_format);
	if(!cd->dcblock_func) {
		trace_dcblock_error_with_ids(dev, "dcblock_prepare(), "
							"No processing function matching source and sink format");
		ret = -EINVAL;
		goto err;
	}

  trace_dcblock_with_ids(dev, "dcblock_prepare(), "
				"source_format=%d, sink_format=%d",
        cd->source_format, cd->sink_format);

  return 0;

err:
	comp_set_state(dev, COMP_TRIGGER_RESET);
	return ret;
}

/**
 * \brief Resets DC Blocking Filter component.
 * \param[in,out] dev DC Blocking Filter base component device.
 * \return Error code.
 */
static int dcblock_reset(struct comp_dev *dev)
{
	trace_dcblock_with_ids(dev, "dcblock_reset()");

	comp_set_state(dev, COMP_TRIGGER_RESET);
	return 0;
}


/** \brief DC Blocking Filter component definition. */
static const struct comp_driver comp_dcblock = {
	.type	= SOF_COMP_DCBLOCK,
	.ops	= {
		.new		= dcblock_new,
		.free		= dcblock_free,
		.params		= dcblock_params,
		.cmd		= dcblock_cmd,
		.trigger	= dcblock_trigger,
		.copy		= dcblock_copy,
		.prepare	= dcblock_prepare,
		.reset		= dcblock_reset,
	},
};

static SHARED_DATA struct comp_driver_info comp_dcblock_info = {
	.drv = &comp_dcblock,
};

static void sys_comp_dcblock_init(void){
  comp_register(&comp_dcblock_info);
}

DECLARE_MODULE(sys_comp_dcblock_init);
