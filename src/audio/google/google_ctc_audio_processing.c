#include <google_ctc_audio_processing.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>

struct comp_driver comp_ctc_audio_processing;

/* bf0e1bbc-dc6a-45fe-bc90-2554cb137ab4 */
DECLARE_SOF_RT_UUID("ctc_audio_processing", ctc_audio_processing_uuid,
                    0xbf0e1bbc, 0xdc6a, 0x45fe, 0xbc, 0x90, 0x25, 0x54, 0xcb,
                    0x13, 0x7a, 0xb4);

DECLARE_TR_CTX(ctc_audio_processing_tr, SOF_UUID(ctc_audio_processing_uuid),
               LOG_LEVEL_INFO);

struct ctc_audio_processing_comp_data {
  struct comp_data_blob_handler *model_handler;
  struct comp_buffer *input;
  struct comp_buffer *output;
  uint32_t num_frames;
  GoogleCtcAudioProcessingState *state;
  struct comp_data_blob_handler *tuning_handler;
  bool reconfigure;
};

struct comp_driver comp_ctc_audio_processing = {
    .uid = SOF_RT_UUID(ctc_audio_processing_uuid),
    .tctx = &ctc_audio_processing_tr,
    .ops =
        {
            .create = ctc_audio_processing_create,
            .free = ctc_audio_processing_free,
            .params = NULL,
            .cmd = ctc_audio_processing_cmd,
            .trigger = ctc_audio_processing_trigger,
            .prepare = ctc_audio_processing_prepare,
            .reset = ctc_audio_processing_reset,
            .copy = ctc_audio_processing_copy,
        },
};

static SHARED_DATA struct comp_driver_info comp_ctc_audio_processing_info = {
    .drv = &comp_ctc_audio_processing,
};

static void sys_comp_ctc_audio_processing_init(void) {
  comp_register(platform_shared_get(&comp_ctc_audio_processing_info,
                                    sizeof(comp_ctc_audio_processing_info)));
}

static struct comp_dev *ctc_audio_processing_create(
    const struct comp_driver *drv, struct comp_ipc_config *config, void *spec) {
  struct comp_dev *dev;
  struct ctc_audio_processing_comp_data *cd;
  struct ipc_config_process *ipc_ctc = spec;

  comp_cl_info(&comp_ctc_audio_processing, "ctc_audio_processing_create()");

  dev = comp_alloc(drv, sizeof(*dev));
  if (!dev) return NULL;
  dev->ipc_config = *config;

  cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
  if (!cd) {
    rfree(dev);
    return NULL;
  }

  cd->num_frames = 64;  // TODO(eddyhsu): figure out correct num_frames

  cd->input =
      rballoc(0, SOF_MEM_CAPS_RAM, cd->num_frames * sizeof(cd->input[0]));
  if (!cd->input) goto fail;
  bzero(cd->input, cd->num_frames * sizeof(cd->input[0]));

  cd->output =
      rballoc(0, SOF_MEM_CAPS_RAM, cd->num_frames * sizeof(cd->output[0]));
  if (!cd->output) goto fail;
  bzero(cd->output, cd->num_frames * sizeof(cd->output[0]));

  cd->state = GoogleCtcAudioProcessingCreate();

  comp_set_drvdata(dev, cd);

  dev->state = COMP_STATE_READY;

  comp_dbg(dev, "crosstalk cancellation created");

  return dev;
fail:
  comp_err(dev, "google_ctc_audio_processing_create(): Failed");
  if (cd) {
    rfree(cd->input);
    rfree(cd->output);
    if (cd->state) {
      GoogleCtcAudioProcessingFree(cd->state);
    }
    rfree(cd);
  }
  rfree(dev);
  return NULL;
}

static void ctc_audio_processing_free(struct comp_dev *dev) {
  struct comp_data *cd = comp_get_drvdata(dev);

  GoogleCtcAudioProcessingFree(cd->state);
  cd->state = NULL;
  rfree(cd->input);
  rfree(cd->output);
  rfree(cd);
  rfree(dev);
}

static int ctc_audio_processing_trigger(struct comp_dev *dev, int cmd) {
  comp_dbg(dev, "crosstalk cancellation got trigger cmd %d", cmd);
  return comp_set_state(dev, cmd);
}

static int ctc_audio_processing_prepare(struct comp_dev *dev) {
  int ret;

  comp_info(dev, "google_ctc_audio_processing_prepare()");

  ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
  if (ret < 0) return ret;

  if (ret == COMP_STATUS_STATE_ALREADY_SET) return PPL_STATUS_PATH_STOP;

  comp_dbg(dev, "crosstalk cancellation prepared");
  return 0;
}

static int ctc_audio_processing_reset(struct comp_dev *dev) {
  return comp_set_state(dev, COMP_TRIGGER_RESET);
}

static int ctc_audio_processing_copy(struct comp_dev *dev) {
  struct comp_copy_limits cl;
  struct comp_buffer *source;
  struct comp_buffer *sink;
  struct ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);
  int frame;
  int channel;
  uint32_t buff_frag = 0;
  int16_t *src;
  int16_t *dst;

  comp_dbg(dev, "ctc_audio_processing_copy()");

  source = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
  sink = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);

  comp_get_copy_limits_with_lock(source, sink, &cl);

  buffer_invalidate(source, cl.source_bytes);

  for (frame = 0; frame < cl.frames; frame++) {
    for (channel = 0; channel < sink->stream.channels; channel++) {
      src = audio_stream_read_frag_s16(&source->stream, buff_frag);
      dst = audio_stream_write_frag_s16(&sink->stream, buff_frag);

      GoogleCtcAudioProcessingProcess(cd->state, src, dst);

      ++buff_frag;
    }
  }

  buffer_writeback(sink, cl.sink_bytes);

  comp_update_buffer_produce(sink, cl.sink_bytes);
  comp_update_buffer_consume(source, cl.source_bytes);

  return 0;
}

static int ctc_audio_processing_cmd_set_data(struct comp_dev *dev,
                                             struct sof_ipc_ctrl_data *cdata) {
  struct ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);

  if (cdata->cmd != SOF_CTRL_CMD_BINARY) {
    comp_err(dev, "ctc_audio_processing_cmd_set_data(): invalid cmd %d",
             cdata->cmd);
    return -EINVAL;
  }

  ret = comp_data_blob_set_cmd(cd->tuning_handler, cdata);
  if (ret) return ret;

  /* Accept the new blob immediately so that userspace can write
   * the control in quick succession without error.
   * This ensures the last successful control write from userspace
   * before prepare/copy is applied.
   * The config blob is not referenced after reconfigure() returns
   * so it is safe to call comp_get_data_blob here which frees the
   * old blob. This assumes cmd() and prepare()/copy() cannot run
   * concurrently which is the case when there is no preemption.
   */
  if (comp_is_new_data_blob_available(cd->tuning_handler)) {
    comp_get_data_blob(cd->tuning_handler, NULL, NULL);
    cd->reconfigure = true;
  }

  comp_dbg(dev, "crosstalk cancellation new settings");
  return 0;
}

static int ctc_audio_processing_cmd_get_data(struct comp_dev *dev,
                                             struct sof_ipc_ctrl_data *cdata,
                                             int max_data_size) {
  struct ctc_audio_processing_comp_data *cd = comp_get_drvdata(dev);

  if (cdata->cmd != SOF_CTRL_CMD_BINARY) {
    comp_err(dev, "ctc_audio_processing_cmd_get_data(): invalid cmd %d",
             cdata->cmd);
    return -EINVAL;
  }

  return comp_data_blob_get_cmd(cd->tuning_handler, cdata, max_data_size);
}

static int ctc_audio_processing_cmd(struct comp_dev *dev, int cmd, void *data,
                                    int max_data_size) {
  struct sof_ipc_ctrl_data *cdata = data;
  int ret = 0;

  switch (cmd) {
    case COMP_CMD_SET_VALUE:
    case COMP_CMD_GET_VALUE:
      return 0;
    case COMP_CMD_SET_DATA:
      ret = ctc_audio_processing_cmd_set_data(dev, cdata);
      break;
    case COMP_CMD_GET_DATA:
      ret = ctc_audio_processing_cmd_get_data(dev, cdata, max_data_size);
      break;
    default:
      comp_err(dev, "ctc_audio_processing_cmd(): unhandled command %d", cmd);
      ret = -EINVAL;
      break;
  }
  return ret;
}

static SHARED_DATA struct comp_driver_info google_ctc_audio_processing_info = {
    .drv = &google_ctc_audio_processing,
};

UT_STATIC void sys_comp_google_ctc_audio_processing_init(void) {
  comp_register(platform_shared_get(&google_ctc_audio_processing_info,
                                    sizeof(google_ctc_audio_processing_info)));
}

DECLARE_MODULE(sys_comp_google_ctc_audio_processing_init);
SOF_MODULE_INIT(google_ctc_audio_processing,
                sys_comp_google_ctc_audio_processing_init);
