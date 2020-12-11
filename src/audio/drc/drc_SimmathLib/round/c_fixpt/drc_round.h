/*
 * File: drc_round.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef DRC_ROUND_H
#define DRC_ROUND_H

/* Include Files */
#include <stddef.h>
#include <stdlib.h>
#include "rtwtypes.h"
#include "drc_round_types.h"

/* Function Declarations */
extern unsigned char drc_round_fixpt(unsigned int x);
extern unsigned int init_struc_fixpt(void);

#endif

/*
 * File trailer for drc_round.h
 *
 * [EOF]
 */
