#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
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
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define trace_amp(__e, ...) trace_event(TRACE_CLASS_AMP, __e, ##__VA_ARGS__)
#define tracev_amp(__e, ...) tracev_event(TRACE_CLASS_AMP, __e, ##__VA_ARGS__)
#define trace_amp_error(__e, ...) \
        trace_error(TRACE_CLASS_AMP, __e, ##__VA_ARGS__)


struct comp_data {
        int placeholder;
};

static struct comp_dev *amp_new(struct sof_ipc_comp *comp)
{
        struct comp_dev *dev;
        struct sof_ipc_comp_process *amp;
        struct sof_ipc_comp_process *ipc_amp
                = (struct sof_ipc_comp_process *)comp;
        struct comp_data *cd;

        dev = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
                      COMP_SIZE(struct sof_ipc_comp_process));
        if (!dev)
                return NULL;

        cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
        if (!cd) {
                rfree(dev);
                return NULL;
        }

        amp = (struct sof_ipc_comp_process *)&dev->comp;
        assert(!memcpy_s(amp, sizeof(*amp), ipc_amp,
                         sizeof(struct sof_ipc_comp_process)));

        comp_set_drvdata(dev, cd);

        dev->state = COMP_STATE_READY;

        trace_amp("Amplifier created");

        return dev;
}

static void amp_free(struct comp_dev *dev)
{
        struct comp_data *cd = comp_get_drvdata(dev);

        rfree(cd);
        rfree(dev);
}

static int amp_trigger(struct comp_dev *dev, int cmd)
{
        trace_amp("Amplifier got trigger cmd %d", cmd);
        return comp_set_state(dev, cmd);
}

static int amp_prepare(struct comp_dev *dev)
{
	struct sof_ipc_comp_config *config = COMP_GET_CONFIG(dev);
  enum sof_ipc_frame src_fmt, sink_fmt;
	struct comp_buffer *sourceb;
	struct comp_buffer *sinkb;
	uint32_t sink_period_bytes;
  uint32_t source_period_bytes;
	int ret;

	trace_amp("amp_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret < 0)
		return ret;

	if (ret == COMP_STATUS_STATE_ALREADY_SET)
		return PPL_STATUS_PATH_STOP;

	/* AMP component will only ever have 1 source and 1 sink buffer */
	sourceb = list_first_item(&dev->bsource_list,
				  struct comp_buffer, sink_list);
	sinkb = list_first_item(&dev->bsink_list,
				struct comp_buffer, source_list);

  src_fmt = sourceb->stream.frame_fmt;
  sink_fmt = sinkb->stream.frame_fmt;

	sink_period_bytes = audio_stream_period_bytes(&sinkb->stream,
						      dev->frames);
  source_period_bytes = audio_stream_period_bytes(&sourceb->stream, dev->frames);

  /* Rewrite params format for this component to match the host side. */
	if (dev->direction == SOF_IPC_STREAM_PLAYBACK)
		sourceb->stream.frame_fmt = src_fmt;
	else
		sinkb->stream.frame_fmt = sink_fmt;

    ret = buffer_set_size(sinkb,
      sink_period_bytes * config->periods_sink);

  if (ret < 0) {
    trace_amp_error("amp_prepare() error: "
        "buffer_set_size() failed %d", ret);
    goto err;
  }

  trace_amp("Amplifier prepared src_fmt %d src_per_bytes: %u "
    "sink_fmt %d sink_per_bytes: %u",
    src_fmt, source_period_bytes,
    sink_fmt, sink_period_bytes);

  return 0;
  err:
  return ret;
}

static int amp_reset(struct comp_dev *dev)
{
        return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int amp_copy(struct comp_dev *dev)
{
        struct comp_copy_limits cl;
        int ret;
        int frame;
        int channel;
        uint32_t buff_frag = 0;
        int16_t *src;
        int16_t *dst;

        ret = comp_get_copy_limits(dev, &cl);
        if (ret < 0) {
                return ret;
        }

        for (frame = 0; frame < cl.frames; frame++) {
                for (channel = 0; channel < cl.source->stream.channels; channel++) {
                        src = audio_stream_read_frag_s16((&cl.source->stream), buff_frag);
                        dst = audio_stream_write_frag_s16((&cl.sink->stream), buff_frag);
                        *dst = *src;
                        ++buff_frag;
                }
        }

        comp_update_buffer_produce(cl.sink, cl.sink_bytes);
        comp_update_buffer_consume(cl.source, cl.source_bytes);

        return 0;
}

struct comp_driver comp_amp = {
  .type = SOF_COMP_AMP,
  .ops = {
    .new = amp_new,
    .free = amp_free,
    .params = NULL,
    .cmd = NULL,
    .trigger = amp_trigger,
    .prepare = amp_prepare,
    .reset = amp_reset,
    .copy = amp_copy
  },
};

static SHARED_DATA struct comp_driver_info comp_amp_info = {
	.drv = &comp_amp,
};

static void sys_comp_amp_init(void){
  comp_register(&comp_amp_info);
}

DECLARE_MODULE(sys_comp_amp_init);
