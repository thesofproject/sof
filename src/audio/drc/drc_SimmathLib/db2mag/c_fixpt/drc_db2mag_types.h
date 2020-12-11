/*
 * File: drc_db2mag_types.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef DRC_DB2MAG_TYPES_H
#define DRC_DB2MAG_TYPES_H

/* Include Files */
#include "rtwtypes.h"

/* Type Definitions */
#ifndef typedef_struct0_T
#define typedef_struct0_T

typedef struct
{
  uint8_T u1;
  uint8_T u2;
}
struct0_T;

#endif                                 /*typedef_struct0_T*/

#ifndef typedef_uint64m_T
#define typedef_uint64m_T

typedef struct
{
  uint32_T chunks[2];
}
uint64m_T;

#endif                                 /*typedef_uint64m_T*/
#endif

/*
 * File trailer for drc_db2mag_types.h
 *
 * [EOF]
 */
