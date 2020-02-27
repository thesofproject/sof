#ifndef __SOF_AUDIO_DCBLOCK_DCBLOCK_H__
#define __SOF_AUDIO_DCBLOCK_DCBLOCK_H__

#include <stdint.h>
#include <sof/platform.h>
#include <ipc/stream.h>

struct audio_stream;
struct comp_dev;

struct dcblock_state {
  int32_t R_coeff; /**< Q1.31 */
	int32_t x_prev; /**< state variable referring to x[n-1] */
	int32_t y_prev; /**< state variable referring to y[n-1] */
};

struct sof_dcblock_config {
  /**
  * array of R coefficients in Q1.31 for the filter. Each i-th entry
  * corresponds to the value used for the i-th channel
  */
  int32_t R_coeffs[PLATFORM_MAX_CHANNELS];
  uint32_t reserved[4];	/**< reserved for future use */
} __attribute__((packed));

/**
  * \brief Type definition for the processing function for the
  * DC Blocking Filter.
  */
typedef void (*dcblock_func)(const struct comp_dev *dev,
          const struct audio_stream *source,
          const struct audio_stream *sink,
          uint32_t frames);

/* DC Blocking Filter component private data */
struct comp_data {
	 struct dcblock_state dcblock[PLATFORM_MAX_CHANNELS]; /**< filters state */
	 struct sof_dcblock_config *config; /**< config for the processing function */
	 struct sof_dcblock_config *config_new;
   enum sof_ipc_frame source_format;	/**< source frame format */
 	 enum sof_ipc_frame sink_format;		/**< sink frame format */
   dcblock_func dcblock_func;         /**< processing function */
};

/** \brief DC Blocking Filter processing functions map item. */
struct dcblock_func_map {
  enum sof_ipc_frame src_fmt;	    /**< source frame format */
	enum sof_ipc_frame sink_fmt;		/**< sink frame format */
	dcblock_func func;			        /**< processing function */
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct dcblock_func_map dcblock_fnmap[];

/** \brief Number of processing functions. */
extern const size_t dcblock_fncount;

/**
 * \brief Retrieives DC Blocking processing function.
 * \param[in,out] dev DC Blocking Filter base component device.
 */
static inline dcblock_func dcblock_find_func(enum sof_ipc_frame src_fmt,
          enum sof_ipc_frame sink_fmt)
{
  int i;

  /* Find suitable processing function from map */
  for(i = 0; i < dcblock_fncount; i++) {
    if(src_fmt != dcblock_fnmap[i].src_fmt)
      continue;
    if(sink_fmt != dcblock_fnmap[i].sink_fmt)
      continue;

    return dcblock_fnmap[i].func;
  }

  return NULL;
}

#endif /* __SOF_AUDIO_DCBLOCK_DCBLOCK_H__ */
