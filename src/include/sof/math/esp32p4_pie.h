/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Sound Open Firmware (SOF) Project
 *
 * Author: Antigravity AI / SOF Team
 */

#ifndef __SOF_MATH_ESP32P4_PIE_H__
#define __SOF_MATH_ESP32P4_PIE_H__

#include <stdint.h>
#include <stdbool.h>

#if defined(CONFIG_SOC_SERIES_ESP32P4) || defined(CONFIG_ESP32P4_PIE_SIMD)

/*
 * ESP32-P4 Processor Instruction Extensions (PIE / Xespv2p2) 128-bit SIMD.
 *
 * To ensure 100% build compatibility with the upstream Zephyr SDK GCC
 * (which lacks custom -march=xesppie assembler recognition), all PIE
 * instructions are implemented using native 32-bit machine word directives (.word).
 *
 * Register mapping conventions:
 *   q0..q7 : 128-bit vector registers (16x8b, 8x16b, or 4x32b)
 *   QACC   : 256-bit accumulator pair (QACC_H / QACC_L)
 *   SAR    : 6-bit Shift Amount Register
 *   a0     : Source pointer (incremented by 16 bytes on vld.128.ip)
 *   a2     : Broadcast scalar / control value
 */

/* Enable / Disable PIE Coprocessor via CSR 0x7F2 (CSR_PIE_STATE_REG) */
static inline void esp_pie_enable(void)
{
	__asm__ volatile ("csrw 0x7F2, %0" :: "r"(1));
}

static inline void esp_pie_disable(void)
{
	__asm__ volatile ("csrw 0x7F2, %0" :: "r"(0));
}

/* Zeroing */
#define ESP_PIE_ZERO_Q0()         __asm__ volatile (".word 0x0040005b") /* esp.zero.q q0 */
#define ESP_PIE_ZERO_Q1()         __asm__ volatile (".word 0x004000db") /* esp.zero.q q1 */
#define ESP_PIE_ZERO_Q2()         __asm__ volatile (".word 0x0040015b") /* esp.zero.q q2 */
#define ESP_PIE_ZERO_Q3()         __asm__ volatile (".word 0x004001db") /* esp.zero.q q3 */
#define ESP_PIE_ZERO_QACC()       __asm__ volatile (".word 0x0000025b") /* esp.zero.qacc */

/* Vector Load 128-bit (16 bytes) with address post-increment (+16) on a0 */
#define ESP_PIE_VLD_128_IP_Q0_A0() \
	__asm__ volatile (".word 0x0201223b" : "+r"(src_a0) :: "memory") /* esp.vld.128.ip q0, a0, 16 */

#define ESP_PIE_VLD_128_IP_Q1_A0() \
	__asm__ volatile (".word 0x0201263b" : "+r"(src_a0) :: "memory") /* esp.vld.128.ip q1, a0, 16 */

#define ESP_PIE_VLD_128_IP_Q2_A0() \
	__asm__ volatile (".word 0x02012a3b" : "+r"(src_a0) :: "memory") /* esp.vld.128.ip q2, a0, 16 */

/* Vector Store 128-bit (16 bytes) with address post-increment (+16) on a1 */
#define ESP_PIE_VST_128_IP_Q0_A1() \
	__asm__ volatile (".word 0x8201a23b" : "+r"(dst_a1) :: "memory") /* esp.vst.128.ip q0, a1, 16 */

#define ESP_PIE_VST_128_IP_Q1_A1() \
	__asm__ volatile (".word 0x8201a63b" : "+r"(dst_a1) :: "memory") /* esp.vst.128.ip q1, a1, 16 */

/* Broadcast 16-bit scalar from address at a2 to all 8 lanes of q2 */
#define ESP_PIE_VLDBC_16_IP_Q2_A2() \
	__asm__ volatile (".word 0x8602283b" : "+r"(bc_a2) :: "memory") /* esp.vldbc.16.ip q2, a2, 0 */

/* Broadcast 32-bit packed stereo [L, R] scalar from address at a2 to all 4 stereo pairs in q2 */
#define ESP_PIE_VLDBC_32_IP_Q2_A2() \
	__asm__ volatile (".word 0x0e02283b" : "+r"(bc_a2) :: "memory") /* esp.vldbc.32.ip q2, a2, 0 */

/* Vector Multiplication signed 16-bit: product shifted right by SAR, lower 16 bits retained */
#define ESP_PIE_VMUL_S16_Q0_Q0_Q2() __asm__ volatile (".word 0x0806bc5f") /* esp.vmul.s16 q0, q0, q2 */
#define ESP_PIE_VMUL_S16_Q1_Q1_Q2() __asm__ volatile (".word 0x2806bcdf") /* esp.vmul.s16 q1, q1, q2 */
#define ESP_PIE_VMUL_S16_Q0_Q0_Q1() __asm__ volatile (".word 0x0406bc5f") /* esp.vmul.s16 q0, q0, q1 */

/* Vector Add signed 16-bit elements */
#define ESP_PIE_VADD_S16_Q0_Q0_Q1() __asm__ volatile (".word 0x0684065f") /* esp.vadd.s16 q0, q0, q1 */
#define ESP_PIE_VADD_S16_Q0_Q1_Q2() __asm__ volatile (".word 0x2a84065f") /* esp.vadd.s16 q0, q1, q2 */

/* Vector Sub signed 16-bit elements */
#define ESP_PIE_VSUB_S16_Q0_Q0_Q1() __asm__ volatile (".word 0x068406df") /* esp.vsub.s16 q0, q0, q1 */
#define ESP_PIE_VSUB_S16_Q0_Q1_Q2() __asm__ volatile (".word 0x2a8406df") /* esp.vsub.s16 q0, q1, q2 */

/* Vector Clamp signed 16-bit elements to signed 16-bit PCM range [-32768, 32767] (imm=15) */
#define ESP_PIE_VCLAMP_S16_Q0_Q0()  __asm__ volatile (".word 0x030c505b") /* esp.vclamp.s16 q0, q0, 15 */
#define ESP_PIE_VCLAMP_S16_Q0_Q1()  __asm__ volatile (".word 0x230c505b") /* esp.vclamp.s16 q0, q1, 15 */
#define ESP_PIE_VCLAMP_S16_Q1_Q1()  __asm__ volatile (".word 0x230c50db") /* esp.vclamp.s16 q1, q1, 15 */

/* Vector Interleave (Zip) and Deinterleave (Unzip) 16-bit elements */
#define ESP_PIE_VZIP_16_Q0_Q1()     __asm__ volatile (".word 0x0682005f") /* esp.vzip.16 q0, q1 */
#define ESP_PIE_VUNZIP_16_Q0_Q1()   __asm__ volatile (".word 0x0686005f") /* esp.vunzip.16 q0, q1 */
#define ESP_PIE_VZIP_16_Q2_Q3()     __asm__ volatile (".word 0x4e82005f") /* esp.vzip.16 q2, q3 */
#define ESP_PIE_VUNZIP_16_Q2_Q3()   __asm__ volatile (".word 0x4e86005f") /* esp.vunzip.16 q2, q3 */

/* Shift Amount Register (SAR) and CFG configuration */
static inline void esp_pie_set_sar(uint32_t sar)
{
	register uint32_t sar_reg asm("a0") = sar;
	__asm__ volatile (".word 0x90b1005f" : "+r"(sar_reg) : : "memory");
}
#define ESP_PIE_SET_SAR(sar)        esp_pie_set_sar(sar)
#define ESP_PIE_SET_SAR_A0()        esp_pie_set_sar(15)
#define ESP_PIE_SET_SAR_A2()        __asm__ volatile (".word 0x90b2005f" :: "r"(sar_a2)) /* esp.movx.w.sar a2 */
#define ESP_PIE_SET_CFG_A0()        __asm__ volatile (".word 0x90d1005f" :: "r"(cfg_a0)) /* esp.movx.w.cfg a0 */

/* Vector Right Shift 32-bit elements by SAR */
#define ESP_PIE_VSR_S32_Q0_Q1()     __asm__ volatile (".word 0x8404035b") /* esp.vsr.s32 q0, q1 */
#define ESP_PIE_VSR_S32_Q0_Q0()     __asm__ volatile (".word 0x8004035b") /* esp.vsr.s32 q0, q0 */

/* Vector Unzip & Truncate (Pack 32-bit lanes from q1, q2 into 16-bit lanes in q0) */
#define ESP_PIE_VUNZIPT_16_Q0_Q1_Q2() __asm__ volatile (".word 0x06c04c5b") /* esp.vunzipt.16 q0, q1, q2 */
#define ESP_PIE_VUNZIPT_16_Q0_Q0_Q1() __asm__ volatile (".word 0x01c04c5b") /* esp.vunzipt.16 q0, q0, q1 */

#endif /* CONFIG_SOC_SERIES_ESP32P4 || CONFIG_ESP32P4_PIE_SIMD */

#endif /* __SOF_MATH_ESP32P4_PIE_H__ */
