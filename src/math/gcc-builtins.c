// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c); 2025 Intel Corporation. All rights reserved.

#include <stdint.h>
#include <rtos/symbol.h>

/*
 * Below we export compiler builtin functions to be used by LLEXT modules.
 *
 * These builtins are defined in the following places:
 *
 * https://gcc.gnu.org/onlinedocs/gccint/Soft-float-library-routines.html
 * https://gcc.gnu.org/onlinedocs/gccint/Integer-library-routines.html
 * https://refspecs.linuxbase.org/LSB_5.0.0/LSB-Core-generic/LSB-Core-generic/baselib---signbitf.html
 *
 * Note: the list below are found in xtensa libgcc, other architecture may
 * export more or less and we should then partition with some ifdefs.
 */
#if !CONFIG_64BIT

long __ashldi3(long a, int b);
EXPORT_SYMBOL(__ashldi3);

long __ashrdi3(long a, int b);
EXPORT_SYMBOL(__ashrdi3);

int __divsi3(int a, int b);
EXPORT_SYMBOL(__divsi3);

long __divdi3(long a, long b);
EXPORT_SYMBOL(__divdi3);

long __lshrdi3(long a, int b);
EXPORT_SYMBOL(__lshrdi3);

int __modsi3(int a, int b);
EXPORT_SYMBOL(__modsi3);

long __moddi3(long a, long b);
EXPORT_SYMBOL(__moddi3);

int __mulsi3(int a, int b);
EXPORT_SYMBOL(__mulsi3);

long __muldi3(long a, long b);
EXPORT_SYMBOL(__muldi3);

long __negdi2(long a);
EXPORT_SYMBOL(__negdi2);

unsigned int __udivsi3(unsigned int a, unsigned int b);
EXPORT_SYMBOL(__udivsi3);

unsigned long __udivdi3(unsigned long a, unsigned long b);
EXPORT_SYMBOL(__udivdi3);

unsigned long __udivmoddi4(unsigned long a, unsigned long b, unsigned long *c);
EXPORT_SYMBOL(__udivmoddi4);

unsigned int __umodsi3(unsigned int a, unsigned int b);
EXPORT_SYMBOL(__umodsi3);

unsigned long __umoddi3(unsigned long a, unsigned long b);
EXPORT_SYMBOL(__umoddi3);

int __cmpdi2(long a, long b);
EXPORT_SYMBOL(__cmpdi2);

int __ucmpdi2(unsigned long a, unsigned long b);
EXPORT_SYMBOL(__ucmpdi2);

int __absvsi2(int a);
EXPORT_SYMBOL(__absvsi2);

long __absvdi2(long a);
EXPORT_SYMBOL(__absvdi2);

int __addvsi3(int a, int b);
EXPORT_SYMBOL(__addvsi3);

long __addvdi3(long a, long b);
EXPORT_SYMBOL(__addvdi3);

int __mulvsi3(int a, int b);
EXPORT_SYMBOL(__mulvsi3);

long __mulvdi3(long a, long b);
EXPORT_SYMBOL(__mulvdi3);

int __negvsi2(int a);
EXPORT_SYMBOL(__negvsi2);

long __negvdi2(long a);
EXPORT_SYMBOL(__negvdi2);

int __subvsi3(int a, int b);
EXPORT_SYMBOL(__subvsi3);

long __subvdi3(long a, long b);
EXPORT_SYMBOL(__subvdi3);

int __clzsi2(unsigned int a);
EXPORT_SYMBOL(__clzsi2);

int __clzdi2(unsigned long a);
EXPORT_SYMBOL(__clzdi2);

int __ctzsi2(unsigned int a);
EXPORT_SYMBOL(__ctzsi2);

int __ctzdi2(unsigned long a);
EXPORT_SYMBOL(__ctzdi2);

int __ffsdi2(unsigned long a);
EXPORT_SYMBOL(__ffsdi2);

int __paritysi2(unsigned int a);
EXPORT_SYMBOL(__paritysi2);

int __paritydi2(unsigned long a);
EXPORT_SYMBOL(__paritydi2);

int __popcountsi2(unsigned int a);
EXPORT_SYMBOL(__popcountsi2);

int __popcountdi2(unsigned long a);
EXPORT_SYMBOL(__popcountdi2);

float __addsf3(float a, float b);
EXPORT_SYMBOL(__addsf3);

double __adddf3(double a, double b);
EXPORT_SYMBOL(__adddf3);

float __subsf3(float a, float b);
EXPORT_SYMBOL(__subsf3);

double __subdf3(double a, double b);
EXPORT_SYMBOL(__subdf3);

float __mulsf3(float a, float b);
EXPORT_SYMBOL(__mulsf3);

double __muldf3(double a, double b);
EXPORT_SYMBOL(__muldf3);

float __divsf3(float a, float b);
EXPORT_SYMBOL(__divsf3);

double __divdf3(double a, double b);
EXPORT_SYMBOL(__divdf3);

float __negsf2(float a);
EXPORT_SYMBOL(__negsf2);

double __negdf2(double a);
EXPORT_SYMBOL(__negdf2);

double __extendsfdf2(float a);
EXPORT_SYMBOL(__extendsfdf2);

float __truncdfsf2(double a);
EXPORT_SYMBOL(__truncdfsf2);

int __fixsfsi(float a);
EXPORT_SYMBOL(__fixsfsi);

int __fixdfsi(double a);
EXPORT_SYMBOL(__fixdfsi);

long __fixsfdi(float a);
EXPORT_SYMBOL(__fixsfdi);

long __fixdfdi(double a);
EXPORT_SYMBOL(__fixdfdi);

unsigned int __fixunssfsi(float a);
EXPORT_SYMBOL(__fixunssfsi);

unsigned int __fixunsdfsi(double a);
EXPORT_SYMBOL(__fixunsdfsi);

unsigned long __fixunssfdi(float a);
EXPORT_SYMBOL(__fixunssfdi);

unsigned long __fixunsdfdi(double a);
EXPORT_SYMBOL(__fixunsdfdi);

float __floatsisf(int i);
EXPORT_SYMBOL(__floatsisf);

double __floatsidf(int i);
EXPORT_SYMBOL(__floatsidf);

float __floatdisf(long i);
EXPORT_SYMBOL(__floatdisf);

double __floatdidf(long i);
EXPORT_SYMBOL(__floatdidf);

float __floatunsisf(unsigned int i);
EXPORT_SYMBOL(__floatunsisf);

double __floatunsidf(unsigned int i);
EXPORT_SYMBOL(__floatunsidf);

float __floatundisf(unsigned long i);
EXPORT_SYMBOL(__floatundisf);

double __floatundidf(unsigned long i);
EXPORT_SYMBOL(__floatundidf);

int __unordsf2(float a, float b);
EXPORT_SYMBOL(__unordsf2);

int __unorddf2(double a, double b);
EXPORT_SYMBOL(__unorddf2);

int __eqsf2(float a, float b);
EXPORT_SYMBOL(__eqsf2);

int __eqdf2(double a, double b);
EXPORT_SYMBOL(__eqdf2);

int __nesf2(float a, float b);
EXPORT_SYMBOL(__nesf2);

int __nedf2(double a, double b);
EXPORT_SYMBOL(__nedf2);

int __gesf2(float a, float b);
EXPORT_SYMBOL(__gesf2);

int __gedf2(double a, double b);
EXPORT_SYMBOL(__gedf2);

int __ltsf2(float a, float b);
EXPORT_SYMBOL(__ltsf2);

int __ltdf2(double a, double b);
EXPORT_SYMBOL(__ltdf2);

int __lesf2(float a, float b);
EXPORT_SYMBOL(__lesf2);

int __ledf2(double a, double b);
EXPORT_SYMBOL(__ledf2);

int __gtsf2(float a, float b);
EXPORT_SYMBOL(__gtsf2);

int __gtdf2(double a, double b);
EXPORT_SYMBOL(__gtdf2);

float __powisf2(float a, int b);
EXPORT_SYMBOL(__powisf2);

double __powidf2(double a, int b);
EXPORT_SYMBOL(__powidf2);

#endif
