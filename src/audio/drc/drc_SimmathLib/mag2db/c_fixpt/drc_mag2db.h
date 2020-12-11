/*
 * File: drc_mag2db.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef DRC_MAG2DB_H
#define DRC_MAG2DB_H

/* Include Files */
#include <stddef.h>
#include <stdlib.h>
#include "rtwtypes.h"
#include "drc_mag2db_types.h"

/* Function Declarations */
extern void drc_mag2db_fixpt(const struct0_T *tstruct, cint32_T ydB_data[], int
  ydB_size[2]);
extern void init_struc_fixpt(struct0_T *tstruct);

#endif

/*
 * File trailer for drc_mag2db.h
 *
 * [EOF]
 */
