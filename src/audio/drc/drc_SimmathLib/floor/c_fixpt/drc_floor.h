/*
 * File: drc_floor.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef DRC_FLOOR_H
#define DRC_FLOOR_H

/* Include Files */
#include <stddef.h>
#include <stdlib.h>
#include "rtwtypes.h"
#include "drc_floor_types.h"

/* Function Declarations */
extern void drc_floor_fixpt(const uint32_T x[5], uint8_T y[5]);
extern void init_struc_fixpt(uint32_T x[5]);

#endif

/*
 * File trailer for drc_floor.h
 *
 * [EOF]
 */
