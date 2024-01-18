/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@intel.com>
 */

/* THIS FILE SHOULD NOT BE INCLUDED DIRECTLY */

#ifdef __SOF_TRACE_PREPROC_PRIVATE_H__
/* Macros defined in this file are only helpers for the macros that are
 * defined in header file containing "namespace"
 *     __SOF_TRACE_PREPROC_PRIVATE_H__ .
 * This combination of #ifdef and #ifndef should sufficently narrow
 * the "include-ability" of this dependent header file.
 * If you wish to use macros from this file directly, be *V E R Y* careful!
 * HIC SUNT DRACONES
 */
#ifndef __SOF_TRACE_PREPROC_PRIVATE_DEC_H__
#define __SOF_TRACE_PREPROC_PRIVATE_DEC_H__

/* The only sane way I found to decrement values in cpreproc */
/* for instance META_DEC(3) will be tokenized to DEC_3
 * and then expanded again to 2
 */
#define _META_DEC_0    0 // notice how we deal with underflow
#define _META_DEC_1    0
#define _META_DEC_2    1
#define _META_DEC_3    2
#define _META_DEC_4    3
#define _META_DEC_5    4
#define _META_DEC_6    5
#define _META_DEC_7    6
#define _META_DEC_8    7
#define _META_DEC_9    8
#define _META_DEC_10   9
#define _META_DEC_11  10
#define _META_DEC_12  11
#define _META_DEC_13  12
#define _META_DEC_14  13
#define _META_DEC_15  14
#define _META_DEC_16  15
#define _META_DEC_17  16
#define _META_DEC_18  17
#define _META_DEC_19  18
#define _META_DEC_20  19
#define _META_DEC_21  20
#define _META_DEC_22  21
#define _META_DEC_23  22
#define _META_DEC_24  23
#define _META_DEC_25  24
#define _META_DEC_26  25
#define _META_DEC_27  26
#define _META_DEC_28  27
#define _META_DEC_29  28
#define _META_DEC_30  29
#define _META_DEC_31  30
#define _META_DEC_32  31
#define _META_DEC_33  32
#define _META_DEC_34  33
#define _META_DEC_35  34
#define _META_DEC_36  35
#define _META_DEC_37  36
#define _META_DEC_38  37
#define _META_DEC_39  38
#define _META_DEC_40  39
#define _META_DEC_41  40
#define _META_DEC_42  41
#define _META_DEC_43  42
#define _META_DEC_44  43
#define _META_DEC_45  44
#define _META_DEC_46  45
#define _META_DEC_47  46
#define _META_DEC_48  47
#define _META_DEC_49  48
#define _META_DEC_50  49
#define _META_DEC_51  50
#define _META_DEC_52  51
#define _META_DEC_53  52
#define _META_DEC_54  53
#define _META_DEC_55  54
#define _META_DEC_56  55
#define _META_DEC_57  56
#define _META_DEC_58  57
#define _META_DEC_59  58
#define _META_DEC_60  59
#define _META_DEC_61  60
#define _META_DEC_62  61
#define _META_DEC_63  62
#define _META_DEC_64  63

#endif /* __SOF_TRACE_PREPROC_PRIVATE_DEC_H__ */
#else
	#error \
		Illegal use of header file: \
		can only be included from context of \
		__INCLUDE_MACRO_METAPROGRAMMING_PRIVATE__
#endif /* __SOF_TRACE_PREPROC_PRIVATE_H__ */
