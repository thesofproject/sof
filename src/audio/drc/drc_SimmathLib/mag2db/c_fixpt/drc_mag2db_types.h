/*
 * File: drc_mag2db_types.h
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

#ifndef DRC_MAG2DB_TYPES_H
#define DRC_MAG2DB_TYPES_H

/* Include Files */
#include "rtwtypes.h"

/* Type Definitions */
#ifndef typedef_int64m_T
#define typedef_int64m_T

typedef struct
{
  unsigned int chunks[2];
}
int64m_T;

#endif                                 /*typedef_int64m_T*/

#ifndef typedef_int96m_T
#define typedef_int96m_T

typedef struct
{
  unsigned int chunks[3];
}
int96m_T;

#endif                                 /*typedef_int96m_T*/

#ifndef typedef_struct0_T
#define typedef_struct0_T

typedef struct
{
  unsigned char x;
  unsigned char ydB;
}
struct0_T;

#endif                                 /*typedef_struct0_T*/
#endif

/*
 * File trailer for drc_mag2db_types.h
 *
 * [EOF]
 */
