// Copyright(c) 2021 Sound Research Corporation. All rights reserved.
#include <sof/audio/component.h>
#include <sof/audio/buffer.h>
#include <sof/audio/sr_audio/sr_audio.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/ipc/msg.h>
#include <sof/lib/alloc.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

//#define _ADD_TEST_CLICK_AT48

static const struct comp_driver comp_sraudio;

// {9C5B18CC-12A5-4904-A84A-7E1FE86E0927}
DECLARE_SOF_RT_UUID("sraudio", sraudio_uuid, 0x9c5b18cc, 0x12a5, 0x4904, 0xa8, 0x4a, 0x7e, 0x1f, 0xe8, 0x6e, 0x9, 0x27);

DECLARE_TR_CTX(sraudio_tr, SOF_UUID(sraudio_uuid), LOG_LEVEL_INFO);

#define PARAM_CONTEXT_SIZE   55000 // iSST

#define USE_STATIC_CTX

typedef struct SR_HIFI2_Context_t {
#if !defined(_MSC_VER) && !defined(USE_STATIC_CTX)
  int32_t ctxBuffer[PARAM_CONTEXT_SIZE / sizeof(int32_t) + 1];
#else
  void* ctxBuffer;
#endif
  int32_t configParamID;
  int32_t writeParamCount;
  int32_t writeParamID;
  int32_t writeParamDataSize;
  int32_t writeParamDataOffset;
  int32_t writeParamDataOffsetEnd;
  int32_t readParamCount;
  int32_t readParamID;
  int32_t readParamDataSize;
  int32_t readParamDataOffset;
  int32_t readParamDataOffsetEnd;
} SR_HIFI2_Context_t;

// Initializes/resets context
void SR_HIFI2_Initialize(SR_HIFI2_Context_t* libCtx);

// Clears internal buffers. For flags, see SR_HIFI2_CLEAR_...
void SR_HIFI2_Clear(SR_HIFI2_Context_t* libCtx, int flags);

// Clears internal buffers asynchronously (inside the processing), for flags see SR_HIFI2_CLEAR_...
void SR_HIFI2_AsyncClear(SR_HIFI2_Context_t* libCtx, int flags);

// The below two functions allow sending/receiving parameter data in fragments.
//
// paramId: parameter id
// paramData: points to parameter data (or to current data fragment, if data is fragmented)
// dataSize: overall data size
// dataFragmentSize: size of current fragment
// isFirstFragment: 1 if paramData points to first fragment, 0 otherwise
//
// If non-fragmented data (aka single fragment) is passed, just call functions as follows below:
// SR_HIFI2_SetParameter(context, paramId, paramData, dataSize, dataSize, 1);
//   or
// SR_HIFI2_GetParameter(context, paramId, paramData, dataSize, dataSize, 1);
//
void SR_HIFI2_SetParameter(SR_HIFI2_Context_t* libCtx, int paramId, const uint8_t* paramData, int dataSize, int dataFragmentSize, int isFirstFragment);
void SR_HIFI2_GetParameter(SR_HIFI2_Context_t* libCtx, int paramId, uint8_t* paramData, int dataSize, int dataFragmentSize, int isFirstFragment);

// Toggles bypass mode (bypass = 1 for bypassing, 0 otherwise)
void SR_HIFI2_Bypass(SR_HIFI2_Context_t* libCtx, int bypass);

// Processing function.
// frameCount: number of samples in each input/output buffers
void SR_HIFI2_Process(SR_HIFI2_Context_t* libCtx, int frameCount, const int32_t* inputL, const int32_t* inputR, int32_t* outputL, int32_t* outputR);

uint32_t xtensa_get_ccount();

#ifdef _ADD_TEST_CLICK_AT48
static void _add_test_click_at47(int32_t* samplesL, int32_t* samplesR) {
  samplesL[47] = 0;
  samplesR[47] = 0;
}
#endif

#define maxChannelCount 2
#define bufferFrameCount 48

// SRAUDIO component private data
struct comp_data {
  struct comp_data_blob_handler* model_handler;
  struct sof_sraudio_config* config;
  size_t configSize;
  enum sof_ipc_frame source_format; // source frame format
  enum sof_ipc_frame sink_format;   // sink frame format
  SR_HIFI2_Context_t sraudio_ctx;   // processing context
  int32_t inProcessBuffer[maxChannelCount][bufferFrameCount];
  int32_t outProcessBuffer[maxChannelCount][bufferFrameCount];
  sraudio_func sraudio_func;        // processing function
};

#if CONFIG_FORMAT_S16LE
static void sraudio_s16_default(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frameCount) {
  struct comp_data* cd = comp_get_drvdata(dev);
  int32_t ch, i, j;
  uint32_t inChannelCount = source->channels;
  uint32_t outChannelCount = sink->channels;
  uint32_t inBufferSampleCount = bufferFrameCount * inChannelCount;
  uint32_t outBufferSampleCount = bufferFrameCount * outChannelCount;
  uint32_t tailFrameCount = frameCount;
  uint32_t inOffset = 0;
  uint32_t outOffset = 0;
  SR_HIFI2_Context_t* ctx = &cd->sraudio_ctx;
  for (; tailFrameCount >= bufferFrameCount; tailFrameCount -= bufferFrameCount, inOffset += inBufferSampleCount, outOffset += outBufferSampleCount) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < bufferFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < bufferFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = (int32_t)*((int16_t*)audio_stream_read_frag_s16(source, j)) << 16;
    }
    SR_HIFI2_Process(ctx, bufferFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < bufferFrameCount; i++, j += outChannelCount)
        *(int16_t*)audio_stream_write_frag_s16(sink, j) = sat_int16(Q_SHIFT_RND(cd->outProcessBuffer[ch][i], 31, 15));
    }
  }
  if (tailFrameCount != 0) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < tailFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < tailFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = (int32_t)*((int16_t*)audio_stream_read_frag_s16(source, j)) << 16;
    }
    SR_HIFI2_Process(ctx, tailFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < tailFrameCount; i++, j += outChannelCount)
        *(int16_t*)audio_stream_write_frag_s16(sink, j) = sat_int16(Q_SHIFT_RND(cd->outProcessBuffer[ch][i], 31, 15));
    }
  }
}
#endif

#if CONFIG_FORMAT_S24LE
static void sraudio_s24_default(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frameCount) {
  struct comp_data* cd = comp_get_drvdata(dev);
  int32_t ch, i, j;
  uint32_t inChannelCount = source->channels;
  uint32_t outChannelCount = sink->channels;
  uint32_t inBufferSampleCount = bufferFrameCount * inChannelCount;
  uint32_t outBufferSampleCount = bufferFrameCount * outChannelCount;
  uint32_t tailFrameCount = frameCount;
  uint32_t inOffset = 0;
  uint32_t outOffset = 0;
  SR_HIFI2_Context_t* ctx = &cd->sraudio_ctx;
  for (; tailFrameCount >= bufferFrameCount; tailFrameCount -= bufferFrameCount, inOffset += inBufferSampleCount, outOffset += outBufferSampleCount) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < bufferFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < bufferFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = *((int32_t*)audio_stream_read_frag_s32(source, j)) << 8;
    }
    SR_HIFI2_Process(ctx, bufferFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < bufferFrameCount; i++, j += outChannelCount)
        *(int32_t*)audio_stream_write_frag_s32(sink, j) = sat_int24(Q_SHIFT_RND(cd->outProcessBuffer[ch][i], 31, 23));
    }
  }
  if (tailFrameCount != 0) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < tailFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < tailFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = *((int32_t*)audio_stream_read_frag_s32(source, j)) << 8;
    }
    SR_HIFI2_Process(ctx, tailFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < tailFrameCount; i++, j += outChannelCount)
        *(int32_t*)audio_stream_write_frag_s32(sink, j) = sat_int24(Q_SHIFT_RND(cd->outProcessBuffer[ch][i], 31, 23));
    }
  }
}
#endif

#if CONFIG_FORMAT_S32LE
static void sraudio_s32_default(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frameCount) {
  struct comp_data* cd = comp_get_drvdata(dev);
  int32_t ch, i, j;
  uint32_t inChannelCount = source->channels;
  uint32_t outChannelCount = sink->channels;
  uint32_t inBufferSampleCount = bufferFrameCount * inChannelCount;
  uint32_t outBufferSampleCount = bufferFrameCount * outChannelCount;
  uint32_t tailFrameCount = frameCount;
  uint32_t inOffset = 0;
  uint32_t outOffset = 0;
  SR_HIFI2_Context_t* ctx = &cd->sraudio_ctx;
  for (; tailFrameCount >= bufferFrameCount; tailFrameCount -= bufferFrameCount, inOffset += inBufferSampleCount, outOffset += outBufferSampleCount) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < bufferFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < bufferFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = *((int32_t*)audio_stream_read_frag_s32(source, j));
    }
    SR_HIFI2_Process(ctx, bufferFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < bufferFrameCount; i++, j += outChannelCount)
        *(int32_t*)audio_stream_write_frag_s32(sink, j) = cd->outProcessBuffer[ch][i];
    }
  }
  if (tailFrameCount != 0) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < tailFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < tailFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = *((int32_t*)audio_stream_read_frag_s32(source, j));
    }
    SR_HIFI2_Process(ctx, tailFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < tailFrameCount; i++, j += outChannelCount)
        *(int32_t*)audio_stream_write_frag_s32(sink, j) = cd->outProcessBuffer[ch][i];
    }
  }
}
#endif

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE
static void sraudio_s32_16_default(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frameCount) {
  struct comp_data* cd = comp_get_drvdata(dev);
  int32_t ch, i, j;
  uint32_t inChannelCount = source->channels;
  uint32_t outChannelCount = sink->channels;
  uint32_t inBufferSampleCount = bufferFrameCount * inChannelCount;
  uint32_t outBufferSampleCount = bufferFrameCount * outChannelCount;
  uint32_t tailFrameCount = frameCount;
  uint32_t inOffset = 0;
  uint32_t outOffset = 0;
  SR_HIFI2_Context_t* ctx = &cd->sraudio_ctx;
  for (; tailFrameCount >= bufferFrameCount; tailFrameCount -= bufferFrameCount, inOffset += inBufferSampleCount, outOffset += outBufferSampleCount) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < bufferFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < bufferFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = *((int32_t*)audio_stream_read_frag_s16(source, j));
    }
    SR_HIFI2_Process(ctx, bufferFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < bufferFrameCount; i++, j += outChannelCount)
        *(int16_t*)audio_stream_write_frag_s16(sink, j) = sat_int16(Q_SHIFT_RND(cd->outProcessBuffer[ch][i], 31, 15));
    }
  }
  if (tailFrameCount != 0) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < tailFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < tailFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = *((int32_t*)audio_stream_read_frag_s16(source, j));
    }
    SR_HIFI2_Process(ctx, tailFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < tailFrameCount; i++, j += outChannelCount)
        *(int16_t*)audio_stream_write_frag_s16(sink, j) = sat_int16(Q_SHIFT_RND(cd->outProcessBuffer[ch][i], 31, 15));
    }
  }
}
#endif

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE
static void sraudio_s32_24_default(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frameCount) {
  struct comp_data* cd = comp_get_drvdata(dev);
  int32_t ch, i, j;
  uint32_t inChannelCount = source->channels;
  uint32_t outChannelCount = sink->channels;
  uint32_t inBufferSampleCount = bufferFrameCount * inChannelCount;
  uint32_t outBufferSampleCount = bufferFrameCount * outChannelCount;
  uint32_t tailFrameCount = frameCount;
  uint32_t inOffset = 0;
  uint32_t outOffset = 0;
  SR_HIFI2_Context_t* ctx = &cd->sraudio_ctx;
  for (; tailFrameCount >= bufferFrameCount; tailFrameCount -= bufferFrameCount, inOffset += inBufferSampleCount, outOffset += outBufferSampleCount) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < bufferFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < bufferFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = *((int32_t*)audio_stream_read_frag_s32(source, j));
    }
    SR_HIFI2_Process(ctx, bufferFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < bufferFrameCount; i++, j += outChannelCount)
        *(int32_t*)audio_stream_write_frag_s32(sink, j) = sat_int24(Q_SHIFT_RND(cd->outProcessBuffer[ch][i], 31, 23));
    }
  }
  if (tailFrameCount != 0) {
    for (ch = inChannelCount; ch < maxChannelCount; ch++)
      for (i = 0; i < tailFrameCount; i++)
        cd->inProcessBuffer[ch][i] = 0;
    for (ch = 0; ch < inChannelCount; ch++) {
      for (i = 0, j = ch + inOffset; i < tailFrameCount; i++, j += inChannelCount)
        cd->inProcessBuffer[ch][i] = *((int32_t*)audio_stream_read_frag_s32(source, j));
    }
    SR_HIFI2_Process(ctx, tailFrameCount, cd->inProcessBuffer[0], cd->inProcessBuffer[1], cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#ifdef _ADD_TEST_CLICK_AT48
    _add_test_click_at47(cd->outProcessBuffer[0], cd->outProcessBuffer[1]);
#endif
    for (ch = 0; ch < outChannelCount; ch++) {
      for (i = 0, j = ch + outOffset; i < tailFrameCount; i++, j += outChannelCount)
        *(int32_t*)audio_stream_write_frag_s32(sink, j) = sat_int24(Q_SHIFT_RND(cd->outProcessBuffer[ch][i], 31, 23));
    }
  }
}
#endif

static void sraudio_pass(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frames) {
  audio_stream_copy(source, 0, sink, 0, frames * source->channels);
}

#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
static void sraudio_s32_s16_pass(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frameCount) {
  int32_t* x;
  int16_t* y;
  int i;
  int n = frameCount * source->channels;
  for (i = 0; i < n; i++) {
    x = audio_stream_read_frag_s32(source, i);
    y = audio_stream_write_frag_s16(sink, i);
    *y = sat_int16(Q_SHIFT_RND(*x, 31, 15));
  }
}
#endif

#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
static void sraudio_s32_s24_pass(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frameCount) {
  int32_t* x;
  int32_t* y;
  int i;
  int n = frameCount * source->channels;
  for (i = 0; i < n; i++) {
    x = audio_stream_read_frag_s32(source, i);
    y = audio_stream_write_frag_s16(sink, i);
    *y = sat_int24(Q_SHIFT_RND(*x, 31, 23));
  }
}
#endif

const struct sraudio_func_map fm_configured[] = {
#if CONFIG_FORMAT_S16LE
  {SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, sraudio_s16_default},
#endif
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE
  {SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, NULL},
  {SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, NULL},
#endif
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
  {SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, NULL},
  {SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, sraudio_s32_16_default},
#endif
#if CONFIG_FORMAT_S24LE
  {SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, sraudio_s24_default},
#endif
#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
  {SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, NULL},
  {SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, sraudio_s32_24_default},
#endif
#if CONFIG_FORMAT_S32LE
  {SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, sraudio_s32_default},
#endif
};

const struct sraudio_func_map fm_passthrough[] = {
#if CONFIG_FORMAT_S16LE
  {SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S16_LE, sraudio_pass},
#endif
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE
  {SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S24_4LE, NULL},
  {SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE, NULL},
#endif
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
  {SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_S32_LE, NULL},
  {SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S16_LE, sraudio_s32_s16_pass},
#endif
#if CONFIG_FORMAT_S24LE
  {SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, sraudio_pass},
#endif
#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
  {SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE, NULL},
  {SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S24_4LE, sraudio_s32_s24_pass},
#endif
#if CONFIG_FORMAT_S32LE
  {SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_S32_LE, sraudio_pass},
#endif
};

static sraudio_func sraudio_find_func(enum sof_ipc_frame source_format, enum sof_ipc_frame sink_format, const struct sraudio_func_map* map, int n) {
  int i;
  for (i = 0; i < n; i++) {
    if ((uint8_t)source_format != map[i].source)
      continue;
    if ((uint8_t)sink_format != map[i].sink)
      continue;
    return map[i].func;
  }
  return NULL;
}

static int sraudio_setup(struct comp_data* cd, int nch) {
  uint32_t* entries = (uint32_t*)cd->config;
  uint32_t* entriesEnd = (uint32_t*)((uint8_t*)cd->config + cd->configSize);
  while (entries < entriesEnd) {
    uint32_t paramHeader = *entries++;
    int paramId = paramHeader & 0xFFFF;
    int paramDataLength = paramHeader >> 16;
    uint32_t* paramData = entries;
    entries += paramDataLength;
    SR_HIFI2_SetParameter(&cd->sraudio_ctx, paramId, (uint8_t*)paramData, paramDataLength << 2, paramDataLength << 2, 1);
  }
  SR_HIFI2_Clear(&cd->sraudio_ctx, -1);
  return 0;
}

static struct comp_dev* sraudio_new(const struct comp_driver* drv, struct sof_ipc_comp* comp) {
  struct comp_dev* dev;
  struct comp_data* cd;
  struct sof_ipc_comp_process* sraudio;
  struct sof_ipc_comp_process* ipc_sraudio = (struct sof_ipc_comp_process*)comp;
  size_t bs = ipc_sraudio->size;
  int ret;
  comp_cl_info(&comp_sraudio, "sraudio_new()");
  dev = comp_alloc(drv, COMP_SIZE(struct sof_ipc_comp_process));
  if (!dev)
    return NULL;
  sraudio = COMP_GET_IPC(dev, sof_ipc_comp_process);
  ret = memcpy_s(sraudio, sizeof(*sraudio), ipc_sraudio, sizeof(struct sof_ipc_comp_process));
  assert(!ret);
  cd = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
  if (!cd) {
    rfree(dev);
    return NULL;
  }
  comp_set_drvdata(dev, cd);
  cd->sraudio_func = NULL;
  cd->model_handler = comp_data_blob_handler_new(dev);
  if (!cd->model_handler) {
    comp_cl_err(&comp_sraudio, "sraudio_new(): comp_data_blob_handler_new() failed.");
    rfree(dev);
    rfree(cd);
    return NULL;
  }
  ret = comp_init_data_blob(cd->model_handler, bs, ipc_sraudio->data);
  if (ret < 0) {
    comp_cl_err(&comp_sraudio, "sraudio_new(): comp_init_data_blob() failed.");
    rfree(dev);
    rfree(cd);
    return NULL;
  }
  SR_HIFI2_Initialize(&cd->sraudio_ctx);
  dev->state = COMP_STATE_READY;
  return dev;
}

static void sraudio_free(struct comp_dev* dev) {
  struct comp_data* cd = comp_get_drvdata(dev);
  comp_info(dev, "sraudio_free()");
  comp_data_blob_handler_free(cd->model_handler);
  rfree(cd);
  rfree(dev);
}

static int sraudio_verify_params(struct comp_dev* dev,
  struct sof_ipc_stream_params* params) {
  struct comp_buffer* sourceb;
  struct comp_buffer* sinkb;
  uint32_t buffer_flag;
  int ret;
  comp_dbg(dev, "sraudio_verify_params()");
  sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
  sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
  buffer_flag = sraudio_find_func(sourceb->stream.frame_fmt, sinkb->stream.frame_fmt, fm_configured, ARRAY_SIZE(fm_configured)) ? BUFF_PARAMS_FRAME_FMT : 0;
  ret = comp_verify_params(dev, buffer_flag, params);
  if (ret < 0) {
    comp_err(dev, "sraudio_verify_params(): comp_verify_params() failed.");
    return ret;
  }
  return 0;
}

static int sraudio_params(struct comp_dev* dev, struct sof_ipc_stream_params* params) {
  int err;
  comp_info(dev, "sraudio_params()");
  err = sraudio_verify_params(dev, params);
  if (err < 0) {
    comp_err(dev, "sraudio_params(): pcm params verification failed.");
    return -EINVAL;
  }
  return 0;
}

static int sraudio_cmd_get_data(struct comp_dev* dev, struct sof_ipc_ctrl_data* cdata, int max_size) {
  struct comp_data* cd = comp_get_drvdata(dev);
  int ret = 0;
  switch (cdata->cmd) {
  case SOF_CTRL_CMD_BINARY:
    comp_info(dev, "sraudio_cmd_get_data(), SOF_CTRL_CMD_BINARY");
    ret = comp_data_blob_get_cmd(cd->model_handler, cdata,
      max_size);
    break;
  default:
    comp_err(dev, "sraudio_cmd_get_data(), invalid command");
    ret = -EINVAL;
    break;
  }
  return ret;
}

static int sraudio_cmd_set_data(struct comp_dev* dev, struct sof_ipc_ctrl_data* cdata) {
  struct comp_data* cd = comp_get_drvdata(dev);
  int ret = 0;
  switch (cdata->cmd) {
  case SOF_CTRL_CMD_BINARY:
    comp_info(dev, "sraudio_cmd_set_data(), SOF_CTRL_CMD_BINARY");
    ret = comp_data_blob_set_cmd(cd->model_handler, cdata);
    break;
  default:
    comp_err(dev, "sraudio_cmd_set_data(), invalid command");
    ret = -EINVAL;
    break;
  }
  return ret;
}

static int sraudio_cmd(struct comp_dev* dev, int cmd, void* data, int max_data_size) {
  struct sof_ipc_ctrl_data* cdata = ASSUME_ALIGNED(data, 4);
  int ret = 0;
  comp_info(dev, "sraudio_cmd()");
  switch (cmd) {
  case COMP_CMD_SET_DATA:
    ret = sraudio_cmd_set_data(dev, cdata);
    break;
  case COMP_CMD_GET_DATA:
    ret = sraudio_cmd_get_data(dev, cdata, max_data_size);
    break;
  default:
    comp_err(dev, "sraudio_cmd(), invalid command");
    ret = -EINVAL;
  }
  return ret;
}

static int sraudio_trigger(struct comp_dev* dev, int cmd) {
  struct comp_data* cd = comp_get_drvdata(dev);
  comp_info(dev, "sraudio_trigger()");
  if (cmd == COMP_TRIGGER_START || cmd == COMP_TRIGGER_RELEASE)
    assert(cd->sraudio_func);
  return comp_set_state(dev, cmd);
}

static void sraudio_process(struct comp_dev* dev, struct comp_buffer* source, struct comp_buffer* sink, int frames, uint32_t source_bytes, uint32_t sink_bytes) {
  struct comp_data* cd = comp_get_drvdata(dev);
  buffer_invalidate(source, source_bytes);
  cd->sraudio_func(dev, &source->stream, &sink->stream, frames);
  buffer_writeback(sink, sink_bytes);
  comp_update_buffer_consume(source, source_bytes);
  comp_update_buffer_produce(sink, sink_bytes);
}

static int sraudio_copy(struct comp_dev* dev) {
  struct comp_copy_limits cl;
  struct comp_data* cd = comp_get_drvdata(dev);
  struct comp_buffer* sourceb;
  struct comp_buffer* sinkb;
  int ret;
  comp_dbg(dev, "sraudio_copy()");
  sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
  if (comp_is_new_data_blob_available(cd->model_handler)) {
    cd->config = comp_get_data_blob(cd->model_handler, &cd->configSize, NULL);
    ret = sraudio_setup(cd, sourceb->stream.channels);
    if (ret < 0) {
      comp_err(dev, "sraudio_copy(), SRAUDIO setup failed");
      return ret;
    }
  }
  sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
  comp_get_copy_limits_with_lock(sourceb, sinkb, &cl);
  sraudio_process(dev, sourceb, sinkb, cl.frames, cl.source_bytes, cl.sink_bytes);
  return 0;
}

static int sraudio_prepare(struct comp_dev* dev) {
  struct comp_data* cd = comp_get_drvdata(dev);
  struct sof_ipc_comp_config* config = dev_comp_config(dev);
  struct comp_buffer* sourceb;
  struct comp_buffer* sinkb;
  uint32_t sink_period_bytes;
  int ret;
  comp_info(dev, "sraudio_prepare()");
  ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
  if (ret < 0)
    return ret;
  if (ret == COMP_STATUS_STATE_ALREADY_SET)
    return PPL_STATUS_PATH_STOP;
  sourceb = list_first_item(&dev->bsource_list, struct comp_buffer, sink_list);
  sinkb = list_first_item(&dev->bsink_list, struct comp_buffer, source_list);
  cd->source_format = sourceb->stream.frame_fmt;
  cd->sink_format = sinkb->stream.frame_fmt;
  sink_period_bytes = audio_stream_period_bytes(&sinkb->stream, dev->frames);
  if (sinkb->stream.size < config->periods_sink * sink_period_bytes) {
    comp_err(dev, "sraudio_prepare(): sink buffer size %d is insufficient < %d * %d",
      sinkb->stream.size, config->periods_sink, sink_period_bytes);
    ret = -ENOMEM;
    goto err;
  }
  cd->config = comp_get_data_blob(cd->model_handler, &cd->configSize, NULL);
  comp_info(dev, "sraudio_prepare(), source_format=%d, sink_format=%d", cd->source_format, cd->sink_format);
  if (cd->config) {
    ret = sraudio_setup(cd, sourceb->stream.channels);
    if (ret < 0) {
      comp_err(dev, "sraudio_prepare(), setup failed.");
      goto err;
    }
    cd->sraudio_func = sraudio_find_func(cd->source_format, cd->sink_format, fm_configured, ARRAY_SIZE(fm_configured));
    if (!cd->sraudio_func) {
      comp_err(dev, "sraudio_prepare(), No proc func");
      ret = -EINVAL;
      goto err;
    }
    comp_info(dev, "sraudio_prepare(), SRAUDIO is configured.");
  } else {
    cd->sraudio_func = sraudio_find_func(cd->source_format, cd->sink_format, fm_passthrough, ARRAY_SIZE(fm_passthrough));
    if (!cd->sraudio_func) {
      comp_err(dev, "sraudio_prepare(), No pass func");
      ret = -EINVAL;
      goto err;
    }
    comp_info(dev, "sraudio_prepare(), pass-through mode.");
  }
  return 0;
err:
  comp_set_state(dev, COMP_TRIGGER_RESET);
  return ret;
}

static int sraudio_reset(struct comp_dev* dev) {
  struct comp_data* cd = comp_get_drvdata(dev);
  comp_info(dev, "sraudio_reset()");
  cd->sraudio_func = NULL;
  comp_set_state(dev, COMP_TRIGGER_RESET);
  return 0;
}

static const struct comp_driver comp_sraudio = {
  .type = SOF_COMP_SRAUDIO,
  .uid = SOF_RT_UUID(sraudio_uuid),
  .tctx = &sraudio_tr,
  .ops = {
    .create = sraudio_new,
    .free = sraudio_free,
    .params = sraudio_params,
    .cmd = sraudio_cmd,
    .trigger = sraudio_trigger,
    .copy = sraudio_copy,
    .prepare = sraudio_prepare,
    .reset = sraudio_reset,
},
};

static SHARED_DATA struct comp_driver_info comp_sraudio_info = {
  .drv = &comp_sraudio,
};

UT_STATIC void sys_comp_sraudio_init(void) {
  comp_register(platform_shared_get(&comp_sraudio_info, sizeof(comp_sraudio_info)));
}

DECLARE_MODULE(sys_comp_sraudio_init);
