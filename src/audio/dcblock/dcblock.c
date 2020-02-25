#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/dcblock/dcblock.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
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

/* DC Blocking Filter component private data */
struct comp_data {
	 int32_t R_coeff;
   int32_t x_prev;                    /**< state variable referring to x[n-1] */
   int32_t y_prev;                    /**< state variable referring to y[n-1] */
   enum sof_ipc_frame source_format;	/**< source frame format */
 	 enum sof_ipc_frame sink_format;		/**< sink frame format */
   dcblock_func dcblock_func;         /**< processing function */
};

/**
 *
 * Genereric processing function. Input is 32 bits.
 *
 */
static int32_t dcblock_generic(struct comp_data *cd, int32_t x) {
  int32_t x_prev = cd->x_prev;
  int32_t y_prev = cd->y_prev;
  int32_t R = cd->R_coeff;

  int64_t out = ((int64_t)x) + Q_SHIFT_RND(((int64_t) R) * y_prev, 62, 31) - (int64_t) x_prev;

	y_prev = sat_int32(out);
  x_prev = x;

  cd->x_prev = x_prev;
	cd->y_prev = y_prev;

	return sat_int32(out);
}

// static int cnt = 0;
// if(out < 0 && cnt < 15) {
// 	trace_dcblock("x: %d.%d, R: %d.%d",
// 	FLOAT_MACRO(x), FLOAT_MACRO(R));
// 	trace_dcblock("y_prev: %d.%d, x_prev: %d.%d",
// 	FLOAT_MACRO(y_prev), FLOAT_MACRO(x_prev));
// 	trace_dcblock("out: %d.%d",
// 	FLOAT_MACRO(out));
// 	cnt++;
// }

static void dcblock_s16le_default(const struct comp_dev *dev,
									const struct audio_stream *source,
									const struct audio_stream *sink,
									uint32_t frames)
{
	struct comp_data *cd = comp_get_drvdata(dev);
	int16_t *x;
	int16_t *y;
	int32_t tmp;
	int idx;
	int ch;
	int i;
	int nch = source->channels;

	for(ch = 0; ch < nch; ch++) {
		idx = ch;
		for(i = 0; i < frames; i++) {
			x = audio_stream_read_frag_s16(source, idx);
			y = audio_stream_read_frag_s16(sink, idx);
			tmp = dcblock_generic(cd, *x << 16);
			*y = sat_int16(Q_SHIFT_RND(tmp, 31, 15));
			idx += nch;
		}
	}
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
  int ret;

  trace_dcblock("dcblock_new()");

  if (IPC_IS_SIZE_INVALID(ipc_dcblock->config)) {
    IPC_SIZE_ERROR_TRACE(TRACE_CLASS_DCBLOCK, ipc_dcblock->config);
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
  cd->R_coeff = 0;
  cd->x_prev = 0;
  cd->y_prev = 0;
  cd->source_format = (int) NULL;
  cd->sink_format = (int) NULL;

  trace_dcblock_with_ids(dev,
            "dcblock->R_coeff = %i, dcblock->x_prev = %i, "
            "dcblock->y_prev = %i",
            cd->R_coeff, cd->x_prev, cd->y_prev);

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
  // struct comp_buffer *sourceb;
	int ret;

  tracev_dcblock_with_ids(dev, "dcblock_copy()");

  // sourceb = list_first_item(&dev->bsource_list, struct comp_buffer,
  //       sink_list);

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
  if(ret < 0)
    return ret;

  if(ret == COMP_STATUS_STATE_ALREADY_SET)
    return PPL_STATUS_PATH_STOP;

  /* DC Filter component will only ever have one source and sink buffer */
  sourceb = list_first_item(&dev->bsource_list,
          struct comp_buffer, sink_list);
  sinkb = list_first_item(&dev->bsink_list,
        struct comp_buffer, source_list);

	/* Rewrite params format for this component to match the host side. */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sourceb->stream.frame_fmt = cd->source_format;
	else
		sinkb->stream.frame_fmt = cd->sink_format;


  /* get source data format */
  cd->source_format = sourceb->stream.frame_fmt;

  /* get sink data format and period bytes */
  cd->sink_format = sinkb->stream.frame_fmt;
  sink_period_bytes = audio_stream_period_bytes(&sinkb->stream, dev->frames);

  if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
		trace_dcblock_error_with_ids(dev,
						"dcblock_prepare(), "
						"sink buffer size %d is insufficient",
						sinkb->stream.size);
		ret = -ENOMEM;
		goto err;
	}

  cd->R_coeff = Q_CONVERT_FLOAT(0.98, 31);
	cd->dcblock_func = dcblock_s16le_default;
	int16_t *x = audio_stream_read_frag_s16((&sourceb->stream), 0);
	cd->x_prev = *x << 16;
	cd->y_prev = 0;

  trace_dcblock_with_ids(dev, "dcblock_prepare(), source_format=%d, sink_format=%d, R_coeff=%f",
        cd->source_format, cd->sink_format, Q_CONVERT_QTOF(cd->R_coeff, 31));

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
		.cmd		= NULL,
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
