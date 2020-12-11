/*
 * File: drc_sin.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef DRC_SIN_H
#define DRC_SIN_H

/* Include Files */
#include <stddef.h>
#include <stdlib.h>
#include "rtwtypes.h"
#include "drc_sin_types.h"

/* Function Declarations */
extern void drc_sin_fixpt(const signed char x[2], int y[2]);
extern void drc_sin_initialize(void);
extern void drc_sin_terminate(void);
extern void init_struc_fixpt(signed char x[2]);

#endif

/*
 * File trailer for drc_sin.h
 *
 * [EOF]
 */
