/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Maxim Integrated All rights reserved.
 *
 * Author: Ryan Lee <ryans.lee@maximintegrated.com>
 *
 * Copyright(c) 2023 Google LLC.
 *
 * Author: Pin-chih Lin <johnylin@google.com>
 */

#ifndef __SOF_AUDIO_DSM_H__
#define __SOF_AUDIO_DSM_H__

#include <sof/platform.h>
#include <sof/audio/component.h>

/* Smart Amplifier component is a two-layer structured design, i.e. generic
 * layer and inner model layer. The latter can have various implementations
 * respectively for Amplifier solution suppliers, while the former is the common
 * part of Smart Amp process adaptable for all solutions.
 *
 * In structural aspect, one can regard generic layer as the glue code that
 * wraps inner model in a SOF component. Ops are defined for interaction between
 * two layers. Inner model is the solution-specific modular code, which may have
 * static libraries linked as needed. The structure is figured below:
 *
 *                                 SRC(FF)   SINK(OUT)  +-SRC(FB)  bytectl
 * +- SMART_AMP         |^ comp ops    |         ^      |           ^|
 * | +------------------v|-------------v---------|------v-----------|v---------+
 * | | Generic Layer                 (chan remap/fmt conv)          ||         |
 * | |    (memory mgr)--+------> :::::::::BUFFERS:::::::::::::      |+> CONFIG |
 * | +------------------|-|^-----------|---------^------|-----------^|---------+
 * |                    | || mod ops   |         |      |           ||
 * | +------------------v-v|-----------v---------|------v-----------|v---------+
 * | | Inner Model   :::::::::::::::::::::BUFFERS:::::::::::::::::::::::       |
 * | |                  (solution-specific impl/wrapper)            |+> MODEL  |
 * | +------------------------------|^------------------------------^----------+
 * +---                             v| lib ops                      | CALDATA
 *                            Static Libs (as needed)     ----------+
 * Note:
 * - FF(Feed-Forward): un-processed playback frame source
 * - FB(Feedback): feedback reference frame source (from the capture pipeline)
 *
 * As illustrated above, generic layer handles the cross-communication for inner
 * model and SOF pipeline flow, as well as the smart-amp common tasks including:
 * 1. Channel remapping for input/output frames
 * 2. Frame format conversion for input/output frames. It allows inner model to
 *    work with different format from SOF audio stream.
 *   (Now it only allows the bitdepth of inner model format >= SOF stream,
 *    e.g. inner model: S32_LE, SOF stream: S16_LE)
 * 3. Full-management of runtime memory. That is, dynamic memory buffers either
 *    required by generic layer or inner model will be allocated/owned/released
 *    by generic layer.
 * More details are stated in the comment of each struct/function later.
 */

/* Maximum number of channels for algorithm in */
#define SMART_AMP_FF_MAX_CH_NUM		2
/* Maximum number of channels for algorithm out */
#define SMART_AMP_FF_OUT_MAX_CH_NUM	4
/* Maximum number of channels for feedback  */
#define SMART_AMP_FB_MAX_CH_NUM		4

#define SMART_AMP_FRM_SZ	48 /* frames per 1ms */
#define SMART_AMP_FF_BUF_SZ	(SMART_AMP_FRM_SZ * SMART_AMP_FF_MAX_CH_NUM)
#define SMART_AMP_FB_BUF_SZ	(SMART_AMP_FRM_SZ * SMART_AMP_FB_MAX_CH_NUM)

#define SMART_AMP_FF_BUF_DB_SZ\
	(SMART_AMP_FF_BUF_SZ * SMART_AMP_FF_MAX_CH_NUM)
#define SMART_AMP_FB_BUF_DB_SZ\
	(SMART_AMP_FB_BUF_SZ * SMART_AMP_FB_MAX_CH_NUM)

struct inner_model_ops;

/* The common base for inner model data structs. Inner model should declare its
 * model data struct while putting this base as the leading member of struct, e.g.
 *    struct solution_foo_mod_data {
 *            struct smart_amp_mod_data_base base;
 *            uint32_t foo_version;
 *            struct foo_parameter_set;
 *            ...
 *    };
 *
 * Then implement mod_data_create() in its own C file, e.g.
 *    struct smart_amp_mod_data_base *mod_data_create(const struct comp_dev *dev)
 *    {
 *            struct solution_foo_mod_data *foo;
 *            foo = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*foo));
 *            assert(foo);
 *            foo->base.dev = dev;
 *            foo->base.mod_ops = foo_ops; // declared somewhere as static const
 *            ...
 *            return &foo->base;
 *    }
 */
struct smart_amp_mod_data_base {
	const struct comp_dev *dev; /* for logger tracing use only */
	const struct inner_model_ops *mod_ops;
};

/* The struct of memory buffer managed by generic layer. */
struct smart_amp_buf {
	void *data;
	uint32_t size;
};

/* For memory allocation, generic layer plays the active role that queries the
 * required memory size for inner model (then allocate and assign back) in
 * some specific moments, i.e. once before and after model initialized. Inner
 * model should consider buffers located and allocated onto 3 memory blocks in
 * terms of usage:
 *    PRIVATE - allocated before model init - for libraries internal usage
 *    FRAME   - allocated after model init  - for audio frame buffer usage
 *    PARAM   - allocated after model init  - for parameter blob usage
 */
enum smart_amp_mod_memblk {
	MOD_MEMBLK_PRIVATE = 0,
	MOD_MEMBLK_FRAME,
	MOD_MEMBLK_PARAM,
	MOD_MEMBLK_MAX
};

/* The intermediate audio data buffer in generic layer. */
struct smart_amp_mod_stream {
	struct smart_amp_buf buf;
	uint32_t channels;
	uint16_t frame_fmt;
	union {
		uint32_t consumed;  /* for source (in frames) */
		uint32_t produced;  /* for sink (in frames) */
	};
};

/******************************************************************************
 * Generic functions:                                                         *
 *    The implementation is placed in smart_amp_generic.c                     *
 ******************************************************************************/

typedef void (*smart_amp_src_func)(struct smart_amp_mod_stream *src_mod,
				   uint32_t frames,
				   const struct audio_stream __sparse_cache *src,
				   const int8_t *chan_map);

typedef void (*smart_amp_sink_func)(const struct smart_amp_mod_stream *sink_mod,
				    uint32_t frames,
				    const struct audio_stream __sparse_cache *sink);

struct smart_amp_func_map {
	uint16_t comp_fmt;
	uint16_t mod_fmt;
	smart_amp_src_func src_func;
	smart_amp_sink_func sink_func;
};

extern const struct smart_amp_func_map src_sink_func_map[];

smart_amp_src_func smart_amp_get_src_func(uint16_t comp_fmt, uint16_t mod_fmt);
smart_amp_sink_func smart_amp_get_sink_func(uint16_t comp_fmt, uint16_t mod_fmt);

/******************************************************************************
 * Inner model operations (mod ops):                                          *
 *    Model implementations are mutual exclusive (separated by Kconfig). It   *
 *    must be only one solution applicable per build. The solution-specific   *
 *    implementation is placed in its own C file smart_amp_${SOLUTION}.c      *
 ******************************************************************************/

/**
 * Creates the self-declared model data struct.
 * Refer to "struct smart_amp_mod_data_base" for details of declaration.
 * Args:
 *    dev - the comp_dev object for logger tracing use only.
 * Returns:
 *    The pointer of "smart_amp_mod_data_base" which is the leading member in
 *    the created model data struct.
 */
struct smart_amp_mod_data_base *mod_data_create(const struct comp_dev *dev);

/* All ops are mandatory. */
struct inner_model_ops {
	/**
	 * Initializes model.
	 * It will be called on comp_ops.create() by generic layer.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*init)(struct smart_amp_mod_data_base *mod);

	/**
	 * Returns the required bytesize for the specific memblk.
	 * It will be called on comp_ops.create() by generic layer, before or after
	 * mod_ops.init() according to the memblk usage.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 *    blk - the memblk usage type.
	 * Returns:
	 *    The bytesize or negative error code.
	 */
	int (*query_memblk_size)(struct smart_amp_mod_data_base *mod,
				 enum smart_amp_mod_memblk blk);

	/**
	 * Sets the allocated memblk info.
	 * It should be called in sequence after mod_ops.query_memblk_size().
	 * Args:
	 *    mod - the pointer of model data struct object.
	 *    blk - the memblk usage type.
	 *    buf - the pointer of smart_amp_buf object including allocated memory.
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*set_memblk)(struct smart_amp_mod_data_base *mod,
			  enum smart_amp_mod_memblk blk,
			  struct smart_amp_buf *buf);

	/**
	 * Returns the list of supported frame formats.
	 * It will be called on comp_ops.prepare() by generic layer.
	 *
	 * Inner model should report all supported formats in the list at once.
	 * Generic layer will resolve the applicable format according to this list and
	 * the formats requested from external source/sink stream buffers. If it turns
	 * out to be no format applicable, generic layer will report errors which will
	 * force the early-termination of the starting process for the whole pipeline.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 *    mod_fmts - the pointer for returning the supported format list.
	 *               Format values should be aligned to "enum sof_ipc_frame" and
	 *               put in ascending order.
	 *    num_mod_fmts - the pointer for returning the length of mod_fmts.
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*get_supported_fmts)(struct smart_amp_mod_data_base *mod,
				  const uint16_t **mod_fmts, int *num_mod_fmts);

	/**
	 * Sets the frame format after resolved.
	 * It will be called on comp_ops.prepare() by generic layer, in sequence after
	 * mod_ops.get_supported_fmts() if the format is resolvable.
	 *
	 * Inner model should honor the received format on processing. The FF and FB
	 * frames (if available) will be put to the input buffers in the same format.
	 * It will be the last function called before audio stream starts processing.
	 * needed, inner model should execute the preparing tasks as soon as it is
	 * called.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 *    mod_fmt - the frame format to be applied..
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*set_fmt)(struct smart_amp_mod_data_base *mod, uint16_t mod_fmt);

	/**
	 * Runs the feed-forward process.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 *    frames - the number of frames to be processed.
	 *    in - the pointer of input stream buffer object. Inner model should set
	 *         "consumed" to the number of conusmed frames.
	 *    out - the pointer of output stream buffer object. Inner model should set
	 *          "produced" to the number of produced frames.
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*ff_proc)(struct smart_amp_mod_data_base *mod,
		       uint32_t frames,
		       struct smart_amp_mod_stream *in,
		       struct smart_amp_mod_stream *out);

	/**
	 * Runs the feedback process.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 *    frames - the number of frames to be processed.
	 *    in - the pointer of input stream buffer object. Inner model should set
	 *         "consumed" to the number of conusmed frames.
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*fb_proc)(struct smart_amp_mod_data_base *mod,
		       uint32_t frames,
		       struct smart_amp_mod_stream *in);

	/**
	 * Gets config data from model.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 *    cdata - the pointer of sof_ipc_ctrl_data object.
	 *    size - the maximal size for config data to read.
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*get_config)(struct smart_amp_mod_data_base *mod,
			  struct sof_ipc_ctrl_data *cdata, uint32_t size);

	/**
	 * Sets config data to model.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 *    cdata - the pointer of sof_ipc_ctrl_data object.
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*set_config)(struct smart_amp_mod_data_base *mod,
			  struct sof_ipc_ctrl_data *cdata);

	/**
	 * Resets model.
	 * It will be called on comp_ops.reset() by generic layer.
	 * Args:
	 *    mod - the pointer of model data struct object.
	 * Returns:
	 *    0 or negative error code.
	 */
	int (*reset)(struct smart_amp_mod_data_base *mod);
};

#endif
