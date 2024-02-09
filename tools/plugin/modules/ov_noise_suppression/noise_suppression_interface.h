/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation. All rights reserved.
 *
 * Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 *
 */

#ifndef _NOISE_SUPPRESSION_INTERFACE_H
#define _NOISE_SUPPRESSION_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif
	typedef void *ns_handle;
	int ov_ns_init(ns_handle *handle);
	void ov_ns_free(ns_handle handle);
	int ov_ns_process(ns_handle handle,
			  struct input_stream_buffer *input_buffers, int num_input_buffers,
			  struct output_stream_buffer *output_buffers, int num_output_buffers);

#ifdef __cplusplus
}
#endif

#endif /*_NOISE_SUPPRESSION_INTERFACE_H */
