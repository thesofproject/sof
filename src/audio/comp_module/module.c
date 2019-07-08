// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 NXP. All rights reserved.
//
// Author: Paul Olaru <paul.olaru@nxp.com>

#include <config.h>

#include <stdint.h>
#include <sof/list.h>
#include <sof/audio/component.h>
#include <sof/ipc.h>
#include <sof/audio/module.h>

static struct comp_dev *module_new(struct sof_ipc_comp *comp)
{
	struct sof_ipc_comp_process *ipc_process =
		(struct sof_ipc_comp_process *)comp;
	struct comp_dev *dev;
	struct module_priv *priv;

	trace_module("module_new()");

	if (IPC_IS_SIZE_INVALID(ipc_process->config)) {
		IPC_SIZE_ERROR_TRACE(TRACE_CLASS_MODULE, ipc_process->config);
		return NULL;
	}

	dev = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		      COMP_SIZE(struct sof_ipc_comp_process));

	if (!dev)
		return NULL;

	assert(!memcpy_s(&dev->comp, sizeof(dev->comp), comp, sizeof(*comp)));

	priv = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
		       sizeof(struct module_priv));

	if (!priv) {
		rfree(dev);
		return NULL;
	}

	comp_set_drvdata(dev, priv);

	dev->state = COMP_STATE_READY;
	return dev;
}

static void module_free(struct comp_dev *dev)
{
	struct module_priv *priv = comp_get_drvdata(dev);

	trace_module("module_free()");

	reset_module_ops(dev);
	rfree(priv);
	comp_set_drvdata(dev, NULL);
	rfree(dev);
}

static int module_params(struct comp_dev *dev)
{
	struct module_priv *priv = comp_get_drvdata(dev);

	trace_module("module_params()");
	if (!priv->mod)
		return -EINVAL;
	if (priv->mod->ops.params)
		return priv->mod->ops.params(dev);
	return 0;
}

static int module_ctrl_set_cmd(struct comp_dev *dev,
			       struct sof_ipc_ctrl_data *cdata)
{
	struct module_priv *priv = comp_get_drvdata(dev);
	int ret = 0;
	int req_type;
	struct registered_module *mod;

	trace_module("module_ctrl_set_cmd(), cdata->cmd = 0x%08x", cdata->cmd);

	switch (cdata->cmd) {
	case SOF_CTRL_CMD_BINARY:
		req_type = (int)cdata->data->data;
		mod = find_module_by_type(req_type);
		ret = 0;
		if (!mod) {
			ret = -ENOENT;
			break;
		}
		ret = set_module_ops(dev, mod);
		if (ret < 0)
			break;
		/* Create the instance of the selected module by means
		 * of the new callback
		 */
		ret = priv->mod->ops.new(dev);
		if (ret < 0) {
			/* Something happened (out of memory?). We have
			 * to ensure no unintended use of other
			 * callbacks will happen.
			 */

			/* This will always succeed at this point (the
			 * "live" flag is never set)
			 */
			reset_module_ops(dev);
		}
		break;
	default:
		trace_module_error("module_ctrl_set_cmd(0 error:"
				  " invalid cdata->cmd ="
				   " 0x%08x", cdata->cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int module_cmd(struct comp_dev *dev, int cmd, void *data,
		      int max_data_size)
{
	struct sof_ipc_ctrl_data *cdata = data;
	struct module_priv *priv = comp_get_drvdata(dev);

	/* Just pass the command to the component once it's up */
	if (priv->mod)
		return priv->mod->ops.cmd(dev, cmd, data, max_data_size);

	trace_module("module_cmd() cmd = 0x%08x", cmd);

	switch (cmd) {
	case COMP_CMD_SET_DATA:
		return module_ctrl_set_cmd(dev, cdata);
	default:
		return -EINVAL;
	}
}

/* process and copy stream data from source to sink buffers */
static int module_copy(struct comp_dev *dev)
{
	struct module_priv *priv = comp_get_drvdata(dev);

	if (!priv->mod)
		return -EINVAL; /* Module not loaded */
	if (!priv->mod->ops.copy)
		return -EINVAL; /* Copy not implemented by module */
	return priv->mod->ops.copy(dev);
}

static int module_reset(struct comp_dev *dev)
{
	struct module_priv *priv = comp_get_drvdata(dev);
	int ret = 0;

	if (priv->mod && priv->mod->ops.reset)
		ret = priv->mod->ops.reset(dev);
	if (ret < 0) {
		trace_module_error("module_reset(): reset callback failed!"
				   " Resetting anyway...");
		/* Shouldn't happen; resets should always succeed!
		 * Continuing.
		 */
	}

	/* Free the module to allow another to load */

	if (priv->mod) {
		if (priv->mod->ops.free)
			priv->mod->ops.free(dev);
		else
			trace_module_error("module_reset() "
					   "missing module free");
	}

	/* To ensure resets succeed anyway */
	comp_set_state(dev, COMP_TRIGGER_RESET);
	priv->live = 0;

	reset_module_ops(dev);

	return 0;
}

static int module_prepare(struct comp_dev *dev)
{
	int ret;
	struct module_priv *priv = comp_get_drvdata(dev);

	trace_module("module_prepare()");

	ret = comp_set_state(dev, COMP_TRIGGER_PREPARE);
	if (ret) {
		trace_module("module_prepare() comp_set_state()"
			     " returned non-zero.");
		return ret;
	}

	if (!priv->mod)
		return -EINVAL; /* No module loaded */

	if (priv->mod->ops.prepare)
		return priv->mod->ops.prepare(dev);

	return 0;
}

static int module_trigger(struct comp_dev *dev, int cmd)
{
	struct module_priv *priv = comp_get_drvdata(dev);

	trace_module("module_trigger(0, command = %u", cmd);

	if (!priv->mod)
		return -EINVAL; /* No module loaded */

	if (priv->mod->ops.trigger)
		return priv->mod->ops.trigger(dev, cmd);

	/* Default functionality */
	return comp_set_state(dev, cmd);
}

static struct comp_driver comp_module = {
	.type	= SOF_COMP_MODULE,
	.ops	= {
		.new		= module_new,
		.free		= module_free,
		.params		= module_params,
		.cmd		= module_cmd,
		.copy		= module_copy,
		.prepare	= module_prepare,
		.reset		= module_reset,
		.trigger	= module_trigger,
	},
};

static struct list_item registered_modules;

static void sys_comp_module_init(void)
{
	list_init(&registered_modules);
	comp_register(&comp_module);
}

DECLARE_MODULE(sys_comp_module_init);

void register_module(struct registered_module *mod)
{
	if (!mod)
		return; /* Ignore invalid requests */
	list_item_prepend(&mod->list, &registered_modules);
	mod->refs = 0;
}

int unregister_module(struct registered_module *mod)
{
	if (!mod)
		return -EINVAL;
	if (mod->refs > 0)
		return -EBUSY;
	list_item_del(&mod->list);
	return 0;
}

int set_module_ops(struct comp_dev *dev, struct registered_module *mod)
{
	int ret;

	if (!dev || !mod)
		return -EINVAL;

	/* Check if the component has the right type */
	if (dev->drv != &comp_module)
		return -EINVAL;

	ret = reset_module_ops(dev);
	if (ret < 0)
		return ret;

	struct module_priv *priv = comp_get_drvdata(dev);

	priv->mod = mod;
	mod->refs++;
	return 0;
}

int reset_module_ops(struct comp_dev *dev)
{
	if (dev->drv != &comp_module)
		return -EINVAL;
	struct module_priv *priv = comp_get_drvdata(dev);

	if (priv->mod) {
		struct registered_module *mod = priv->mod;

		if (priv->live) {
			trace_module_error("reset_module_ops():"
					   " cannot remove module while pipeline is active");
			return -EBUSY;
		}

		priv->mod = NULL;
		mod->refs--;

		assert(mod->refs >= 0);
	}
	return 0;
}

struct registered_module *find_module_by_type(int module_type)
{
	struct list_item *it;

	/* Treat module_type as an opaque value */
	list_for_item(it, &registered_modules) {
		struct registered_module *mod;

		mod = list_item(it, struct registered_module, list);
		if (mod->module_type == module_type)
			return mod;
	}

	return NULL;
}

struct registered_module *find_first_free_module(void)
{
	struct list_item *it;

	list_for_item(it, &registered_modules) {
		struct registered_module *mod;

		mod = list_item(it, struct registered_module, list);
		if (!mod->refs)
			return mod;
	}

	return NULL;
}
