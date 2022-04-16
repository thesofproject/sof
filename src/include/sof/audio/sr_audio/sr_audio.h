// Copyright(c) 2021 Sound Research Corporation. All rights reserved.
#ifndef __SOF_AUDIO_SRAUDIO_H__
#define __SOF_AUDIO_SRAUDIO_H__
#include <stdint.h>

struct audio_stream;
struct comp_dev;

typedef void (*sraudio_func)(const struct comp_dev* dev, const struct audio_stream* source, struct audio_stream* sink, uint32_t frames);

struct sraudio_func_map {
  uint8_t source;     // source frame format
  uint8_t sink;       // sink frame format
  sraudio_func func;  // processing function
};

#endif
