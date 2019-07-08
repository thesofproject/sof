/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 NXP. All rights reserved.
 *
 * Author: Paul Olaru <paul.olaru@nxp.com>
 */

/**
 * \file include/sof/audio/module.h
 * \brief Module component header file
 * \authors Paul Olaru <paul.olaru@nxp.com>
 */

#ifndef __SOF_AUDIO_MODULE_H__
#define __SOF_AUDIO_MODULE_H__

#include <config.h>

#if CONFIG_COMP_MODULE

#include <stdint.h>
#include <ipc/stream.h>
#include <sof/audio/component.h>

/* tracing */
#define trace_module(__e, ...) \
	trace_event(TRACE_CLASS_MODULE, __e, ##__VA_ARGS__)
#define trace_module_error(__e, ...) \
	trace_error(TRACE_CLASS_MODULE, __e, ##__VA_ARGS__)
#define tracev_module(__e, ...) \
	tracev_event(TRACE_CLASS_MODULE, __e, ##__VA_ARGS__)

struct module_ops {
	/** private data allocation */
	int (*new)(struct comp_dev *dev);
	/** private data free */
	void (*free)(struct comp_dev *dev);
	/** set component audio stream params */
	int (*params)(struct comp_dev *dev);
	/** used to pass standard and bespoke commands (with optional data) */
	int (*cmd)(struct comp_dev *dev, int cmd, void *data,
		   int max_data_size);
	/** atomic - used to start/stop/pause stream operations */
	int (*trigger)(struct comp_dev *dev, int cmd);
	/** prepare component after params are set */
	int (*prepare)(struct comp_dev *dev);
	/** reset component */
	int (*reset)(struct comp_dev *dev);
	/** copy and process stream data from source to sink buffers */
	int (*copy)(struct comp_dev *dev);
	/** position */
	int (*position)(struct comp_dev *dev,
			struct sof_ipc_stream_posn *posn);
	/** set attribute in component */
	int (*set_attribute)(struct comp_dev *dev, uint32_t type,
			     void *value);
};

struct module_priv {
	/** Store which module implements the operations for this
	 * component instance
	 */
	struct registered_module *mod;
	/** Component is doing work (protects the "mod" field) */
	bool live;
	/** Private per-component data for the module itself */
	void *private;
};

struct registered_module {
	/** Opaque, used to select a module to be loaded for a component */
	int module_type;
	/** The module functionality above */
	struct module_ops ops;
	/** Reference counter (how many components are using this module) */
	int refs;
	/** List used for registering and looking up the module */
	struct list_item list;
};

/** Register the module and allow it to be used with components.
 * Intended to be called back from any initialization functions (after
 * loading for dynamic modules, at boot for statically linked ones).
 *
 * @param mod A struct registered_module via which the module itself
 *	       exports its functionality
 */
void register_module(struct registered_module *mod);
/** Unregister a module and allow it to be removed from a system.
 * This must be called before any attempts to actually unload code, to
 * ensure said code is not actually in use.
 *
 * @param mod The same parameter given to register_module previously.
 * @ret 0 on success, -EBUSY if the module is in use. Might also return
 *      module-specific errors. On error, the module remains
 *      registered.
 */
int unregister_module(struct registered_module *mod);

#define module_get_drvdata(dev) \
	(((struct module_priv *)comp_get_drvdata(dev))->private)
#define module_set_drvdata(dev, priv) (module_get_drvdata(dev) = (priv))

/** Set which module controls this component.
 *
 * @param dev The component to be updated
 * @param mod The module (same parameter passed to register_module)
 * @ret 0 on success, some error code on failure.
 */
int set_module_ops(struct comp_dev *dev, struct registered_module *mod);
/** Set this component to no longer use a module.
 *
 * @param dev The component to be updated
 * @ret 0 on success, some error code on failure. If dev is of the
 *      correct type and an error (most likely -EBUSY) is returned, the
 *      component still uses the said module.
 */
int reset_module_ops(struct comp_dev *dev);

/** Locate a registered module based on its type.
 *
 * @param module_type The requested type
 * @ret a registered module, or NULL of none could be found that
 *      matches the request.
 */
struct registered_module *find_module_by_type(int module_type);

/** Locate a module not used by any components.
 * Intended to be used with dynamic modules (in case of out-of-memory
 * conditions we can locate a module to be unloaded and reclaim its
 * memory).
 *
 * @ret A registered module if it's not in use, or NULL if all are
 * being used.
 */
struct registered_module *find_first_free_module(void);

#endif /* CONFIG_COMP_MODULE */

#endif /* __SOF_AUDIO_MODULE_H__ */
