#ifndef __SOF_AUDIO_DCBLOCK_DCBLOCK_H__
#define __SOF_AUDIO_DCBLOCK_DCBLOCK_H__

#include <stdint.h>

struct audio_stream;
struct comp_dev;
/**
  * \brief Type definition for the processing function for the
  * DC Blocking Filter.
  */
typedef void (*dcblock_func)(const struct comp_dev *dev,
          const struct audio_stream *source,
          const struct audio_stream *sink,
          uint32_t frames);

/** \brief DC Blocking Filter processing functions map item. */
struct dcblock_func_map {
  enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */
	dcblock_func func;			/**< processing function */
};

#endif /* __SOF_AUDIO_DCBLOCK_DCBLOCK_H__ */
