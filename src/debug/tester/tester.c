//SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Marcin Szkudlinski <marcin.szkudlinski@linux.intel.com>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/sink_source_utils.h>
#include "tester.h"
#include "tester_dummy_test.h"

/**
 * Tester module is a framework for a runtime testing that need a special test code
 * injected into the SOF system, i.e.
 *  - provide an extra load to CPU by running additional thread
 *  - execute some incorrect operations
 *
 * The idea is to allow using of special cases in testing like Continuous Integration or pre-release
 * testing using a standard, production build.
 *
 * In order to have only one testing module (in meaning of a SOF module with separate UUID),
 * a framework is introduced, where a test to be executed is selected by IPC parameter
 *
 * In production, module should be compiled as a loadable module, as it should not needed to be
 * loaded and used on customers' boards.
 * During developing, however, the module may be built in to keep debugging simpler
 *
 */

LOG_MODULE_REGISTER(tester, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(tester);

DECLARE_TR_CTX(tester_tr, SOF_UUID(tester_uuid), LOG_LEVEL_INFO);

struct tester_init_config {
	struct ipc4_base_module_cfg ipc4_cfg;
	int32_t test_type;
} __attribute__((packed, aligned(4)));

static int tester_init(struct processing_module *mod)
{
	struct module_data *md = &mod->priv;
	struct comp_dev *dev = mod->dev;
	struct module_config *cfg = &md->cfg;
	size_t bs = cfg->size;
	struct tester_module_data *cd = NULL;
	int ret = 0;

	if (bs != sizeof(struct tester_init_config)) {
		comp_err(dev, "Invalid config");
		return -EINVAL;
	}

	/* allocate ctx for test module in shared memory - to allow all non-standard operations
	 * without problems with cache
	 */
	cd = rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*cd));
	if (!cd) {
		comp_err(dev, "Out of memory");
		return -ENOMEM;
	}

	struct tester_init_config *init_cfg = (struct tester_init_config *)cfg->init_data;

	cd->test_case_type = init_cfg->test_type;
	switch (cd->test_case_type) {
	case TESTER_MODULE_CASE_DUMMY_TEST:
		cd->tester_case_interface = &tester_interface_dummy_test;
		break;

	default:
		comp_err(dev, "Invalid config, unknown test type");
		rfree(cd);
		return -EINVAL;
	}

	module_set_private_data(mod, cd);

	if (cd->tester_case_interface->init)
		ret = cd->tester_case_interface->init(mod, &cd->test_case_ctx);

	if (ret) {
		module_set_private_data(mod, NULL);
		rfree(cd);
	}

	return ret;
}

static int tester_prepare(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct tester_module_data *cd = module_get_private_data(mod);
	int ret = 0;

	if (cd->tester_case_interface->prepare)
		ret = cd->tester_case_interface->prepare(cd->test_case_ctx, mod,
							 sources, num_of_sources,
							 sinks, num_of_sinks);

	return ret;
}

int tester_set_configuration(struct processing_module *mod,
			     uint32_t config_id,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size)
{
	struct tester_module_data *cd = module_get_private_data(mod);
	int ret = 0;

	if (cd->tester_case_interface->set_configuration)
		ret = cd->tester_case_interface->set_configuration(cd->test_case_ctx, mod,
								   config_id, pos, data_offset_size,
								   fragment, fragment_size,
								   response, response_size);

	return ret;
}

static int tester_process(struct processing_module *mod,
			  struct sof_source **sources, int num_of_sources,
			  struct sof_sink **sinks, int num_of_sinks)
{
	struct tester_module_data *cd = module_get_private_data(mod);
	int ret = 0;
	bool do_copy = true;

	if (cd->tester_case_interface->process)
		ret = cd->tester_case_interface->process(cd->test_case_ctx, mod,
							 sources, num_of_sources,
							 sinks, num_of_sinks, &do_copy);

	if (!ret) {
		size_t sink_free = sink_get_free_size(sinks[0]);
		size_t source_avail = source_get_data_available(sources[0]);
		size_t to_copy = MIN(sink_free, source_avail);

		if (do_copy) {
			/* copy data from input to output */
			source_to_sink_copy(sources[0], sinks[0], true, to_copy);
		} else {
			/* drop data and generate silence */
			source_drop_data(sources[0], to_copy);
			sink_fill_with_silence(sinks[0], to_copy);
		}
	}

	return ret;
}

static int tester_reset(struct processing_module *mod)
{
	struct tester_module_data *cd = module_get_private_data(mod);
	int ret = 0;

	if (cd->tester_case_interface->reset)
		ret = cd->tester_case_interface->reset(cd->test_case_ctx, mod);

	return ret;
}

static int tester_free(struct processing_module *mod)
{
	struct tester_module_data *cd = module_get_private_data(mod);
	int ret = 0;

	if (cd->tester_case_interface->free)
		ret = cd->tester_case_interface->free(cd->test_case_ctx, mod);

	rfree(cd);
	module_set_private_data(mod, NULL);
	return ret;
}

static int tester_bind(struct processing_module *mod, void *data)
{
	struct tester_module_data *cd = module_get_private_data(mod);
	int ret = 0;

	if (cd->tester_case_interface->bind)
		ret = cd->tester_case_interface->bind(cd->test_case_ctx, mod, data);

	return ret;
}

static int tester_unbind(struct processing_module *mod, void *data)
{
	struct tester_module_data *cd = module_get_private_data(mod);
	int ret = 0;

	if (cd->tester_case_interface->unbind)
		ret = cd->tester_case_interface->unbind(cd->test_case_ctx, mod, data);

	return ret;
}

static int tester_trigger(struct processing_module *mod, int cmd)
{
	struct tester_module_data *cd = module_get_private_data(mod);
	int ret = 0;

	if (cd->tester_case_interface->trigger)
		ret = cd->tester_case_interface->trigger(cd->test_case_ctx, mod, cmd);

	if (!ret)
		ret = module_adapter_set_state(mod, mod->dev, cmd);

	return ret;
}

static const struct module_interface tester_interface = {
	.init = tester_init,
	.prepare = tester_prepare,
	.set_configuration = tester_set_configuration,
	.process = tester_process,
	.reset = tester_reset,
	.free = tester_free,
	.bind = tester_bind,
	.unbind = tester_unbind,
	.trigger = tester_trigger
};

DECLARE_MODULE_ADAPTER(tester_interface, tester_uuid, tester_tr);
SOF_MODULE_INIT(tester, sys_comp_module_tester_interface_init);

#if CONFIG_COMP_TESTER_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

SOF_LLEXT_MOD_ENTRY(tester, &tester_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("TESTER", tester_llext_entry, 1, SOF_REG_UUID(tester), 40);

SOF_LLEXT_BUILDINFO;

#endif
