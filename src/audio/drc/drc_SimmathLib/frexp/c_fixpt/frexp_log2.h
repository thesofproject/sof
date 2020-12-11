/*
 * File: frexp_log2.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef FREXP_LOG2_H
#define FREXP_LOG2_H

/* Include Files */
#include <stddef.h>
#include <stdlib.h>
#include "rtwtypes.h"
#include "frexp_log2_types.h"

/* Function Declarations */
extern void frexp_log2_fixpt(uint32_T x, uint32_T *F, uint8_T *E);
extern uint32_T init_struc_fixpt(void);

#endif

/*
 * File trailer for frexp_log2.h
 *
 * [EOF]
 */
