/*
 * File: drc_log.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef DRC_LOG_H
#define DRC_LOG_H

/* Include Files */
#include <stddef.h>
#include <stdlib.h>
#include "rtwtypes.h"
#include "drc_log_types.h"

/* Function Declarations */
extern void drc_log_fixpt(const uint8_T x[10], cuint32_T y[10]);
extern void init_struc_fixpt(uint8_T x[10]);

#endif

/*
 * File trailer for drc_log.h
 *
 * [EOF]
 */
