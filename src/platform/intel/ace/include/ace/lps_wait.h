/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Marcin Maka <marcin.maka@linux.intel.com>
 */

#ifndef __ACE_LPS_WAIT_H__
#define __ACE_LPS_WAIT_H__

extern void *lps_pic_restore_vector;
extern void *lps_pic_restore_vector_end;
extern void *lps_pic_restore_vector_literals;

void lps_wait_for_interrupt(int level);

#endif /*__ACE_LPS_WAIT_H__ */
