/*
 * Copyright (c) 2016-2017 by Cadence Design Systems, Inc.  ALL RIGHTS RESERVED.
 * These coded instructions, statements, and computer programs are the
 * copyrighted works and confidential proprietary information of
 * Cadence Design Systems Inc.  They may be adapted and modified by bona fide
 * purchasers for internal use, but neither the original nor any adapted
 * or modified version may be disclosed or distributed to third parties
 * in any manner, medium, or form, in whole or in part, without the prior
 * written consent of Cadence Design Systems Inc.  This software and its
 * derivatives are to be executed solely on products incorporating a Cadence
 * Design Systems processor.
 */


#ifndef  __XA_TYPE_DEF_H__
#define  __XA_TYPE_DEF_H__

/****************************************************************************/
/*     types               type define    prefix        examples      bytes */
/************************  ***********    ******    ****************  ***** */
typedef char             WORD8   ;/* b       WORD8    b_name     1   */
typedef char         *   pWORD8  ;/* pb      pWORD8   pb_nmae    1   */
typedef unsigned char           UWORD8  ;/* ub      UWORD8   ub_count   1   */
typedef unsigned char       *   pUWORD8 ;/* pub     pUWORD8  pub_count  1   */

typedef signed short            WORD16  ;/* s       WORD16   s_count    2   */
typedef signed short        *   pWORD16 ;/* ps      pWORD16  ps_count   2   */
typedef unsigned short          UWORD16 ;/* us      UWORD16  us_count   2   */
typedef unsigned short      *   pUWORD16;/* pus     pUWORD16 pus_count  2   */

#if !defined(WORD24_H)
#define WORD24_H
typedef signed long              WORD24  ;/* k       WORD24   k_count    3   */
typedef signed long          *   pWORD24 ;/* pk      pWORD24  pk_count   3   */
typedef unsigned long            UWORD24 ;/* uk      UWORD24  uk_count   3   */
typedef unsigned long        *   pUWORD24;/* puk     pUWORD24 puk_count  3   */
#endif /* WORD24_H */

typedef signed int              WORD32  ;/* i       WORD32   i_count    4   */
typedef signed int          *   pWORD32 ;/* pi      pWORD32  pi_count   4   */
typedef unsigned int            UWORD32 ;/* ui      UWORD32  ui_count   4   */
typedef unsigned int        *   pUWORD32;/* pui     pUWORD32 pui_count  4   */

typedef signed long long        WORD40  ;/* m       WORD40   m_count    5   */
typedef signed long long    *   pWORD40 ;/* pm      pWORD40  pm_count   5   */
typedef unsigned long long      UWORD40 ;/* um      UWORD40  um_count   5   */
typedef unsigned long long  *   pUWORD40;/* pum     pUWORD40 pum_count  5   */

typedef signed long long        WORD64  ;/* h       WORD64   h_count    8   */
typedef signed long long    *   pWORD64 ;/* ph      pWORD64  ph_count   8   */
typedef unsigned long long      UWORD64 ;/* uh      UWORD64  uh_count   8   */
typedef unsigned long long  *   pUWORD64;/* puh     pUWORD64 puh_count  8   */

typedef float                   FLOAT32 ;/* f       FLOAT32  f_count    4   */
typedef float               *   pFLOAT32;/* pf      pFLOAT32 pf_count   4   */
typedef double                  FLOAT64 ;/* d       UFLOAT64 d_count    8   */
typedef double              *   pFlOAT64;/* pd      pFLOAT64 pd_count   8   */

typedef void                    VOID    ;/* v       VOID     v_flag     4   */
typedef void                *   pVOID   ;/* pv      pVOID    pv_flag    4   */

/* variable size types: platform optimized implementation */
typedef signed int              BOOL    ;/* bool    BOOL     bool_true      */
typedef unsigned int            UBOOL   ;/* ubool   BOOL     ubool_true     */
typedef signed int              FLAG    ;/* flag    FLAG     flag_false     */
typedef unsigned int            UFLAG   ;/* uflag   FLAG     uflag_false    */
typedef signed int              LOOPIDX ;/* lp      LOOPIDX  lp_index       */
typedef unsigned int            ULOOPIDX;/* ulp     SLOOPIDX ulp_index      */
typedef signed int              WORD    ;/* lp      LOOPIDX  lp_index       */
typedef unsigned int            UWORD   ;/* ulp     SLOOPIDX ulp_index      */

typedef LOOPIDX                 LOOPINDEX; /* lp    LOOPIDX  lp_index       */
typedef ULOOPIDX                ULOOPINDEX;/* ulp   SLOOPIDX ulp_index      */

#define PLATFORM_INLINE __inline

typedef struct xa_codec_opaque { WORD32 _; } *xa_codec_handle_t;

typedef int XA_ERRORCODE;

typedef XA_ERRORCODE xa_codec_func_t(xa_codec_handle_t p_xa_module_obj,
				     WORD32            i_cmd,
				     WORD32            i_idx,
				     pVOID             pv_value);

#endif /* __XA_TYPE_DEF_H__ */
