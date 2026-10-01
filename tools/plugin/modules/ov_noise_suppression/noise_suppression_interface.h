/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 *
 */

#ifndef _NOISE_SUPPRESSION_INTERFACE_H
#define _NOISE_SUPPRESSION_INTERFACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
	struct cir_buf_source;
	struct cir_buf_sink;
	typedef void *ns_handle;
	int ov_ns_init(ns_handle *handle);
	void ov_ns_free(ns_handle handle);
	int ov_ns_process(ns_handle handle,
			  struct cir_buf_source *source,
			  struct cir_buf_sink *sink,
			  size_t frame_count);

#ifdef __cplusplus
}
#endif

#endif /*_NOISE_SUPPRESSION_INTERFACE_H */
