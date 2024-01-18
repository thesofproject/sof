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
#ifndef __SOF_TRACE_PREPROC_PRIVATE_INC_H__
#define __SOF_TRACE_PREPROC_PRIVATE_INC_H__

/* The only sane way I found to increment values in cpreproc */
/* for instance META_INC(3) will be tokenized to INC_3
 * and then expanded again to 4
 */
#define _META_INC_0    1
#define _META_INC_1    2
#define _META_INC_2    3
#define _META_INC_3    4
#define _META_INC_4    5
#define _META_INC_5    6
#define _META_INC_6    7
#define _META_INC_7    8
#define _META_INC_8    9
#define _META_INC_9   10
#define _META_INC_10  11
#define _META_INC_11  12
#define _META_INC_12  13
#define _META_INC_13  14
#define _META_INC_14  15
#define _META_INC_15  16
#define _META_INC_16  17
#define _META_INC_17  18
#define _META_INC_18  19
#define _META_INC_19  20
#define _META_INC_20  21
#define _META_INC_21  22
#define _META_INC_22  23
#define _META_INC_23  24
#define _META_INC_24  25
#define _META_INC_25  26
#define _META_INC_26  27
#define _META_INC_27  28
#define _META_INC_28  29
#define _META_INC_29  30
#define _META_INC_30  31
#define _META_INC_31  32
#define _META_INC_32  33
#define _META_INC_33  34
#define _META_INC_34  35
#define _META_INC_35  36
#define _META_INC_36  37
#define _META_INC_37  38
#define _META_INC_38  39
#define _META_INC_39  40
#define _META_INC_40  41
#define _META_INC_41  42
#define _META_INC_42  43
#define _META_INC_43  44
#define _META_INC_44  45
#define _META_INC_45  46
#define _META_INC_46  47
#define _META_INC_47  48
#define _META_INC_48  49
#define _META_INC_49  50
#define _META_INC_50  51
#define _META_INC_51  52
#define _META_INC_52  53
#define _META_INC_53  54
#define _META_INC_54  55
#define _META_INC_55  56
#define _META_INC_56  57
#define _META_INC_57  58
#define _META_INC_58  59
#define _META_INC_59  60
#define _META_INC_60  61
#define _META_INC_61  62
#define _META_INC_62  63
#define _META_INC_63  64
#define _META_INC_64  64 // notice how we deal with overflow

#endif /* __SOF_TRACE_PREPROC_PRIVATE_INC_H__ */
#else
	#error \
		Illegal use of header file: \
		can only be included from context of \
		__INCLUDE_MACRO_METAPROGRAMMING_PRIVATE__
#endif /* __SOF_TRACE_PREPROC_PRIVATE_H__ */
