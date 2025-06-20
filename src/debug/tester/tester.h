/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 * Author: Marcin Szkudlinski <marcin.szkudlinski@linux.intel.com>
 */

#ifndef COMP_TESTER
#define COMP_TESTER

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <module/module/interface.h>

#define TESTER_MODULE_CASE_NO_TEST  0
#define TESTER_MODULE_CASE_DUMMY_TEST 1
#define TESTER_MODULE_CASE_SIMPLE_DRAM_TEST 2

/**
 * API of a test case
 * it is mostly a replica of module interface, with some additional parameters and functions
 *
 * all methods are optional
 */
struct tester_test_case_interface {
	/**
	 * test initialization procedure,
	 * called in module init method
	 * It should allocate all required structures and return pointer to context
	 * in ctx param
	 */
	int (*init)(struct processing_module *mod, void **ctx);

	/**
	 * copy of module prepare method, with additional ctx param
	 */
	int (*prepare)(void *ctx, struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks);

	/**
	 * copy of module set_configuration method, with additional ctx param
	 */
	int (*set_configuration)(void *ctx, struct processing_module *mod,
				 uint32_t config_id,
				 enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				 const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				 size_t response_size);

	/**
	 * copy of module process method, with additional
	 *  - ctx param
	 *  - do_copy return flag. If is set to true, the tester base will do copy all samples
	 *    from source to sink.
	 *    May be used to indicate if the test has failed without any messages, just by stopping
	 *    data stream
	 */
	int (*process)(void *ctx, struct processing_module *mod,
		       struct sof_source **sources, int num_of_sources,
		       struct sof_sink **sinks, int num_of_sinks,
		       bool *do_copy);

	/**
	 * copy of module reset method, with additional ctx param
	 */
	int (*reset)(void *ctx, struct processing_module *mod);

	/**
	 * copy of module free method, with additional ctx param
	 * must free all data allocated during test
	 */
	int (*free)(void *ctx, struct processing_module *mod);

	/**
	 * copy of module bind method, with additional ctx param
	 */
	int (*bind)(void *ctx, struct processing_module *mod, struct bind_info *bind_data);

	/**
	 * copy of module unbind method, with additional ctx param
	 */
	int (*unbind)(void *ctx, struct processing_module *mod, struct bind_info *unbind_data);

	/**
	 * copy of module trigger method, with additional ctx param
	 */
	int (*trigger)(void *ctx, struct processing_module *mod, int cmd);

};

struct tester_module_data {
	const struct tester_test_case_interface *tester_case_interface;
	uint32_t test_case_type;
	void *test_case_ctx;
};

#endif /* COMP_TESTER */
