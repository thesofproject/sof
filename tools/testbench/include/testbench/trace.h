/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Ranjani Sridharan <ranjani.sridharan@linux.intel.com>
 */

#include <stdbool.h>

#ifndef _TESTBENCH_TRACE_H
#define _TESTBENCH_TRACE_H

void tb_enable_trace(unsigned int log_level);

bool tb_check_trace(unsigned int log_level);

#endif /* _TESTBENCH_TRACE_H */
