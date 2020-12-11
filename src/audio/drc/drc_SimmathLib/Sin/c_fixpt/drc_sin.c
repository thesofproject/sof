/*
 * File: drc_sin.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "drc_sin.h"
#include <string.h>

/* Variable Definitions */
static short FI_SIN_COS_LUT[256];
static boolean_T isInitialized_drc_sin = false;

/* Function Declarations */
static bool MultiWord2Bool(const unsigned int u[], int n);
static int MultiWord2sLong(const unsigned int u[]);
static unsigned int MultiWord2uLong(const unsigned int u[]);
static void MultiWordNeg(const unsigned int u1[], unsigned int y[], int n);
static void MultiWordSetSignedMax(unsigned int y[], int n);
static void MultiWordSetSignedMin(unsigned int y[], int n);
static void MultiWordSignedWrap(const unsigned int u1[], int n1, unsigned int n2,
  unsigned int y[]);
static void MultiWordSub(const unsigned int u1[], const unsigned int u2[],
  unsigned int y[], int n);
static int asr_s32(int u, unsigned int n);
static int mul_ssu32_loSR(int a, unsigned int b, unsigned int aShift);
static unsigned int mul_u32_loSR(unsigned int a, unsigned int b, unsigned int
  aShift);
static void mul_wide_su32(int in0, unsigned int in1, unsigned int *ptrOutBitsHi,
  unsigned int *ptrOutBitsLo);
static void mul_wide_u32(unsigned int in0, unsigned int in1, unsigned int
  *ptrOutBitsHi, unsigned int *ptrOutBitsLo);
static void sLong2MultiWord(int u, unsigned int y[], int n);
static void sMultiWord2MultiWord(const unsigned int u1[], int n1, unsigned int
  y[], int n);
static void sMultiWordDivFloor(const unsigned int u1[], int n1, const unsigned
  int u2[], int n2, unsigned int b_y1[], int m1, unsigned int y2[], int m2,
  unsigned int t1[], int l1, unsigned int t2[], int l2);
static void sMultiWordShl(const unsigned int u1[], int n1, unsigned int n2,
  unsigned int y[], int n);
static void sMultiWordShr(const unsigned int u1[], int n1, unsigned int n2,
  unsigned int y[], int n);
static void sin_init(void);
static void ssuMultiWordMul(const unsigned int u1[], int n1, const unsigned int
  u2[], int n2, unsigned int y[], int n);
static void uLong2MultiWord(unsigned int u, unsigned int y[], int n);
static int uMultiWordDiv(unsigned int a[], int na, unsigned int b[], int nb,
  unsigned int q[], int nq, unsigned int r[], int nr);
static void uMultiWordInc(unsigned int y[], int n);

/* Function Definitions */

/*
 * Arguments    : const unsigned int u[]
 *                int n
 * Return Type  : bool
 */
static bool MultiWord2Bool(const unsigned int u[], int n)
{
  bool y;
  int i;
  y = false;
  i = 0;
  while ((i < n) && (!y))
  {
    y = ((u[i] != 0U) || y);
    i++;
  }

  return y;
}

/*
 * Arguments    : const unsigned int u[]
 * Return Type  : int
 */
static int MultiWord2sLong(const unsigned int u[])
{
  return (int)u[0];
}

/*
 * Arguments    : const unsigned int u[]
 * Return Type  : unsigned int
 */
static unsigned int MultiWord2uLong(const unsigned int u[])
{
  return u[0];
}

/*
 * Arguments    : const unsigned int u1[]
 *                unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void MultiWordNeg(const unsigned int u1[], unsigned int y[], int n)
{
  int i;
  unsigned int yi;
  int carry = 1;
  for (i = 0; i < n; i++)
  {
    yi = (~u1[i]) + ((unsigned int)carry);
    y[i] = yi;
    carry = (int)((yi < ((unsigned int)carry)) ? 1 : 0);
  }
}

/*
 * Arguments    : unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void MultiWordSetSignedMax(unsigned int y[], int n)
{
  int n1;
  int i;
  n1 = n - 1;
  for (i = 0; i < n1; i++)
  {
    y[i] = MAX_uint32_T;
  }

  y[n1] = 2147483647U;
}

/*
 * Arguments    : unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void MultiWordSetSignedMin(unsigned int y[], int n)
{
  int n1;
  n1 = n - 1;
  if (0 <= (n1 - 1))
  {
    memset(&y[0], 0, ((unsigned int)n1) * (sizeof(unsigned int)));
  }

  y[n1] = 2147483648U;
}

/*
 * Arguments    : const unsigned int u1[]
 *                int n1
 *                unsigned int n2
 *                unsigned int y[]
 * Return Type  : void
 */
static void MultiWordSignedWrap(const unsigned int u1[], int n1, unsigned int n2,
  unsigned int y[])
{
  int n1m1;
  unsigned int mask;
  unsigned int ys;
  n1m1 = n1 - 1;
  if (0 <= (n1m1 - 1))
  {
    memcpy(&y[0], &u1[0], ((unsigned int)n1m1) * (sizeof(unsigned int)));
  }

  mask = (1U << (31U - n2));
  if ((u1[n1m1] & mask) != 0U)
  {
    ys = MAX_uint32_T;
  }
  else
  {
    ys = 0U;
  }

  mask = (mask << 1U) - 1U;
  y[n1m1] = (u1[n1m1] & mask) | ((~mask) & ys);
}

/*
 * Arguments    : const unsigned int u1[]
 *                const unsigned int u2[]
 *                unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void MultiWordSub(const unsigned int u1[], const unsigned int u2[],
  unsigned int y[], int n)
{
  int i;
  unsigned int u1i;
  unsigned int yi;
  int borrow = 0;
  for (i = 0; i < n; i++)
  {
    u1i = u1[i];
    yi = (u1i - u2[i]) - ((unsigned int)borrow);
    y[i] = yi;
    if (((unsigned int)borrow) != 0U)
    {
      borrow = (int)((yi >= u1i) ? 1 : 0);
    }
    else
    {
      borrow = (int)((yi > u1i) ? 1 : 0);
    }
  }
}

/*
 * Arguments    : int u
 *                unsigned int n
 * Return Type  : int
 */
static int asr_s32(int u, unsigned int n)
{
  int y;
  if (u >= 0)
  {
    y = (int)((unsigned int)(((unsigned int)u) >> n));
  }
  else
  {
    y = (-((int)((unsigned int)(((unsigned int)((int)(-1 - u))) >> n)))) - 1;
  }

  return y;
}

/*
 * Arguments    : int a
 *                unsigned int b
 *                unsigned int aShift
 * Return Type  : int
 */
static int mul_ssu32_loSR(int a, unsigned int b, unsigned int aShift)
{
  unsigned int u32_chi;
  unsigned int u32_clo;
  mul_wide_su32(a, b, &u32_chi, &u32_clo);
  u32_clo = (u32_chi << (32U - aShift)) | (u32_clo >> aShift);
  return (int)u32_clo;
}

/*
 * Arguments    : unsigned int a
 *                unsigned int b
 *                unsigned int aShift
 * Return Type  : unsigned int
 */
static unsigned int mul_u32_loSR(unsigned int a, unsigned int b, unsigned int
  aShift)
{
  unsigned int result;
  unsigned int u32_chi;
  mul_wide_u32(a, b, &u32_chi, &result);
  return (u32_chi << (32U - aShift)) | (result >> aShift);
}

/*
 * Arguments    : int in0
 *                unsigned int in1
 *                unsigned int *ptrOutBitsHi
 *                unsigned int *ptrOutBitsLo
 * Return Type  : void
 */
static void mul_wide_su32(int in0, unsigned int in1, unsigned int *ptrOutBitsHi,
  unsigned int *ptrOutBitsLo)
{
  unsigned int absIn0;
  int in0Hi;
  int in0Lo;
  int in1Hi;
  int in1Lo;
  unsigned int productLoHi;
  unsigned int productLoLo;
  unsigned int outBitsLo;
  if (in0 < 0)
  {
    absIn0 = (~((unsigned int)in0)) + 1U;
  }
  else
  {
    absIn0 = (unsigned int)in0;
  }

  in0Hi = (int)((unsigned int)(absIn0 >> 16U));
  in0Lo = (int)((unsigned int)(absIn0 & 65535U));
  in1Hi = (int)((unsigned int)(in1 >> 16U));
  in1Lo = (int)((unsigned int)(in1 & 65535U));
  absIn0 = ((unsigned int)in0Hi) * ((unsigned int)in1Lo);
  productLoHi = ((unsigned int)in0Lo) * ((unsigned int)in1Hi);
  productLoLo = ((unsigned int)in0Lo) * ((unsigned int)in1Lo);
  in0Lo = 0;
  outBitsLo = productLoLo + (productLoHi << 16U);
  if (outBitsLo < productLoLo)
  {
    in0Lo = 1;
  }

  productLoLo = outBitsLo;
  outBitsLo += (absIn0 << 16U);
  if (outBitsLo < productLoLo)
  {
    in0Lo = (int)((unsigned int)(((unsigned int)in0Lo) + 1U));
  }

  absIn0 = ((((unsigned int)in0Lo) + (((unsigned int)in0Hi) * ((unsigned int)
    in1Hi))) + (productLoHi >> 16U)) + (absIn0 >> 16U);
  if ((in1 != 0U) && (in0 < 0))
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
 * Arguments    : unsigned int in0
 *                unsigned int in1
 *                unsigned int *ptrOutBitsHi
 *                unsigned int *ptrOutBitsLo
 * Return Type  : void
 */
static void mul_wide_u32(unsigned int in0, unsigned int in1, unsigned int
  *ptrOutBitsHi, unsigned int *ptrOutBitsLo)
{
  int in0Hi;
  int in0Lo;
  int in1Hi;
  int in1Lo;
  unsigned int productHiLo;
  unsigned int productLoHi;
  unsigned int productLoLo;
  unsigned int outBitsLo;
  in0Hi = (int)((unsigned int)(in0 >> 16U));
  in0Lo = (int)((unsigned int)(in0 & 65535U));
  in1Hi = (int)((unsigned int)(in1 >> 16U));
  in1Lo = (int)((unsigned int)(in1 & 65535U));
  productHiLo = ((unsigned int)in0Hi) * ((unsigned int)in1Lo);
  productLoHi = ((unsigned int)in0Lo) * ((unsigned int)in1Hi);
  productLoLo = ((unsigned int)in0Lo) * ((unsigned int)in1Lo);
  in0Lo = 0;
  outBitsLo = productLoLo + (productLoHi << 16U);
  if (outBitsLo < productLoLo)
  {
    in0Lo = 1;
  }

  productLoLo = outBitsLo;
  outBitsLo += (productHiLo << 16U);
  if (outBitsLo < productLoLo)
  {
    in0Lo = (int)((unsigned int)(((unsigned int)in0Lo) + 1U));
  }

  *ptrOutBitsHi = ((((unsigned int)in0Lo) + (((unsigned int)in0Hi) * ((unsigned
    int)in1Hi))) + (productLoHi >> 16U)) + (productHiLo >> 16U);
  *ptrOutBitsLo = outBitsLo;
}

/*
 * Arguments    : int u
 *                unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void sLong2MultiWord(int u, unsigned int y[], int n)
{
  unsigned int yi;
  int i;
  y[0] = (unsigned int)u;
  if (u < 0)
  {
    yi = MAX_uint32_T;
  }
  else
  {
    yi = 0U;
  }

  for (i = 1; i < n; i++)
  {
    y[i] = yi;
  }
}

/*
 * Arguments    : const unsigned int u1[]
 *                int n1
 *                unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void sMultiWord2MultiWord(const unsigned int u1[], int n1, unsigned int
  y[], int n)
{
  int nm;
  unsigned int u1i;
  int i;
  if (n1 < n)
  {
    nm = n1;
  }
  else
  {
    nm = n;
  }

  if (0 <= (nm - 1))
  {
    memcpy(&y[0], &u1[0], ((unsigned int)nm) * (sizeof(unsigned int)));
  }

  if (n > n1)
  {
    if ((u1[n1 - 1] & 2147483648U) != 0U)
    {
      u1i = MAX_uint32_T;
    }
    else
    {
      u1i = 0U;
    }

    for (i = nm; i < n; i++)
    {
      y[i] = u1i;
    }
  }
}

/*
 * Arguments    : const unsigned int u1[]
 *                int n1
 *                const unsigned int u2[]
 *                int n2
 *                unsigned int b_y1[]
 *                int m1
 *                unsigned int y2[]
 *                int m2
 *                unsigned int t1[]
 *                int l1
 *                unsigned int t2[]
 *                int l2
 * Return Type  : void
 */
static void sMultiWordDivFloor(const unsigned int u1[], int n1, const unsigned
  int u2[], int n2, unsigned int b_y1[], int m1, unsigned int y2[], int m2,
  unsigned int t1[], int l1, unsigned int t2[], int l2)
{
  bool numNeg;
  bool denNeg;
  numNeg = ((u1[n1 - 1] & 2147483648U) != 0U);
  denNeg = ((u2[n2 - 1] & 2147483648U) != 0U);
  if (numNeg)
  {
    MultiWordNeg(u1, t1, n1);
  }
  else
  {
    sMultiWord2MultiWord(u1, n1, t1, l1);
  }

  if (denNeg)
  {
    MultiWordNeg(u2, t2, n2);
  }
  else
  {
    sMultiWord2MultiWord(u2, n2, t2, l2);
  }

  if (uMultiWordDiv(t1, l1, t2, l2, b_y1, m1, y2, m2) < 0)
  {
    if (numNeg)
    {
      MultiWordSetSignedMin(b_y1, m1);
    }
    else
    {
      MultiWordSetSignedMax(b_y1, m1);
    }
  }
  else
  {
    if (numNeg != denNeg)
    {
      if (MultiWord2Bool(y2, m2))
      {
        uMultiWordInc(b_y1, m1);
      }

      MultiWordNeg(b_y1, b_y1, m1);
    }
  }
}

/*
 * Arguments    : const unsigned int u1[]
 *                int n1
 *                unsigned int n2
 *                unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void sMultiWordShl(const unsigned int u1[], int n1, unsigned int n2,
  unsigned int y[], int n)
{
  int nb;
  unsigned int ys;
  int nc;
  unsigned int u1i;
  int i;
  unsigned int nl;
  unsigned int nr;
  unsigned int yi;
  nb = ((int)n2) / 32;
  if ((u1[n1 - 1] & 2147483648U) != 0U)
  {
    ys = MAX_uint32_T;
  }
  else
  {
    ys = 0U;
  }

  if (nb > n)
  {
    nc = n;
  }
  else
  {
    nc = nb;
  }

  u1i = 0U;
  if (0 <= (nc - 1))
  {
    memset(&y[0], 0, ((unsigned int)nc) * (sizeof(unsigned int)));
  }

  for (i = 0; i < nc; i++)
  {
  }

  if (nb < n)
  {
    nl = n2 - (((unsigned int)nb) * 32U);
    nb += n1;
    if (nb > n)
    {
      nb = n;
    }

    nb -= i;
    if (nl > 0U)
    {
      nr = 32U - nl;
      for (nc = 0; nc < nb; nc++)
      {
        yi = (u1i >> nr);
        u1i = u1[nc];
        y[i] = yi | (u1i << nl);
        i++;
      }

      if (i < n)
      {
        y[i] = (u1i >> nr) | (ys << nl);
        i++;
      }
    }
    else
    {
      for (nc = 0; nc < nb; nc++)
      {
        y[i] = u1[nc];
        i++;
      }
    }
  }

  while (i < n)
  {
    y[i] = ys;
    i++;
  }
}

/*
 * Arguments    : const unsigned int u1[]
 *                int n1
 *                unsigned int n2
 *                unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void sMultiWordShr(const unsigned int u1[], int n1, unsigned int n2,
  unsigned int y[], int n)
{
  int nb;
  int i;
  unsigned int ys;
  int nc;
  unsigned int nr;
  int i1;
  unsigned int nl;
  unsigned int u1i;
  unsigned int yi;
  nb = ((int)n2) / 32;
  i = 0;
  if ((u1[n1 - 1] & 2147483648U) != 0U)
  {
    ys = MAX_uint32_T;
  }
  else
  {
    ys = 0U;
  }

  if (nb < n1)
  {
    nc = n + nb;
    if (nc > n1)
    {
      nc = n1;
    }

    nr = n2 - (((unsigned int)nb) * 32U);
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

      if (nc < n1)
      {
        yi = u1[nc];
      }
      else
      {
        yi = ys;
      }

      y[i] = (u1i >> nr) | (yi << nl);
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
    y[i] = ys;
    i++;
  }
}

/*
 * Arguments    : void
 * Return Type  : void
 */
static void sin_init(void)
{
  static const short iv[256] =
  {
    0, 804, 1608, 2411, 3212, 4011, 4808, 5602, 6393, 7180, 7962, 8740, 9512,
    10279, 11039, 11793, 12540, 13279, 14010, 14733, 15447, 16151, 16846, 17531,
    18205, 18868, 19520, 20160, 20788, 21403, 22006, 22595, 23170, 23732, 24279,
    24812, 25330, 25833, 26320, 26791, 27246, 27684, 28106, 28511, 28899, 29269,
    29622, 29957, 30274, 30572, 30853, 31114, 31357, 31581, 31786, 31972, 32138,
    32286, 32413, 32522, 32610, 32679, 32729, 32758, MAX_int16_T, 32758, 32729,
    32679, 32610, 32522, 32413, 32286, 32138, 31972, 31786, 31581, 31357, 31114,
    30853, 30572, 30274, 29957, 29622, 29269, 28899, 28511, 28106, 27684, 27246,
    26791, 26320, 25833, 25330, 24812, 24279, 23732, 23170, 22595, 22006, 21403,
    20788, 20160, 19520, 18868, 18205, 17531, 16846, 16151, 15447, 14733, 14010,
    13279, 12540, 11793, 11039, 10279, 9512, 8740, 7962, 7180, 6393, 5602, 4808,
    4011, 3212, 2411, 1608, 804, 0, -804, -1608, -2411, -3212, -4011, -4808,
    -5602, -6393, -7180, -7962, -8740, -9512, -10279, -11039, -11793, -12540,
    -13279, -14010, -14733, -15447, -16151, -16846, -17531, -18205, -18868,
    -19520, -20160, -20788, -21403, -22006, -22595, -23170, -23732, -24279,
    -24812, -25330, -25833, -26320, -26791, -27246, -27684, -28106, -28511,
    -28899, -29269, -29622, -29957, -30274, -30572, -30853, -31114, -31357,
    -31581, -31786, -31972, -32138, -32286, -32413, -32522, -32610, -32679,
    -32729, -32758, -32767, -32758, -32729, -32679, -32610, -32522, -32413,
    -32286, -32138, -31972, -31786, -31581, -31357, -31114, -30853, -30572,
    -30274, -29957, -29622, -29269, -28899, -28511, -28106, -27684, -27246,
    -26791, -26320, -25833, -25330, -24812, -24279, -23732, -23170, -22595,
    -22006, -21403, -20788, -20160, -19520, -18868, -18205, -17531, -16846,
    -16151, -15447, -14733, -14010, -13279, -12540, -11793, -11039, -10279,
    -9512, -8740, -7962, -7180, -6393, -5602, -4808, -4011, -3212, -2411, -1608,
    -804
  };

  memcpy(&FI_SIN_COS_LUT[0], &iv[0], 256U * (sizeof(short)));
}

/*
 * Arguments    : const unsigned int u1[]
 *                int n1
 *                const unsigned int u2[]
 *                int n2
 *                unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void ssuMultiWordMul(const unsigned int u1[], int n1, const unsigned int
  u2[], int n2, unsigned int y[], int n)
{
  bool isNegative1;
  int cb1;
  int i;
  unsigned int cb;
  unsigned int u1i;
  int k;
  int a1;
  unsigned int yk;
  int a0;
  int ni;
  int j;
  int b1;
  int b0;
  unsigned int w01;
  unsigned int t;
  isNegative1 = ((u1[n1 - 1] & 2147483648U) != 0U);
  cb1 = 1;

  /* Initialize output to zero */
  if (0 <= (n - 1))
  {
    memset(&y[0], 0, ((unsigned int)n) * (sizeof(unsigned int)));
  }

  for (i = 0; i < n1; i++)
  {
    cb = 0U;
    u1i = u1[i];
    if (isNegative1)
    {
      u1i = (~u1i) + ((unsigned int)cb1);
      cb1 = (int)((u1i < ((unsigned int)cb1)) ? 1 : 0);
    }

    a1 = (int)((unsigned int)(u1i >> 16U));
    a0 = (int)((unsigned int)(u1i & 65535U));
    ni = n - i;
    if (n2 <= ni)
    {
      ni = n2;
    }

    k = i;
    for (j = 0; j < ni; j++)
    {
      u1i = u2[j];
      b1 = (int)((unsigned int)(u1i >> 16U));
      b0 = (int)((unsigned int)(u1i & 65535U));
      u1i = ((unsigned int)a1) * ((unsigned int)b0);
      w01 = ((unsigned int)a0) * ((unsigned int)b1);
      yk = y[k] + cb;
      cb = (unsigned int)((yk < cb) ? 1 : 0);
      t = ((unsigned int)a0) * ((unsigned int)b0);
      yk += t;
      cb += (unsigned int)((yk < t) ? 1 : 0);
      t = (u1i << 16U);
      yk += t;
      cb += (unsigned int)((yk < t) ? 1 : 0);
      t = (w01 << 16U);
      yk += t;
      cb += (unsigned int)((yk < t) ? 1 : 0);
      y[k] = yk;
      cb += (u1i >> 16U);
      cb += (w01 >> 16U);
      cb += ((unsigned int)a1) * ((unsigned int)b1);
      k++;
    }

    if (k < n)
    {
      y[k] = cb;
    }
  }

  /* Apply sign */
  if (isNegative1)
  {
    cb = 1U;
    for (k = 0; k < n; k++)
    {
      yk = (~y[k]) + cb;
      y[k] = yk;
      cb = (unsigned int)((yk < cb) ? 1 : 0);
    }
  }
}

/*
 * Arguments    : unsigned int u
 *                unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void uLong2MultiWord(unsigned int u, unsigned int y[], int n)
{
  y[0] = u;
  if (1 <= (n - 1))
  {
    memset(&y[1], 0, ((unsigned int)((int)(n + -1))) * (sizeof(unsigned int)));
  }
}

/*
 * Arguments    : unsigned int a[]
 *                int na
 *                unsigned int b[]
 *                int nb
 *                unsigned int q[]
 *                int nq
 *                unsigned int r[]
 *                int nr
 * Return Type  : int
 */
static int uMultiWordDiv(unsigned int a[], int na, unsigned int b[], int nb,
  unsigned int q[], int nq, unsigned int r[], int nr)
{
  int y;
  int nzb;
  int tpi;
  int nza;
  int nb1;
  int na1;
  unsigned int ak;
  unsigned int kbb;
  unsigned int bk;
  unsigned int t;
  unsigned int nbq;
  unsigned int kba;
  unsigned int nba;
  unsigned int nbb;
  unsigned int mask;
  unsigned int kbs;
  int kb;
  unsigned int tnb;
  int ka;
  nzb = nb;
  tpi = nb - 1;
  while ((nzb > 0) && (b[tpi] == 0U))
  {
    nzb--;
    tpi--;
  }

  if (nzb > 0)
  {
    nza = na;
    if (0 <= (nq - 1))
    {
      memset(&q[0], 0, ((unsigned int)nq) * (sizeof(unsigned int)));
    }

    tpi = na - 1;
    while ((nza > 0) && (a[tpi] == 0U))
    {
      nza--;
      tpi--;
    }

    if ((nza > 0) && (nza >= nzb))
    {
      nb1 = nzb - 1;
      na1 = nza - 1;
      if (0 <= (nr - 1))
      {
        memset(&r[0], 0, ((unsigned int)nr) * (sizeof(unsigned int)));
      }

      /* Quick return if dividend and divisor fit into single word. */
      if (nza == 1)
      {
        ak = a[0];
        bk = b[0];
        nbq = ak / bk;
        q[0] = nbq;
        r[0] = ak - (nbq * bk);
        y = 7;
      }
      else
      {
        /* Remove leading zeros from both, dividend and divisor. */
        kbb = 1U;
        t = (b[nb1] >> 1U);
        while (t != 0U)
        {
          kbb++;
          t >>= 1U;
        }

        kba = 1U;
        t = (a[na1] >> 1U);
        while (t != 0U)
        {
          kba++;
          t >>= 1U;
        }

        /* Quick return if quotient is zero. */
        if ((nza > nzb) || (kba >= kbb))
        {
          nba = (((unsigned int)na1) * 32U) + kba;
          nbb = (((unsigned int)nb1) * 32U) + kbb;

          /* Normalize b. */
          if (kbb != 32U)
          {
            bk = b[nb1];
            kbs = 32U - kbb;
            for (kb = nb1; kb > 0; kb--)
            {
              t = (bk << kbs);
              bk = b[kb - 1];
              t |= (bk >> kbb);
              b[kb] = t;
            }

            b[0] = (bk << kbs);
            mask = ~((1U << kbs) - 1U);
          }
          else
          {
            mask = MAX_uint32_T;
          }

          /* Initialize quotient to zero. */
          tnb = 0U;
          y = 0;

          /* Until exit conditions have been met, do */
          do
          {
            /* Normalize a */
            if (kba != 32U)
            {
              kbs = 32U - kba;
              tnb += kbs;
              ak = a[na1];
              for (ka = na1; ka > 0; ka--)
              {
                t = (ak << kbs);
                ak = a[ka - 1];
                t |= (ak >> kba);
                a[ka] = t;
              }

              a[0] = (ak << kbs);
            }

            /* Compare b against the a. */
            ak = a[na1];
            bk = b[nb1];
            if (nb1 == 0)
            {
              t = mask;
            }
            else
            {
              t = MAX_uint32_T;
            }

            if ((ak & t) == bk)
            {
              tpi = 0;
              ka = na1;
              kb = nb1;
              while ((tpi == 0) && (kb > 0))
              {
                ka--;
                ak = a[ka];
                kb--;
                bk = b[kb];
                if (kb == 0)
                {
                  t = mask;
                }
                else
                {
                  t = MAX_uint32_T;
                }

                if ((ak & t) != bk)
                {
                  if (ak > bk)
                  {
                    tpi = 1;
                  }
                  else
                  {
                    tpi = -1;
                  }
                }
              }
            }
            else if (ak > bk)
            {
              tpi = 1;
            }
            else
            {
              tpi = -1;
            }

            /* If the remainder in a is still greater or equal to b, subtract normalized divisor from a. */
            if ((tpi >= 0) || (nba > nbb))
            {
              nbq = nba - nbb;

              /* If the remainder and the divisor are equal, set remainder to zero. */
              if (tpi == 0)
              {
                ka = na1;
                for (kb = nb1; kb > 0; kb--)
                {
                  a[ka] = 0U;
                  ka--;
                }

                a[ka] -= b[0];
              }
              else
              {
                /* Otherwise, subtract the divisor from the remainder */
                if (tpi < 0)
                {
                  ak = a[na1];
                  kba = 31U;
                  for (ka = na1; ka > 0; ka--)
                  {
                    t = (ak << 1U);
                    ak = a[ka - 1];
                    t |= (ak >> 31U);
                    a[ka] = t;
                  }

                  a[0] = (ak << 1U);
                  tnb++;
                  nbq--;
                }

                tpi = 0;
                ka = na1 - nb1;
                for (kb = 0; kb < nzb; kb++)
                {
                  bk = b[kb];
                  t = a[ka];
                  ak = (t - bk) - ((unsigned int)tpi);
                  if (((unsigned int)tpi) != 0U)
                  {
                    tpi = (int)((ak >= t) ? 1 : 0);
                  }
                  else
                  {
                    tpi = (int)((ak > t) ? 1 : 0);
                  }

                  a[ka] = ak;
                  ka++;
                }
              }

              /* Update the quotient. */
              tpi = ((int)nbq) / 32;
              q[tpi] |= (1U << (nbq - (((unsigned int)tpi) * 32U)));

              /* Remove leading zeros from the remainder and check whether the exit conditions have been met. */
              tpi = na1;
              while ((nza > 0) && (a[tpi] == 0U))
              {
                nza--;
                tpi--;
              }

              if (nza >= nzb)
              {
                na1 = nza - 1;
                kba = 1U;
                t = (a[na1] >> 1U);
                while (t != 0U)
                {
                  kba++;
                  t >>= 1U;
                }

                nba = ((((unsigned int)na1) * 32U) + kba) - tnb;
                if (nba < nbb)
                {
                  y = 2;
                }
              }
              else if (nza == 0)
              {
                y = 1;
              }
              else
              {
                na1 = nza - 1;
                y = 4;
              }
            }
            else
            {
              y = 3;
            }
          }
          while (y == 0);

          /* Return the remainder. */
          if (y == 1)
          {
            r[0] = a[0];
          }
          else
          {
            kb = ((int)tnb) / 32;
            nbq = tnb - (((unsigned int)kb) * 32U);
            if (nbq == 0U)
            {
              ka = kb;
              for (tpi = 0; tpi <= nb1; tpi++)
              {
                r[tpi] = a[ka];
                ka++;
              }
            }
            else
            {
              kbs = 32U - nbq;
              ak = a[kb];
              tpi = 0;
              for (ka = kb + 1; ka <= na1; ka++)
              {
                t = (ak >> nbq);
                ak = a[ka];
                t |= (ak << kbs);
                r[tpi] = t;
                tpi++;
              }

              r[tpi] = (ak >> nbq);
            }
          }

          /* Restore b. */
          if (kbb != 32U)
          {
            bk = b[0];
            kbs = 32U - kbb;
            for (kb = 0; kb < nb1; kb++)
            {
              t = (bk >> kbs);
              bk = b[kb + 1];
              t |= (bk << kbb);
              b[kb] = t;
            }

            b[kb] = (bk >> kbs);
          }
        }
        else
        {
          for (tpi = 0; tpi < nr; tpi++)
          {
            r[tpi] = a[tpi];
          }

          y = 6;
        }
      }
    }
    else
    {
      for (tpi = 0; tpi < nr; tpi++)
      {
        r[tpi] = a[tpi];
      }

      y = 5;
    }
  }
  else
  {
    y = -1;
  }

  return y;
}

/*
 * Arguments    : unsigned int y[]
 *                int n
 * Return Type  : void
 */
static void uMultiWordInc(unsigned int y[], int n)
{
  int i;
  unsigned int yi;
  int carry = 1;
  for (i = 0; i < n; i++)
  {
    yi = y[i] + ((unsigned int)carry);
    y[i] = yi;
    carry = (int)((yi < ((unsigned int)carry)) ? 1 : 0);
  }
}

/*
 * function y = drc_sin_fixpt(x)
 * Arguments    : const signed char x[2]
 *                int y[2]
 * Return Type  : void
 */
void drc_sin_fixpt(const signed char x[2], int y[2])
{
  int96m_T r;
  int96m_T r1;
  int64m_T r2;
  int64m_T r3;
  int96m_T r4;
  int64m_T r5;
  int64m_T r6;
  int64m_T r7;
  int64m_T r8;
  int64m_T r9;
  signed char i;
  unsigned int u;
  unsigned int u1;
  int64m_T r10;
  unsigned short u_idx_0;
  unsigned char slice_temp;
  int y_tmp;
  int64m_T r11;
  int64m_T r12;
  int64m_T r13;
  if (isInitialized_drc_sin == false)
  {
    drc_sin_initialize();
  }

  sLong2MultiWord((int)x[0], (unsigned int *)(&r.chunks[0U]), 3);
  sMultiWordShl((unsigned int *)(&r.chunks[0U]), 3, 58U, (unsigned int *)
                (&r1.chunks[0U]), 3);
  sMultiWord2MultiWord((unsigned int *)(&r1.chunks[0U]), 3, (unsigned int *)
                       (&r2.chunks[0U]), 2);
  uLong2MultiWord(3373259426U, (unsigned int *)(&r3.chunks[0U]), 2);
  sMultiWordDivFloor((unsigned int *)(&r2.chunks[0U]), 2, (unsigned int *)
                     (&r3.chunks[0U]), 2, (unsigned int *)(&r4.chunks[0U]), 3,
                     (unsigned int *)(&r5.chunks[0U]), 2, (unsigned int *)
                     (&r6.chunks[0U]), 2, (unsigned int *)(&r7.chunks[0U]), 2);
  sMultiWord2MultiWord((unsigned int *)(&r4.chunks[0U]), 3, (unsigned int *)
                       (&r8.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r8.chunks[0U]), 2, 31U, (unsigned int *)
                      (&r9.chunks[0U]));
  sMultiWordShr((unsigned int *)(&r9.chunks[0U]), 2, 29U, (unsigned int *)
                (&r8.chunks[0U]), 2);
  i = (signed char)MultiWord2sLong((unsigned int *)(&r8.chunks[0U]));
  sLong2MultiWord(((int)x[0]) * 536870912, (unsigned int *)(&r7.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r7.chunks[0U]), 2, 31U, (unsigned int *)
                      (&r6.chunks[0U]));
  if ((i & 8) != 0)
  {
    u = (unsigned int)((signed char)(i | -8));
  }
  else
  {
    u = (unsigned int)((signed char)(i & 7));
  }

  u1 = 3373259426U;
  ssuMultiWordMul((unsigned int *)(&u), 1, (unsigned int *)(&u1), 1, (unsigned
    int *)(&r10.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r10.chunks[0U]), 2, 31U, (unsigned int *)
                      (&r7.chunks[0U]));
  MultiWordSub((unsigned int *)(&r6.chunks[0U]), (unsigned int *)(&r7.chunks[0U]),
               (unsigned int *)(&r5.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r5.chunks[0U]), 2, 31U, (unsigned int *)
                      (&r3.chunks[0U]));
  sMultiWordShr((unsigned int *)(&r3.chunks[0U]), 2, 16U, (unsigned int *)
                (&r2.chunks[0U]), 2);
  u_idx_0 = (unsigned short)MultiWord2uLong((unsigned int *)(&r2.chunks[0U]));
  u_idx_0 = (unsigned short)(mul_u32_loSR(683563337U, (unsigned int)u_idx_0, 13U)
    >> ((unsigned int)16));
  slice_temp = (unsigned char)(((unsigned int)u_idx_0) >> ((unsigned int)8));
  y_tmp = ((int)FI_SIN_COS_LUT[slice_temp]) * 32768;
  y[0] = ((int)((short)asr_s32(y_tmp + (((int)((short)asr_s32(mul_ssu32_loSR
    ((((int)FI_SIN_COS_LUT[(unsigned char)(((unsigned int)slice_temp) + 1U)]) *
      32768) - y_tmp, (unsigned int)((int)(((int)u_idx_0) & ((unsigned short)255))),
     8U), 15U))) * 32768), 15U))) * 65536;
  sLong2MultiWord((int)x[1], (unsigned int *)(&r.chunks[0U]), 3);
  sMultiWordShl((unsigned int *)(&r.chunks[0U]), 3, 58U, (unsigned int *)
                (&r1.chunks[0U]), 3);
  sMultiWord2MultiWord((unsigned int *)(&r1.chunks[0U]), 3, (unsigned int *)
                       (&r5.chunks[0U]), 2);
  uLong2MultiWord(3373259426U, (unsigned int *)(&r6.chunks[0U]), 2);
  sMultiWordDivFloor((unsigned int *)(&r5.chunks[0U]), 2, (unsigned int *)
                     (&r6.chunks[0U]), 2, (unsigned int *)(&r4.chunks[0U]), 3,
                     (unsigned int *)(&r7.chunks[0U]), 2, (unsigned int *)
                     (&r10.chunks[0U]), 2, (unsigned int *)(&r11.chunks[0U]), 2);
  sMultiWord2MultiWord((unsigned int *)(&r4.chunks[0U]), 3, (unsigned int *)
                       (&r3.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r3.chunks[0U]), 2, 31U, (unsigned int *)
                      (&r12.chunks[0U]));
  sMultiWordShr((unsigned int *)(&r12.chunks[0U]), 2, 29U, (unsigned int *)
                (&r3.chunks[0U]), 2);
  i = (signed char)MultiWord2sLong((unsigned int *)(&r3.chunks[0U]));
  sLong2MultiWord(((int)x[1]) * 536870912, (unsigned int *)(&r11.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r11.chunks[0U]), 2, 31U, (unsigned int *)
                      (&r10.chunks[0U]));
  if ((i & 8) != 0)
  {
    u = (unsigned int)((signed char)(i | -8));
  }
  else
  {
    u = (unsigned int)((signed char)(i & 7));
  }

  ssuMultiWordMul((unsigned int *)(&u), 1, (unsigned int *)(&u1), 1, (unsigned
    int *)(&r13.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r13.chunks[0U]), 2, 31U, (unsigned int *)
                      (&r11.chunks[0U]));
  MultiWordSub((unsigned int *)(&r10.chunks[0U]), (unsigned int *)(&r11.chunks
    [0U]), (unsigned int *)(&r7.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r7.chunks[0U]), 2, 31U, (unsigned int *)
                      (&r6.chunks[0U]));
  sMultiWordShr((unsigned int *)(&r6.chunks[0U]), 2, 16U, (unsigned int *)
                (&r5.chunks[0U]), 2);
  u_idx_0 = (unsigned short)MultiWord2uLong((unsigned int *)(&r5.chunks[0U]));
  u_idx_0 = (unsigned short)(mul_u32_loSR(683563337U, (unsigned int)u_idx_0, 13U)
    >> ((unsigned int)16));
  slice_temp = (unsigned char)(((unsigned int)u_idx_0) >> ((unsigned int)8));
  y_tmp = ((int)FI_SIN_COS_LUT[slice_temp]) * 32768;
  y[1] = ((int)((short)asr_s32(y_tmp + (((int)((short)asr_s32(mul_ssu32_loSR
    ((((int)FI_SIN_COS_LUT[(unsigned char)(((unsigned int)slice_temp) + 1U)]) *
      32768) - y_tmp, (unsigned int)((int)(((int)u_idx_0) & ((unsigned short)255))),
     8U), 15U))) * 32768), 15U))) * 65536;
}

/*
 * Arguments    : void
 * Return Type  : void
 */
void drc_sin_initialize(void)
{
  sin_init();
  isInitialized_drc_sin = true;
}

/*
 * Arguments    : void
 * Return Type  : void
 */
void drc_sin_terminate(void)
{
  /* (no terminate code required) */
  isInitialized_drc_sin = false;
}

/*
 * function x = init_struc_fixpt
 * Arguments    : signed char x[2]
 * Return Type  : void
 */
void init_struc_fixpt(signed char x[2])
{
  if (isInitialized_drc_sin == false)
  {
    drc_sin_initialize();
  }

  x[0] = -1;
  x[1] = 1;

}

/*
 * File trailer for drc_sin.c
 *
 * [EOF]
 */
