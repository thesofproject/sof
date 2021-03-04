/*
 * File: ref_sin_fixpt.c
 *
 */
 // SPDX - License - Identifier: BSD - 3 - Clause
 //
 //Copyright(c) 2021 Intel Corporation.All rights reserved.
 //
 //Author : Shriram Shastry <malladi.sastry@linux.intel.com>

/* Include Files */
#include "ref_sin_fixpt.h"
#include <string.h>
#include <stdint.h>
#include "typedef.h"
#include <errno.h>


/* Function Definitions */
/*
 * function [cdcSinTh] = ref_sine_fixpt(thRadFxp)
 * Use Q12.20-bit quantized inputs and number of iterations to 10.
 *  Compare the fixed-point cordicsin function results to the
 *  results of the double-precision sin function.
 * +--------------+---------+----------+----------+---------+
 * |              |thRadFxp |cdcSinTh  |QthRadFxp |QcdcSinTh|
 * +--------------+---------+----------+----------+---------+
 * |    WordLength| 31      |  32      |12.20     | 3.29    |
 * |FractionLength| 20      |  29      |          |         |
 * +--------------+---------+----------+----------+---------+
 *  THD+N = 29*6 = 174
 * Arguments    : int32_t thRadFxp Q12.20
 *                int32_t cdcSinTh Q3.29
 * Return Type  : void
 */
inline int32_t ref_sine_fixpt(int32_t thRadFxp)
{
  static const int32_t inpLUT[30] = { 421657428, 248918915, 131521918, 66762579,
    33510843, 16771758, 8387925, 4194219, 2097141, 1048575, 524288, 262144,
    131072, 65536, 32768, 16384, 8192, 4096, 2048, 1024, 512, 256, 128, 64, 32,
    16, 8, 4, 2, 1 };

  int32_t b_idx;
  int32_t b_yn;
  int32_t idx;
  int32_t thRadFxp;
  int32_t xn;
  int32_t xtmp;
  int32_t ytmp;
  int32_t z;
  int32_t cdcSinTh;
  boolean_t negate;
  

    if (thRadFxp > 1647099) {                               // >20bits
      if (((thRadFxp - 3294199) & 1073741824) != 0) {       // NonZero thRadFxp - 21bits & 30bits 
        z = (thRadFxp - 3294199) | -1073741824;
      } else {
        z = (thRadFxp - 3294199) & 1073741823;
      }

      if (z <= 1647099) {                                   // <20 bits
        thRadFxp = z;
        negate = 1U;
      } else {
        if (((thRadFxp - 6588397) & 1073741824) != 0) {     // thRadFxp - 22bits & 30bits
          thRadFxp = (thRadFxp - 6588397) | -1073741824;    // 
        } else {
          thRadFxp = (thRadFxp - 6588397) & 1073741823;
        }

        negate = 0U;
      }
    } else if (thRadFxp < -1647099) {                         // <20
      if (((thRadFxp + 3294199) & 1073741824) != 0) {         // NonZero thRadFxp - 21bits & 30bits 
        z = (thRadFxp + 3294199) | -1073741824;
      } else {
        z = (thRadFxp + 3294199) & 1073741823;
      }

      if (z >= -1647099) {
        thRadFxp = z;
        negate = 1U;
      } else {
        if (((thRadFxp + 6588397) & 1073741824) != 0) {     // thRadFxp - 22bits & 30bits
          thRadFxp = (thRadFxp + 6588397) | -1073741824;
        } else {
          thRadFxp = (thRadFxp + 6588397) & 1073741823;
        }

        negate = 0U;
      }
    } else {
      negate = 0U;
    }

    thRadFxp <<= 9;     // thRadFxp *= 512
    if ((thRadFxp & 1073741824) != 0) {                     // NonZero 30bits
      z = thRadFxp | -1073741824;
    } else {
      z = thRadFxp & 1073741823;
    }

    b_yn = 0;
    xn = 326016437;                                         // 28 bits
    xtmp = 326016437;
    ytmp = 0;
    for (b_idx = 0; b_idx < 30; b_idx++) {                  // Iterate data with lookup table
      if (z < 0) {                                          // negative data 
        thRadFxp = z + inpLUT[b_idx];
        if ((thRadFxp & 1073741824) != 0) {                 // 30 bits
          z = thRadFxp | -1073741824;                       // subtract
        } else {
          z = thRadFxp & 1073741823;                        // multiply
        }

        thRadFxp = xn + ytmp;
        if ((thRadFxp & 1073741824) != 0) {
          xn = thRadFxp | -1073741824;
        } else {
          xn = thRadFxp & 1073741823;
        }

        thRadFxp = b_yn - xtmp;
        if ((thRadFxp & 1073741824) != 0) {                 // 30 bits
          b_yn = thRadFxp | -1073741824;
        } else {
          b_yn = thRadFxp & 1073741823;
        }
      } else {
        thRadFxp = z - inpLUT[b_idx];                       // positive LookUp table
        if ((thRadFxp & 1073741824) != 0) {
          z = thRadFxp | -1073741824;
        } else {
          z = thRadFxp & 1073741823;
        }

        thRadFxp = xn - ytmp;
        if ((thRadFxp & 1073741824) != 0) {
          xn = thRadFxp | -1073741824;
        } else {
          xn = thRadFxp & 1073741823;
        }

        thRadFxp = b_yn + xtmp;
        if ((thRadFxp & 1073741824) != 0) {
          b_yn = thRadFxp | -1073741824;
        } else {
          b_yn = thRadFxp & 1073741823;
        }
      }

      thRadFxp = xn >> (b_idx + 1);                         // arithmatic right shift
      if ((thRadFxp & 1073741824) != 0) {
        xtmp = thRadFxp | -1073741824;
      } else {
        xtmp = thRadFxp & 1073741823;
      }

      thRadFxp = b_yn >> (b_idx + 1);
      if ((thRadFxp & 1073741824) != 0) {
        ytmp = thRadFxp | -1073741824;
      } else {
        ytmp = thRadFxp & 1073741823;
      }
    }

    if (negate) {
      if ((-b_yn & 1073741824) != 0) {
        cdcSinTh = -b_yn | -1073741824;
      } else {
        cdcSinTh = -b_yn & 1073741823;
      }
    } else {
      cdcSinTh = b_yn;
    }
}

