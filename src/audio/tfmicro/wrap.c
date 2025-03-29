// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.

#if 0

U __assert_func
U atexit
U __cxa_pure_virtual

U __errno
U expf

U floor
U fmaxf
U fminf
U frexp

U _impure_ptr
U log_const_tfmicro
U logf
U memcmp
U memmove

U round

U strcmp
U strncmp

U __vec_memcpy
U __vec_memset
U vfprintf
U vsnprintf
#endif

float fminf( float x, float y)
{
    if (x < y)
        return x;
    else
        return y;
}

double fmin(double x, double y)
{
    if (x < y)
        return x;
    else
        return y;
}

void __assert_func(void *a, int b, void *c, void *d)
{}


