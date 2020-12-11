/*
 * File: drc_db2mag.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "drc_db2mag.h"
#include <string.h>

/* Function Declarations */
static void MultiWordUnsignedWrap(const uint32_T u1[], int32_T n1, uint32_T n2,
  uint32_T y[]);
static int32_T mul_s32_sat(int32_T a, int32_T b);
static void mul_wide_s32(int32_T in0, int32_T in1, uint32_T *ptrOutBitsHi,
  uint32_T *ptrOutBitsLo);
static int32_T uMultiWord2sLongSat(const uint32_T u1[], int32_T n1);
static void uMultiWord2sMultiWordSat(const uint32_T u1[], int32_T n1, uint32_T
  y[], int32_T n);
static void uMultiWordMul(const uint32_T u1[], int32_T n1, const uint32_T u2[],
  int32_T n2, uint32_T y[], int32_T n);
static void uMultiWordShr(const uint32_T u1[], int32_T n1, uint32_T n2, uint32_T
  y[], int32_T n);

/* Function Definitions */

/*
 * Arguments    : const uint32_T u1[]
 *                int32_T n1
 *                uint32_T n2
 *                uint32_T y[]
 * Return Type  : void
 */
static void MultiWordUnsignedWrap(const uint32_T u1[], int32_T n1, uint32_T n2,
  uint32_T y[])
{
  int32_T n1m1;
  n1m1 = n1 - 1;
  if (0 <= (n1m1 - 1))
  {
    memcpy(&y[0], &u1[0], ((uint32_T)n1m1) * (sizeof(uint32_T)));
  }

  y[n1m1] = u1[n1m1] & ((1U << (32U - n2)) - 1U);
}

/*
 * Arguments    : int32_T a
 *                int32_T b
 * Return Type  : int32_T
 */
static int32_T mul_s32_sat(int32_T a, int32_T b)
{
  int32_T result;
  uint32_T u32_chi;
  uint32_T u32_clo;
  mul_wide_s32(a, b, &u32_chi, &u32_clo);
  if ((((int32_T)u32_chi) > 0) || ((u32_chi == 0U) && (u32_clo >= 2147483648U)))
  {
    result = MAX_int32_T;
  }
  else if ((((int32_T)u32_chi) < -1) || ((((int32_T)u32_chi) == -1) && (u32_clo <
             2147483648U)))
  {
    result = MIN_int32_T;
  }
  else
  {
    result = (int32_T)u32_clo;
  }

  return result;
}

/*
 * Arguments    : int32_T in0
 *                int32_T in1
 *                uint32_T *ptrOutBitsHi
 *                uint32_T *ptrOutBitsLo
 * Return Type  : void
 */
static void mul_wide_s32(int32_T in0, int32_T in1, uint32_T *ptrOutBitsHi,
  uint32_T *ptrOutBitsLo)
{
  uint32_T absIn0;
  uint32_T absIn1;
  int32_T in0Hi;
  int32_T in0Lo;
  int32_T in1Hi;
  int32_T in1Lo;
  uint32_T productLoLo;
  uint32_T outBitsLo;
  if (in0 < 0)
  {
    absIn0 = (~((uint32_T)in0)) + 1U;
  }
  else
  {
    absIn0 = (uint32_T)in0;
  }

  if (in1 < 0)
  {
    absIn1 = (~((uint32_T)in1)) + 1U;
  }
  else
  {
    absIn1 = (uint32_T)in1;
  }

  in0Hi = (int32_T)((uint32_T)(absIn0 >> 16U));
  in0Lo = (int32_T)((uint32_T)(absIn0 & 65535U));
  in1Hi = (int32_T)((uint32_T)(absIn1 >> 16U));
  in1Lo = (int32_T)((uint32_T)(absIn1 & 65535U));
  absIn0 = ((uint32_T)in0Hi) * ((uint32_T)in1Lo);
  absIn1 = ((uint32_T)in0Lo) * ((uint32_T)in1Hi);
  productLoLo = ((uint32_T)in0Lo) * ((uint32_T)in1Lo);
  in0Lo = 0;
  outBitsLo = productLoLo + (absIn1 << 16U);
  if (outBitsLo < productLoLo)
  {
    in0Lo = 1;
  }

  productLoLo = outBitsLo;
  outBitsLo += (absIn0 << 16U);
  if (outBitsLo < productLoLo)
  {
    in0Lo = (int32_T)((uint32_T)(((uint32_T)in0Lo) + 1U));
  }

  absIn0 = ((((uint32_T)in0Lo) + (((uint32_T)in0Hi) * ((uint32_T)in1Hi))) +
            (absIn1 >> 16U)) + (absIn0 >> 16U);
  if ((in0 != 0) && ((in1 != 0) && ((in0 > 0) != (in1 > 0))))
  {
    absIn0 = ~absIn0;
    outBitsLo = ~outBitsLo;
    outBitsLo++;
    if (outBitsLo == 0U)
    {
      absIn0++;
    }
  }

  *ptrOutBitsHi = absIn0;
  *ptrOutBitsLo = outBitsLo;
}

/*
 * Arguments    : const uint32_T u1[]
 *                int32_T n1
 * Return Type  : int32_T
 */
static int32_T uMultiWord2sLongSat(const uint32_T u1[], int32_T n1)
{
  uint32_T y;
  uMultiWord2sMultiWordSat(u1, n1, (uint32_T *)(&y), 1);
  return (int32_T)y;
}

/*
 * Arguments    : const uint32_T u1[]
 *                int32_T n1
 *                uint32_T y[]
 *                int32_T n
 * Return Type  : void
 */
static void uMultiWord2sMultiWordSat(const uint32_T u1[], int32_T n1, uint32_T
  y[], int32_T n)
{
  int32_T nm1;
  boolean_T doSaturation = false;
  int32_T i;
  nm1 = n - 1;
  if (n1 >= n)
  {
    doSaturation = ((u1[nm1] & 2147483648U) != 0U);
    i = n1 - 1;
    while ((!doSaturation) && (i >= n))
    {
      doSaturation = (u1[i] != 0U);
      i--;
    }
  }

  if (doSaturation)
  {
    for (i = 0; i < nm1; i++)
    {
      y[i] = MAX_uint32_T;
    }

    y[i] = 2147483647U;
  }
  else
  {
    if (n1 < n)
    {
      nm1 = n1;
    }
    else
    {
      nm1 = n;
    }

    if (0 <= (nm1 - 1))
    {
      memcpy(&y[0], &u1[0], ((uint32_T)nm1) * (sizeof(uint32_T)));
    }

    for (i = 0; i < nm1; i++)
    {
    }

    while (i < n)
    {
      y[i] = 0U;
      i++;
    }
  }
}

/*
 * Arguments    : const uint32_T u1[]
 *                int32_T n1
 *                const uint32_T u2[]
 *                int32_T n2
 *                uint32_T y[]
 *                int32_T n
 * Return Type  : void
 */
static void uMultiWordMul(const uint32_T u1[], int32_T n1, const uint32_T u2[],
  int32_T n2, uint32_T y[], int32_T n)
{
  int32_T i;
  uint32_T cb;
  uint32_T u1i;
  int32_T a1;
  int32_T a0;
  int32_T ni;
  int32_T k;
  int32_T j;
  int32_T b1;
  int32_T b0;
  uint32_T w01;
  uint32_T yk;
  uint32_T t;

  /* Initialize output to zero */
  if (0 <= (n - 1))
  {
    memset(&y[0], 0, ((uint32_T)n) * (sizeof(uint32_T)));
  }

  for (i = 0; i < n1; i++)
  {
    cb = 0U;
    u1i = u1[i];
    a1 = (int32_T)((uint32_T)(u1i >> 16U));
    a0 = (int32_T)((uint32_T)(u1i & 65535U));
    ni = n - i;
    if (n2 <= ni)
    {
      ni = n2;
    }

    k = i;
    for (j = 0; j < ni; j++)
    {
      u1i = u2[j];
      b1 = (int32_T)((uint32_T)(u1i >> 16U));
      b0 = (int32_T)((uint32_T)(u1i & 65535U));
      u1i = ((uint32_T)a1) * ((uint32_T)b0);
      w01 = ((uint32_T)a0) * ((uint32_T)b1);
      yk = y[k] + cb;
      cb = (uint32_T)((yk < cb) ? 1 : 0);
      t = ((uint32_T)a0) * ((uint32_T)b0);
      yk += t;
      cb += (uint32_T)((yk < t) ? 1 : 0);
      t = (u1i << 16U);
      yk += t;
      cb += (uint32_T)((yk < t) ? 1 : 0);
      t = (w01 << 16U);
      yk += t;
      cb += (uint32_T)((yk < t) ? 1 : 0);
      y[k] = yk;
      cb += (u1i >> 16U);
      cb += (w01 >> 16U);
      cb += ((uint32_T)a1) * ((uint32_T)b1);
      k++;
    }

    if (k < n)
    {
      y[k] = cb;
    }
  }
}

/*
 * Arguments    : const uint32_T u1[]
 *                int32_T n1
 *                uint32_T n2
 *                uint32_T y[]
 *                int32_T n
 * Return Type  : void
 */
static void uMultiWordShr(const uint32_T u1[], int32_T n1, uint32_T n2, uint32_T
  y[], int32_T n)
{
  int32_T nb;
  int32_T i;
  int32_T nc;
  uint32_T nr;
  int32_T i1;
  uint32_T nl;
  uint32_T u1i;
  uint32_T yi;
  nb = ((int32_T)n2) / 32;
  i = 0;
  if (nb < n1)
  {
    nc = n + nb;
    if (nc > n1)
    {
      nc = n1;
    }

    nr = n2 - (((uint32_T)nb) * 32U);
    if (nr > 0U)
    {
      nl = 32U - nr;
      u1i = u1[nb];
      for (i1 = nb + 1; i1 < nc; i1++)
      {
        yi = (u1i >> nr);
        u1i = u1[i1];
        y[i] = yi | (u1i << nl);
        i++;
      }

      yi = (u1i >> nr);
      if (nc < n1)
      {
        yi |= (u1[nc] << nl);
      }

      y[i] = yi;
      i++;
    }
    else
    {
      for (i1 = nb; i1 < nc; i1++)
      {
        y[i] = u1[i1];
        i++;
      }
    }
  }

  while (i < n)
  {
    y[i] = 0U;
    i++;
  }
}

/*
 * function y = drc_db2mag_fixpt(tstruct)
 * y = 10.^(ydb/20);     % db2mag = 10.^(21/20)
 * Arguments    : const struct0_T *tstruct
 * Return Type  : uint32_T
 */
uint32_T drc_db2mag_fixpt(const struct0_T *tstruct)
{
  uint32_T y;
  int32_T a;
  int32_T varargin_1;
  uint32_T bu;
  uint32_T u;
  uint64m_T r;
  uint64m_T r1;
  uint64m_T r2;
  int32_T exitg1;

  a = (int32_T)tstruct->u1;

  varargin_1 = 1;
  bu = (uint32_T)tstruct->u2;
  u = 3435973837U;
  uMultiWordMul((uint32_T *)(&bu), 1, (uint32_T *)(&u), 1, (uint32_T *)
                (&r.chunks[0U]), 2);
  uMultiWordShr((uint32_T *)(&r.chunks[0U]), 2, 36U, (uint32_T *)(&r1.chunks[0U]),
                2);
  MultiWordUnsignedWrap((uint32_T *)(&r1.chunks[0U]), 2, 27U, (uint32_T *)
                        (&r2.chunks[0U]));
  bu = (uint32_T)uMultiWord2sLongSat((uint32_T *)(&r2.chunks[0U]), 2);
  do
  {
    exitg1 = 0;
    if ((bu & 1U) != 0U)
    {
      varargin_1 = mul_s32_sat(a, varargin_1);
    }

    bu >>= 1U;
    if (((int32_T)bu) == 0)
    {
      exitg1 = 1;
    }
    else
    {
      a = mul_s32_sat(a, a);
    }
  }
  while (exitg1 == 0);

  y = (((uint32_T)varargin_1) << ((uint32_T)28));

  /*  =power(10,(21/20)) */
  return y;
}



/*
 * function [tstruct] = init_struc_fixpt
 * Arguments    : struct0_T *tstruct
 * Return Type  : void
 */
void init_struc_fixpt(struct0_T *tstruct)
{
  tstruct->u1 = 10U;

  tstruct->u2 = 21U;

  /*  raise pow should be positive */
  /*  tstruct.u2(find(tstruct.u2>=2^7)) = -2^8 + tstruct.u2(find(tstruct.u2>=2^7)); */
  /*  size(tstruct.u1) */
  /*  size(tstruct.u2) */
}

/*
 * File trailer for drc_db2mag.c
 *
 * [EOF]
 */
