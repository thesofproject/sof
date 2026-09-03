/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski	<karolx.trzcinski@linux.intel.com>
 */

#ifndef __LOGGER_FILTER_H__
#define __LOGGER_FILTER_H__

#define FILTER_KERNEL_PATH "/sys/kernel/debug/sof/filter"

int filter_update_firmware(void);

#endif /* __LOGGER_FILTER_H__ */
