/*
 * File: typedef.h
 *
 */
 // SPDX - License - Identifier: BSD - 3 - Clause
 //
 //Copyright(c) 2021 Intel Corporation.All rights reserved.
 //
 //Author : Shriram Shastry <malladi.sastry@linux.intel.com>

#ifndef TYPEDEF_H
#define TYPEDEF_H

/* Include Files */
#include "stdint.h"
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus

extern "C" {

#endif


  /*=======================================================================*
   * Fixed width word size data types:                                     *
   *   int8_t, int16_t, int32_t     - signed 8, 16, or 32 bit integers     *
   *   uint8_t, uint16_t, uint32_t  - unsigned 8, 16, or 32 bit integers   *
   *   real32_t, real64_t           - 32 and 64 bit floating point numbers *
   *=======================================================================*/
  typedef signed char int8_t;
  typedef unsigned char uint8_t;
  typedef short int16_t;
  typedef unsigned short uint16_t;
  typedef int int32_t;
  typedef unsigned int uint32_t;
  typedef float real32_t;
  typedef double real64_t;

  /*===========================================================================*
   * Generic type definitions: real_T, time_T, boolean_t, int_T, uint_T,       *
   *                           ulong_T, char_T and byte_T.                     *
   *===========================================================================*/
  typedef double real_t;
  typedef double time_t;
  typedef bool boolean_t;

  /*===========================================================================*
   * Complex number type definitions                                           *
   *===========================================================================*/
#define CREAL_T


  /*=======================================================================*
   * Min and Max:                                                          *
   *   int8_t, int16_t, int32_t     - signed 8, 16, or 32 bit integers     *
   *   uint8_t, uint16_t, uint32_t  - unsigned 8, 16, or 32 bit integers   *
   *=======================================================================*/
#define MAX_int8_T                     ((int8_t)(127))
#define MIN_int8_T                     ((int8_t)(-128))
#define MAX_uint8_T                    ((uint8_t)(255))
#define MIN_uint8_T                    ((uint8_t)(0))
#define MAX_int16_T                    ((int16_t)(32767))
#define MIN_int16_T                    ((int16_t)(-32768))
#define MAX_uint16_T                   ((uint16_t)(65535))
#define MIN_uint16_T                   ((uint16_t)(0))
#define MAX_int32_T                    ((int32_t)(2147483647))
#define MIN_int32_T                    ((int32_t)(-2147483647-1))
#define MAX_uint32_T                   ((uint32_t)(0xFFFFFFFFU))
#define MIN_uint32_T                   ((uint32_t)(0))

  /* Logical type definitions */
#if (!defined(__cplusplus)) && (!defined(__true_false_are_keywords)) && (!defined(__bool_true_false_are_defined))
#ifndef false
#define false                          (0U)
#endif

#ifndef true
#define true                           (1U)
#endif
#endif

#ifdef __cplusplus

}
#endif
#endif

/*
 * File trailer for typedef.h
 *
 * [EOF]
 */
