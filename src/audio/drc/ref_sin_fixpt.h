/*
 * File: ref_sin_fixpt.h
 *
 */
 // SPDX - License - Identifier: BSD - 3 - Clause
 //
 //Copyright(c) 2021 Intel Corporation.All rights reserved.
 //
 //Author : Shriram Shastry <malladi.sastry@linux.intel.com>

#ifndef REF_SIN_FIXPT_H
#define REF_SIN_FIXPT_H

/* Include Files */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus

extern "C" {

#endif

  /* Function Declarations */
  extern void ref_sine_fixpt(const int32_t thRadFxp[1024], int32_t cdcSinTh
    [1024]);
  extern void init_data_fixpt(int32_t thRadFxp[1024]);


#ifdef __cplusplus

}
#endif
#endif

/*
 * File trailer for ref_sin_fixpt.h
 *
 * [EOF]
 */
