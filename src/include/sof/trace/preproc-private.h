/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@intel.com>
 */

/* THIS FILE SHOULD NOT BE INCLUDED DIRECTLY */

#ifdef __SOF_TRACE_PREPROC_H__
/* Macros defined in this file are only helpers for the macros that are
 * defined in header file containing "namespace"
 *     __SOF_TRACE_PREPROC_H__ .
 * This combination of #ifdef and #ifndef should sufficently narrow
 * the "include-ability" of this dependent header file.
 * If you wish to use macros from this file directly, be *V E R Y* careful!
 * HIC SUNT DRACONES
 */
#ifndef __SOF_TRACE_PREPROC_PRIVATE_H__
#define __SOF_TRACE_PREPROC_PRIVATE_H__

/* Include
 * #define _META_DEC_0   0
 * #define _META_DEC_1   1
 * #define _META_DEC_2   1
 * #define _META_DEC_3   2
 * ...
 * #define _META_DEC_N   N-1
 */
#include <sof/trace/preproc-private-dec.h>
/* Include
 * #define _META_INC_0   1
 * #define _META_INC_1   2
 * ...
 * #define _META_INC_N-1 N
 * #define _META_INC_N   N
 */
#include <sof/trace/preproc-private-inc.h>

/* count number of var args - during preprocesing
 * works for predefined number of args
 * META_COUNT_VARAGS_BEFORE_COMPILE(A,B,C,D) evaluates to 4
 */
#define _META_PP_NARG_BEFORE_COMPILE_(...) \
	_META_PP_ARG_N(__VA_ARGS__)

#define _META_PP_ARG_N(\
	_1,   _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, _10, \
	_11, _12, _13, _14, _15, _16, _17, _18, _19, _20, \
	_21, _22, _23, _24, _25, _26, _27, _28, _29, _30, \
	_31, _32, _33, _34, _35, _36, _37, _38, _39, _40, \
	_41, _42, _43, _44, _45, _46, _47, _48, _49, _50, \
	_51, _52, _53, _54, _55, _56, _57, _58, _59, _60, \
	_61, _62, _63, N, ...) N

#define _META_PP_RSEQ_N() \
	63, 62, 61, 60, \
	59, 58, 57, 56, 55, 54, 53, 52, 51, 50, \
	49, 48, 47, 46, 45, 44, 43, 42, 41, 40, \
	39, 38, 37, 36, 35, 34, 33, 32, 31, 30, \
	29, 28, 27, 26, 25, 24, 23, 22, 21, 20, \
	19, 18, 17, 16, 15, 14, 13, 12, 11, 10, \
	 9,  8,  7,  6,  5,  4,  3,  2,  1,  0

/* treat x as string while forcing x expansion beforehand */
#define _META_QUOTE(x) #x

/* concat x and y while forcing x and y expansion beforehand */
#define _META_CONCAT_BASE(x, y) x##y

/* discard first x-1 args in vararg and return the xth arg */
#define _META_GET_ARG_1(arg1, ...) arg1
#define _META_GET_ARG_2(arg1, arg2, ...) arg2
/* TODO: GET_ARG version for arbitrary x>2 should be possible using
 * META_RECURSE(META_REPEAT
 */

/* _META_IS_PROBE(...) evaluates to 0 when __VA_ARGS__ is single token
 * _META_IS_PROBE(PROBE()) evaulates to 1, because it is equivalent to
 * _META_GET_ARG_2(~, 1, 0)
 * ~ is no special value, it is just a meaningless placeholder,
 * it could be something else if that thing would also have no meaning
 * but be a valid C
 */
#define _META_IS_PROBE(...) _META_GET_ARG_2(__VA_ARGS__, 0)
#define _META_PROBE() ~, 1

/* _META_NOT_0 evaluates to '~, 1'
 * _META_NOT_1 evaluates to '_META_NOT_1' (because it is not a macro)
 * _META_IS_PROBE(_META_NOT_0) evaluates to 1, because it is equivalent to
 * _META_GET_ARG_2(~, 1, 0)
 * _META_IS_PROBE(_NOT_1)      evaluates to 0, because it is equivalent to
 * _META_GET_ARG_2(_NOT_1, 0)
 *
 * notice, that any x!=0 would also result in 0
 * e.x. META_NOT(123) evaluates to 0
 */
#define _META_NOT_0 _META_PROBE()

/* indirection forces condition to be "cast" to 0 1
 * then for 0 discard first (), and for 1 discard second ()
 * so  META_IF_ELSE(0)(a)(b) expands to b,
 * and META_IF_ELSE(1)(a)(b) expands to a
 */
#define _META_IF_ELSE(condition) META_CONCAT(_META_IF_, condition)

#define _META_IF_1(...) __VA_ARGS__ _META_IF_1_ELSE
#define _META_IF_0(...)             _META_IF_0_ELSE

#define _META_IF_1_ELSE(...)
#define _META_IF_0_ELSE(...) __VA_ARGS__

#define _META_IIF(condition) META_CONCAT(_META_IIF_, condition)
#define _META_IIF_0(x, ...) __VA_ARGS__
#define _META_IIF_1(x, ...) x

/* primitive recursion */
#define _META_REQRS_8(...)    _META_REQRS_4(  _META_REQRS_4  (__VA_ARGS__))
#define _META_REQRS_4(...)    _META_REQRS_2(  _META_REQRS_2  (__VA_ARGS__))
#define _META_REQRS_2(...)    _META_REQRS_1(  _META_REQRS_1  (__VA_ARGS__))
#define _META_REQRS_1(...) __VA_ARGS__

/* Delay macro m expansion depth times
 * IT IS CRUCIAL FOR NO #define _META_EMPTY macro to exist!!!
 * _META_DEFER_N(depth) will work for any depth valid in META_REPEAT
 * (which is confined only by META_DEC).
 * _META_DEFER_N will NOT work inside META_REPEAT, because
 * _META_DEFER_N uses META_REPEAT as seen below.
 * In order for META_REPEAT to work (which also requires DEFER functionality)
 * a duplicate, implicit _META_DEFER_2(m) has to be defined.
 * It is because how the c preprocesor works.
 */
#define _META_EMPTY()

/* Special, implicit defer implementation for META_REPEAT to work */
#define _META_DEFER_2(m) m _META_EMPTY _META_EMPTY () ()

/* map every group of arg_count arguments onto function m
 * i.e. arg_count=2;m=ADD;args=1,2,3,4,5,6,7...
 * results in ADD(1,2) ADD(3,4) ADD(5,6) and so on
 * MAP##N must exist for arg_count == N to work
 */
#define _META_MAP() META_MAP

/* implements MAP(1, m, ...) */
#define _META_MAP_1(m, arg1, ...)\
	m(arg1)\
	_META_DEFER_2(_META_MAP_BODY_TMP)()(1, m, __VA_ARGS__)

/* implements MAP(2, m, ...) */
#define _META_MAP_2(m, arg1, arg2, ...)\
	m(arg1, arg2)\
	_META_DEFER_2(_META_MAP_BODY_TMP)()(2, m, __VA_ARGS__)

/* implements MAP(3, m, ...) */
#define _META_MAP_3(m, arg1, arg2, arg3, ...)\
	m(arg1, arg2, arg3)\
	_META_DEFER_2(_META_MAP_BODY_TMP)()(3, m, __VA_ARGS__)

/* used by macro MAP, don't use on its own */
#define _META_MAP_BODY(arg_count, m, ...)\
	META_IF_ELSE(META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__))(\
		META_CONCAT(_META_MAP_, arg_count)(m, __VA_ARGS__) \
	)()
#define _META_MAP_BODY_TMP() _META_MAP_BODY

/* map aggregator and every group of arg_count arguments onto function m
 * i.e. aggr=x;arg_count=1;m=ADD;args=1,2,3,4,5,6,7...
 * results in x = ... ADD(7,ADD(6,ADD(5,ADD(4,ADD(3,ADD(2,ADD(1,x))))))) ...
 * MAP##N must exist for arg_count == N to work
 */
#define _META_MAP_AGGREGATE() META_MAP_AGGREGATE

/* implements MAP_AGGREGATE(1, m, ...) */
#define _META_MAP_AGGREGATE_1(m, aggr, arg1, ...)\
	_META_MAP_AGGREGATE_BODY(1, m, m(aggr, arg1), __VA_ARGS__)

/* implements MAP_AGGREGATE(2, m, ...) */
#define _META_MAP_AGGREGATE_2(m, aggr, arg1, arg2, ...)\
	_META_MAP_AGGREGATE_BODY(2, m, m(aggr, arg1, arg2), __VA_ARGS__)

/* used by macro MAP_AGGREGATE, don't use on its own */
#define _META_MAP_AGGREGATE_BODY(arg_count, m, aggr, ...)\
	META_IF_ELSE(META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__))(\
		_META_DEFER_2(_META_MAP_AGGREGATE)()\
			(arg_count, m, aggr, __VA_ARGS__)\
	)(aggr)

/* UNUSED private macros */
#define _META_VOID(x) (void)(x)
#define _META_VOID2(x, y) x; _META_VOID(y)

#endif /* __SOF_TRACE_PREPROC_PRIVATE_H__ */
#else
	#error \
		Illegal use of header file: \
		can only be included from context of \
		__INCLUDE_MACRO_METAPROGRAMMING__
#endif /* __SOF_TRACE_PREPROC_H__ */
