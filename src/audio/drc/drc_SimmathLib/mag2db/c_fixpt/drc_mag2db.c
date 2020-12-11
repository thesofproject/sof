/*
 * File: drc_mag2db.c
 *
 * Author :Shriram Shastry
 * Copyright (c) 2016, Intel Corporation All rights reserved.
 */

/* Include Files */
#include "drc_mag2db.h"
#include <math.h>
#include <string.h>

/* Type Definitions */
#ifndef typedef_cint64m_T
#define typedef_cint64m_T

typedef struct
{
  int64m_T re;
  int64m_T im;
}
cint64m_T;

#endif                                 /*typedef_cint64m_T*/

/* Function Declarations */
static int MultiWord2sLong(const unsigned int u[]);
static void MultiWordNeg(const unsigned int u1[], unsigned int y[], int n);
static void MultiWordSetSignedMax(unsigned int y[], int n);
static void MultiWordSetSignedMin(unsigned int y[], int n);
static void MultiWordSignedWrap(const unsigned int u1[], int n1, unsigned int n2,
  unsigned int y[]);
static void sLong2MultiWord(int u, unsigned int y[], int n);
static void sMultiWord2MultiWord(const unsigned int u1[], int n1, unsigned int
  y[], int n);
static void sMultiWordDivZero(const unsigned int u1[], int n1, const unsigned
  int u2[], int n2, unsigned int b_y1[], int m1, unsigned int y2[], int m2,
  unsigned int t1[], int l1, unsigned int t2[], int l2);
static void sMultiWordShl(const unsigned int u1[], int n1, unsigned int n2,
  unsigned int y[], int n);
static void ssuMultiWordMul(const unsigned int u1[], int n1, const unsigned int
  u2[], int n2, unsigned int y[], int n);
static int uMultiWordDiv(unsigned int a[], int na, unsigned int b[], int nb,
  unsigned int q[], int nq, unsigned int r[], int nr);

/* Function Definitions */

/*
 * Arguments    : const unsigned int u[]
 * Return Type  : int
 */
static int MultiWord2sLong(const unsigned int u[])
{
  return (int)u[0];
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
static void sMultiWordDivZero(const unsigned int u1[], int n1, const unsigned
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
 * function [ydB] = drc_mag2db_fixpt(tstruct)
 * Arguments    : const struct0_T *tstruct
 *                cint32_T ydB_data[]
 *                int ydB_size[2]
 * Return Type  : void
 */
void drc_mag2db_fixpt(const struct0_T *tstruct, cint32_T ydB_data[], int
                      ydB_size[2])
{
  double u;
  double v;
  int c_re;
  unsigned int b_u;
  unsigned int u1;
  int64m_T ci_data[1];
  cint64m_T a1_data[1];
  int64m_T r;
  int64m_T r1;
  int64m_T r2;
  int96m_T r3;
  int64m_T r4;
  int64m_T r5;
  int64m_T r6;
  int96m_T r7;
  int64m_T r8;
  int96m_T r9;
  int a1_im;

  u = log((double)tstruct->x) * 1.6777216E+7;
  v = fabs(u);
  if (v < 4.503599627370496E+15)
  {
    if (v >= 0.5)
    {
      u = floor(u + 0.5);
    }
    else
    {
      u = 0.0;
    }
  }

  if (u < 2.147483648E+9)
  {
    if (u >= -2.147483648E+9)
    {
      c_re = (int)u;
    }
    else
    {
      c_re = MIN_int32_T;
    }
  }
  else
  {
    c_re = MAX_int32_T;
  }

  b_u = 0U;
  u1 = 20U;
  ssuMultiWordMul((unsigned int *)(&b_u), 1, (unsigned int *)(&u1), 1, (unsigned
    int *)(&ci_data[0].chunks[0U]), 2);
  b_u = (unsigned int)c_re;
  ssuMultiWordMul((unsigned int *)(&b_u), 1, (unsigned int *)(&u1), 1, (unsigned
    int *)(&a1_data[0].re.chunks[0U]), 2);

  /* 'drc_mag2db_fixpt:55' coder.inline( 'always' ); */
  /* 'drc_mag2db_fixpt:56' if isfi( a ) && isfi( b ) && isscalar( b ) */
  /* 'drc_mag2db_fixpt:57' a1 = fi( a, 'RoundMode', 'fix' ); */
  /* 'drc_mag2db_fixpt:58' b1 = fi( b, 'RoundMode', 'fix' ); */
  /* 'drc_mag2db_fixpt:59' c1 = divide( divideType( a1, b1 ), a1, b1 ); */
  r = ci_data[0];
  sMultiWordShl((unsigned int *)(&r.chunks[0U]), 2, 24U, (unsigned int *)
                (&r1.chunks[0U]), 2);
  sLong2MultiWord(38630967, (unsigned int *)(&r2.chunks[0U]), 2);
  sMultiWordDivZero((unsigned int *)(&r1.chunks[0U]), 2, (unsigned int *)
                    (&r2.chunks[0U]), 2, (unsigned int *)(&r3.chunks[0U]), 3,
                    (unsigned int *)(&r4.chunks[0U]), 2, (unsigned int *)
                    (&r5.chunks[0U]), 2, (unsigned int *)(&r6.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r3.chunks[0U]), 3, 27U, (unsigned int *)
                      (&r7.chunks[0U]));

  /* 'drc_mag2db_fixpt:60' c = fi( c1, numerictype( c1 ), fimath( a ) ); */
  ydB_size[0] = 1;
  ydB_size[1] = 1;
  r = a1_data[0].re;
  sMultiWordShl((unsigned int *)(&r.chunks[0U]), 2, 24U, (unsigned int *)
                (&r2.chunks[0U]), 2);
  sLong2MultiWord(38630967, (unsigned int *)(&r4.chunks[0U]), 2);
  sMultiWordDivZero((unsigned int *)(&r2.chunks[0U]), 2, (unsigned int *)
                    (&r4.chunks[0U]), 2, (unsigned int *)(&r3.chunks[0U]), 3,
                    (unsigned int *)(&r5.chunks[0U]), 2, (unsigned int *)
                    (&r6.chunks[0U]), 2, (unsigned int *)(&r8.chunks[0U]), 2);
  MultiWordSignedWrap((unsigned int *)(&r3.chunks[0U]), 3, 27U, (unsigned int *)
                      (&r9.chunks[0U]));
  c_re = MultiWord2sLong((unsigned int *)(&r9.chunks[0U]));
  a1_im = MultiWord2sLong((unsigned int *)(&r7.chunks[0U]));
  ydB_data[0].re = c_re;
  ydB_data[0].im = a1_im;

  /* ydb = 20*log10(y); */
}

/*
 * function [tstruct] = init_struc_fixpt
 * Arguments    : struct0_T *tstruct
 * Return Type  : void
 */
void init_struc_fixpt(struct0_T *tstruct)
{
  tstruct->x = 14U;

  tstruct->ydB = 0U;
}

/*
 * File trailer for drc_mag2db.c
 *
 * [EOF]
 */
