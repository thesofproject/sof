/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Sound Open Firmware. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __SOF_AUDIO_ARM_SIMD_H__
#define __SOF_AUDIO_ARM_SIMD_H__

#include <stdint.h>
#include <stdbool.h>

#if defined(__arm__) || defined(__thumb__) || defined(CONFIG_ARM)

#if defined(__ZEPHYR__)
#include <cmsis_core.h>
#endif

/*
 * Branchless Saturation Primitives using ARM SSAT instruction
 */
static inline int16_t arm_sat_s16(int32_t val)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("ssat %0, #16, %1" : "=r"(res) : "r"(val));
	return (int16_t)res;
#elif defined(__SSAT)
	return (int16_t)__SSAT(val, 16);
#else
	if (val > 32767)
		return 32767;
	if (val < -32768)
		return -32768;
	return (int16_t)val;
#endif
}

static inline int32_t arm_sat_s24(int32_t val)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("ssat %0, #24, %1" : "=r"(res) : "r"(val));
	return res;
#elif defined(__SSAT)
	return __SSAT(val, 24);
#else
	if (val > 8388607)
		return 8388607;
	if (val < -8388608)
		return -8388608;
	return val;
#endif
}

static inline int32_t arm_sat_q31(int64_t val)
{
	if (__builtin_expect(val >= (int64_t)INT32_MIN && val <= (int64_t)INT32_MAX, 1))
		return (int32_t)val;
	return (val > INT32_MAX) ? INT32_MAX : INT32_MIN;
}

/*
 * Dual 16-bit Arithmetic
 */
static inline uint32_t arm_qadd16(uint32_t a, uint32_t b)
{
#if defined(__GNUC__)
	uint32_t res;
	__asm__("qadd16 %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
	return res;
#elif defined(__QADD16)
	return __QADD16(a, b);
#else
	int16_t a_l = (int16_t)(a & 0xffff);
	int16_t a_h = (int16_t)(a >> 16);
	int16_t b_l = (int16_t)(b & 0xffff);
	int16_t b_h = (int16_t)(b >> 16);
	int16_t r_l = arm_sat_s16((int32_t)a_l + b_l);
	int16_t r_h = arm_sat_s16((int32_t)a_h + b_h);
	return ((uint32_t)(uint16_t)r_h << 16) | (uint16_t)r_l;
#endif
}

static inline uint32_t arm_qsub16(uint32_t a, uint32_t b)
{
#if defined(__GNUC__)
	uint32_t res;
	__asm__("qsub16 %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
	return res;
#elif defined(__QSUB16)
	return __QSUB16(a, b);
#else
	int16_t a_l = (int16_t)(a & 0xffff);
	int16_t a_h = (int16_t)(a >> 16);
	int16_t b_l = (int16_t)(b & 0xffff);
	int16_t b_h = (int16_t)(b >> 16);
	int16_t r_l = arm_sat_s16((int32_t)a_l - b_l);
	int16_t r_h = arm_sat_s16((int32_t)a_h - b_h);
	return ((uint32_t)(uint16_t)r_h << 16) | (uint16_t)r_l;
#endif
}

static inline int32_t arm_qadd(int32_t a, int32_t b)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("qadd %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
	return res;
#elif defined(__QADD)
	return __QADD(a, b);
#else
	int64_t sum = (int64_t)a + b;
	return arm_sat_q31(sum);
#endif
}

static inline int32_t arm_qsub(int32_t a, int32_t b)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("qsub %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
	return res;
#elif defined(__QSUB)
	return __QSUB(a, b);
#else
	int64_t diff = (int64_t)a - b;
	return arm_sat_q31(diff);
#endif
}

/*
 * Signed 16-bit Multiply
 */
static inline int32_t arm_smulbb(int32_t a, int32_t b)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("smulbb %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
	return res;
#else
	return (int32_t)(int16_t)(a & 0xffff) * (int16_t)(b & 0xffff);
#endif
}

static inline int32_t arm_smultt(int32_t a, int32_t b)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("smultt %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
	return res;
#else
	return (int32_t)(int16_t)(a >> 16) * (int16_t)(b >> 16);
#endif
}

/*
 * Signed Most Significant Word Multiply with Rounding (SMMULR)
 * Computes: (a * b + 0x80000000) >> 32 in a single clock cycle.
 * Ideal for Q1.31 audio scaling.
 */
static inline int32_t arm_smmulr(int32_t a, int32_t b)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("smmulr %0, %1, %2" : "=r"(res) : "r"(a), "r"(b));
	return res;
#else
	return (int32_t)(((int64_t)a * b + (1LL << 31)) >> 32);
#endif
}

/*
 * 64-bit Multiply-Accumulate (SMLAL)
 * Computes: acc += (int64_t)a * b in 1 cycle throughput.
 */
static inline int64_t arm_smlal(int64_t acc, int32_t a, int32_t b)
{
#if defined(__GNUC__)
	uint32_t lo = (uint32_t)acc;
	int32_t hi = (int32_t)(acc >> 32);
	__asm__("smlal %0, %1, %2, %3" : "+r"(lo), "+r"(hi) : "r"(a), "r"(b));
	return ((int64_t)hi << 32) | lo;
#else
	return acc + ((int64_t)a * b);
#endif
}

/*
 * 32x16 Word-Halfword Multiply Accumulate (SMLAWB / SMLAWT)
 */
static inline int32_t arm_smlawb(int32_t acc, int32_t a, int32_t b)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("smlawb %0, %1, %2, %3" : "=r"(res) : "r"(a), "r"(b), "r"(acc));
	return res;
#else
	return acc + (int32_t)(((int64_t)a * (int16_t)(b & 0xffff)) >> 16);
#endif
}

static inline int32_t arm_smlawt(int32_t acc, int32_t a, int32_t b)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("smlawt %0, %1, %2, %3" : "=r"(res) : "r"(a), "r"(b), "r"(acc));
	return res;
#else
	return acc + (int32_t)(((int64_t)a * (int16_t)(b >> 16)) >> 16);
#endif
}

/*
 * Pack Halfwords (PKHBT / PKHTB)
 */
static inline uint32_t arm_pkhbt(uint32_t a, uint32_t b, uint32_t lsl_shift)
{
#if defined(__GNUC__)
	uint32_t res;
	__asm__("pkhbt %0, %1, %2, lsl %3" : "=r"(res) : "r"(a), "r"(b), "I"(lsl_shift));
	return res;
#elif defined(__PKHBT)
	return __PKHBT(a, b, lsl_shift);
#else
	return (a & 0xffff) | ((b << lsl_shift) & 0xffff0000);
#endif
}

/*
 * Dual 16-bit Multiply-Accumulate (SMLAD / SMLALD)
 */
static inline int32_t arm_smlad(uint32_t a, uint32_t b, int32_t acc)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("smlad %0, %1, %2, %3" : "=r"(res) : "r"(a), "r"(b), "r"(acc));
	return res;
#elif defined(__SMLAD)
	return __SMLAD(a, b, acc);
#else
	int16_t a0 = (int16_t)(a & 0xffff);
	int16_t a1 = (int16_t)(a >> 16);
	int16_t b0 = (int16_t)(b & 0xffff);
	int16_t b1 = (int16_t)(b >> 16);
	return acc + (int32_t)a0 * b0 + (int32_t)a1 * b1;
#endif
}

static inline int64_t arm_smlald(uint32_t a, uint32_t b, int64_t acc)
{
#if defined(__GNUC__)
	uint32_t lo = (uint32_t)acc;
	int32_t hi = (int32_t)(acc >> 32);
	__asm__("smlald %0, %1, %2, %3" : "+r"(lo), "+r"(hi) : "r"(a), "r"(b));
	return ((int64_t)hi << 32) | lo;
#elif defined(__SMLALD)
	return __SMLALD(a, b, acc);
#else
	int16_t a0 = (int16_t)(a & 0xffff);
	int16_t a1 = (int16_t)(a >> 16);
	int16_t b0 = (int16_t)(b & 0xffff);
	int16_t b1 = (int16_t)(b >> 16);
	return acc + ((int64_t)a0 * b0) + ((int64_t)a1 * b1);
#endif
}

/*
 * Sign Extend Halfword (SXTH)
 */
static inline int32_t arm_sxth(int32_t val)
{
#if defined(__GNUC__)
	int32_t res;
	__asm__("sxth %0, %1" : "=r"(res) : "r"(val));
	return res;
#elif defined(__SXTH)
	return __SXTH(val);
#else
	return (int32_t)(int16_t)(val & 0xffff);
#endif
}

/*
 * Count Leading Zeros
 */
static inline uint32_t arm_clz(uint32_t val)
{
	return (uint32_t)__builtin_clz(val);
}

#else /* Non-ARM fallback */

static inline int16_t arm_sat_s16(int32_t val)
{
	if (val > 32767) return 32767;
	if (val < -32768) return -32768;
	return (int16_t)val;
}

static inline int32_t arm_sat_s24(int32_t val)
{
	if (val > 8388607) return 8388607;
	if (val < -8388608) return -8388608;
	return val;
}

static inline int32_t arm_sat_q31(int64_t val)
{
	if (val > INT32_MAX) return INT32_MAX;
	if (val < INT32_MIN) return INT32_MIN;
	return (int32_t)val;
}

static inline uint32_t arm_qadd16(uint32_t a, uint32_t b)
{
	int16_t a_l = (int16_t)(a & 0xffff);
	int16_t a_h = (int16_t)(a >> 16);
	int16_t b_l = (int16_t)(b & 0xffff);
	int16_t b_h = (int16_t)(b >> 16);
	int16_t r_l = arm_sat_s16((int32_t)a_l + b_l);
	int16_t r_h = arm_sat_s16((int32_t)a_h + b_h);
	return ((uint32_t)(uint16_t)r_h << 16) | (uint16_t)r_l;
}

static inline int32_t arm_qadd(int32_t a, int32_t b)
{
	return arm_sat_q31((int64_t)a + b);
}

static inline int32_t arm_smulbb(int32_t a, int32_t b)
{
	return (int32_t)(int16_t)(a & 0xffff) * (int16_t)(b & 0xffff);
}

static inline int32_t arm_smultt(int32_t a, int32_t b)
{
	return (int32_t)(int16_t)(a >> 16) * (int16_t)(b >> 16);
}

static inline int32_t arm_smmulr(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)a * b + (1LL << 31)) >> 32);
}

static inline int64_t arm_smlal(int64_t acc, int32_t a, int32_t b)
{
	return acc + ((int64_t)a * b);
}

static inline uint32_t arm_pkhbt(uint32_t a, uint32_t b, uint32_t lsl_shift)
{
	return (a & 0xffff) | ((b << lsl_shift) & 0xffff0000);
}

static inline int32_t arm_smlad(uint32_t a, uint32_t b, int32_t acc)
{
	int16_t a0 = (int16_t)(a & 0xffff);
	int16_t a1 = (int16_t)(a >> 16);
	int16_t b0 = (int16_t)(b & 0xffff);
	int16_t b1 = (int16_t)(b >> 16);
	return acc + (int32_t)a0 * b0 + (int32_t)a1 * b1;
}

static inline int64_t arm_smlald(uint32_t a, uint32_t b, int64_t acc)
{
	int16_t a0 = (int16_t)(a & 0xffff);
	int16_t a1 = (int16_t)(a >> 16);
	int16_t b0 = (int16_t)(b & 0xffff);
	int16_t b1 = (int16_t)(b >> 16);
	return acc + ((int64_t)a0 * b0) + ((int64_t)a1 * b1);
}

static inline int32_t arm_sxth(int32_t val)
{
	return (int32_t)(int16_t)(val & 0xffff);
}

static inline uint32_t arm_clz(uint32_t val)
{
	return (uint32_t)__builtin_clz(val);
}

#endif /* defined(__arm__) || defined(__thumb__) || defined(CONFIG_ARM) */

#endif /* __SOF_AUDIO_ARM_SIMD_H__ */
