/*
 * File: drc_db2mag.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef DRC_DB2MAG_H
#define DRC_DB2MAG_H

/* Include Files */
#include <stddef.h>
#include <stdlib.h>
#include "rtwtypes.h"
#include "drc_db2mag_types.h"

/* Function Declarations */
extern uint32_T drc_db2mag_fixpt(const struct0_T *tstruct);
extern void init_struc_fixpt(struct0_T *tstruct);

#endif

/*
 * File trailer for drc_db2mag.h
 *
 * [EOF]
 */
