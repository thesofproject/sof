/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@intel.com>
 */

 /* THIS FILE SHOULD NOT BE INCLUDED DIRECTLY */

#ifdef __INCLUDE_MACRO_METAPROGRAMMING_PRIVATE__
/* Macros defined in this file are only helpers for the macros that are
 * defined in header file containing "namespace"
 *     __INCLUDE_MACRO_METAPROGRAMMING_PRIVATE__ .
 * This combination of #ifdef and #ifndef should sufficently narrow
 * the "include-ability" of this dependent header file.
 * If you wish to use macros from this file directly, be *V E R Y* careful!
 * HIC SUNT DRACONES
 */
#ifndef __INCLUDE_MACRO_METAPROGRAMMING_PRIVATE_INC__
#define __INCLUDE_MACRO_METAPROGRAMMING_PRIVATE_INC__

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

#endif // __INCLUDE_MACRO_METAPROGRAMMING_PRIVATE_INC__
#else
	#error \
		Illegal use of header file: \
		can only be included from context of \
		__INCLUDE_MACRO_METAPROGRAMMING_PRIVATE__
#endif // __INCLUDE_MACRO_METAPROGRAMMING_PRIVATE__
