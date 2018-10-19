/*
 * Copyright (c) 2019, Intel Corporation
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
 */

#include <xtensa/config/core-isa.h>

#define REG_OFFSET_EXCCAUSE     0x0
#define REG_OFFSET_EXCVADDR     0x4
#define REG_OFFSET_PS           0x8
#define REG_OFFSET_EPC1         0xc
#define REG_OFFSET_EPC2         0x10
#define REG_OFFSET_EPC3         0x14
#define REG_OFFSET_EPC4         0x18
#define REG_OFFSET_EPC5         0x1c
#define REG_OFFSET_EPC6         0x20
#define REG_OFFSET_EPC7         0x24
#define REG_OFFSET_EPS2         0x28
#define REG_OFFSET_EPS3         0x2c
#define REG_OFFSET_EPS4         0x30
#define REG_OFFSET_EPS5         0x34
#define REG_OFFSET_EPS6         0x38
#define REG_OFFSET_EPS7         0x3c
#define REG_OFFSET_DEPC         0x40
#define REG_OFFSET_INTENABLE    0x44
#define REG_OFFSET_INTERRUPT    0x48
#define REG_OFFSET_SAR          0x4c
#define REG_OFFSET_DEBUGCAUSE   0x50
#define REG_OFFSET_WINDOWBASE   0x54
#define REG_OFFSET_WINDOWSTART  0x58
#define REG_OFFSET_EXCSAVE1     0x5c
#define REG_OFFSET_AR_BEGIN     0x60
#define REG_OFFSET_AR_END       (REG_OFFSET_AR_BEGIN + 4 * XCHAL_NUM_AREGS)
