/*
 * xtensa/corebits.h - Xtensa Special Register field positions, masks, values.
 */

/*
 * Copyright (c) 2005-2018 Cadence Design Systems, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef XTENSA_COREBITS_H
#define XTENSA_COREBITS_H

#ifdef __XTENSA__
#include <xtensa/config/core-isa.h>
#else
#include "core-isa.h"
#endif

#if XCHAL_HAVE_XEA3

/*  EXCCAUSE register fields:  */
#define	EXCCAUSE_CAUSE_SHIFT	0
#define	EXCCAUSE_CAUSE_BITS	4
#define	EXCCAUSE_CAUSE_MASK	0x0F
#define	EXCCAUSE_TYPE_SHIFT	4
#define	EXCCAUSE_TYPE_BITS	4
#define	EXCCAUSE_TYPE_MASK	0xF0
#define	EXCCAUSE_SUBTYPE_SHIFT	8
#define	EXCCAUSE_SUBTYPE_BITS	4
#define	EXCCAUSE_SUBTYPE_MASK	0xF00
#define	EXCCAUSE_FULLTYPE_SHIFT	0
#define	EXCCAUSE_FULLTYPE_BITS	12
#define	EXCCAUSE_FULLTYPE_MASK	0xFFF
#define	EXCCAUSE_LSFO_SHIFT	12
#define	EXCCAUSE_LSFO_BITS	2
#define	EXCCAUSE_LSFO_MASK	0x3000
#define	EXCCAUSE_IMPR_SHIFT	14
#define	EXCCAUSE_IMPR_BITS	2
#define	EXCCAUSE_IMPR_MASK	0xC000

/*  EXCCAUSE register values:  */
/*  General Exception causes (Bits 3:0 of EXCCAUSE register)  */
#define	EXCCAUSE_NONE			0	/* No exception */
#define	EXCCAUSE_INSTRUCTION		1	/* Instruction usage */
#define	EXCCAUSE_ADDRESS		2	/* Addressing usage */
#define	EXCCAUSE_EXTERNAL		3	/* External causes */
#define	EXCCAUSE_DEBUG			4	/* Debug exception */
#define	EXCCAUSE_SYSCALL		5	/* Syscall exception */
#define	EXCCAUSE_HARDWARE		6	/* Hardware failure */
#define	EXCCAUSE_MEMORY			7	/* Memory management */
#define	EXCCAUSE_CP_DISABLED		8	/* Coprocessor */
/*  Reserved 9-15  */

/*  Complete exception identifier - Subtype/Type/Cause  */
/*  Instruction usage  */
#define	EXC_TYPE_ILL_INST		0x101	/* ILL(.N) instruction */
#define EXC_TYPE_ILLEGAL_INST		0x201	/* Illegal instruction (not ILL(.N)) */
#define	EXC_TYPE_PRIV_INSTRUCTION	0x111	/* User executing privileged instruction */
#define	EXC_TYPE_PRIV_ERI_ACCESS	0x211	/* User access to privileged ERI */
#define	EXC_TYPE_DIVIDE_BY_ZERO		0x121	/* Integer divide by zero */
#define	EXC_TYPE_ILLEGAL_ENTRY		0x221	/* Entry/Retw with CALL0 */
#define	EXC_TYPE_FP_INV_OP		0x141	/* FP exception invalid operation (V) */
#define	EXC_TYPE_FP_DIV_BY_ZERO		0x241	/* FP exception divide by zero (Z) */
#define	EXC_TYPE_FP_OVERFLOW		0x341	/* FP exception overflow (O) */
#define	EXC_TYPE_FP_UNDERFLOW		0x441	/* FP exception underflow (U) */
#define	EXC_TYPE_FP_INEXACT		0x541	/* FP exception inexact (I) */
#define	EXC_TYPE_FP_UF_INEXACT		0x641	/* FP exception underflow and inexact (UI) */
#define	EXC_TYPE_NONAR_SAT		0x151	/* non-AR saturation exception */

/*  Addressing usage  */
#define	EXC_TYPE_UNALIGNED_ACCESS	0x102	/* Unaligned access */
#define	EXC_TYPE_INVALID_PC		0x112	/* PC out of range  */
#define	EXC_TYPE_INVALID_TCM		0x122	/* Invalid TCM access */
#define	EXC_TYPE_UNALIGNED_OTHER	0x222	/* Unaligned access to IMEM/XLMI/Device */
#define	EXC_TYPE_IRAM_LDST		0x322	/* IRAM ld/st wrong instruction/slot */
#define	EXC_TYPE_CROSS_MEM_ACCESS	0x422	/* Cross memory/MPU access */
#define	EXC_TYPE_INVALID_MSPACE		0x522	/* Access outside valid memory space */
#define	EXC_TYPE_UNSUPP_ATOMIC_OP	0x622	/* Atomic op not supported by memory type */
#define	EXC_TYPE_UNSPEC_IFETCH		0x722	/* Unspecified ifetch error */
#define	EXC_TYPE_LINE_LOCK		0x142	/* Unlockable cache line */
#define EXC_TYPE_EXT_ATOMIC_LOCKED	0x242	/* External atomic to locked cache line */
#define	EXC_TYPE_BP_DOWNGRADE		0x342	/* Large downgrade block prefetch */
#define	EXC_TYPE_L1_HITMISS		0x442	/* L1S hit and L1V miss - if possible */
#define	EXC_TYPE_FLIX_OVERLAP		0x152	/* FLIX with overlap stores - if possible */
#define	EXC_TYPE_GS_IDX_TOO_LARGE	0x162	/* Gather/scatter index too large */
#define	EXC_TYPE_GS_UNALIGNED_ADDR	0x262	/* Gather/scatter unaligned address */
#define	EXC_TYPE_GS_UNCORR_ERROR	0x362	/* Gather/scatter uncorrectable ECC/parity error */

/*  External  */
#define	EXC_TYPE_BUS_ADDR_ERR		0x103	/* Bus decode/address error */
#define	EXC_TYPE_BUS_CASTOUT_ADDR	0x203	/* Castout decode/address write bus error */
#define	EXC_TYPE_BUS_DATA_ERR		0x113	/* Bus slave/data error */
#define	EXC_TYPE_BUS_CASTOUT_ERR	0x213	/* Castout slave/data write bus error */
#define	EXC_TYPE_BUS_TIMEOUT		0x123	/* Bus timeout */
#define	EXC_TYPE_BUS_CASTOUT_TO		0x223	/* Castout write - bus timeout */
#define	EXC_TYPE_BUS_ATOMIC_OP		0x133	/* Atomic op not supported by slave */
#define	EXC_TYPE_PT_BUS_ADDR_ERR	0x143	/* Bus decode/address error, page table walk */
#define	EXC_TYPE_PT_CASTOUT_ADDR	0x243	/* Castout decode/address write bus error, page table walk */
#define	EXC_TYPE_PT_BUS_DATA_ERR	0x153	/* Bus slave/data error, page table walk */
#define	EXC_TYPE_PT_CASTOUT_DATA	0x253	/* Castout slave/data write bus error, page table walk */
#define	EXC_TYPE_PT_BUS_TIMEOUT		0x163	/* Bus timeout, page table walk */
#define	EXC_TYPE_PT_CASTOUT_BUS		0x263	/* Castout write - bus timeout, page table walk */

/*  Debug  */
#define	EXC_TYPE_SINGLE_STEP		0x104	/* Single-step exception */
#define	EXC_TYPE_BREAK1			0x114	/* Break1 instruction */
#define	EXC_TYPE_BREAKN			0x214	/* Break.N instruction */
#define	EXC_TYPE_BREAK			0x314	/* Break instruction */
#define	EXC_TYPE_IBREAK			0x044	/* IBreak match */
#define	EXC_TYPE_DBREAK			0x054	/* DBreak match */
#define	EXC_TYPE_STACK_LIMIT		0x064	/* Stack limit exceeded */

/*  Syscall */
#define	EXC_TYPE_SYSCALL		0x005	/* Syscall exception (bits 11:8 -> imm4) */

/*  HW failures  */
#define	EXC_TYPE_TAG_READ		0x146	/* Uncorrectable tag read exception */
#define	EXC_TYPE_CLEAN_DATA_READ	0x246	/* Uncorrectable clean data read */
#define	EXC_TYPE_DATA_READ		0x346	/* Uncorrectable data read */
#define	EXC_TYPE_CASTOUT_UADDR		0x446	/* Uncorrectable castout at unknown address */
#define	EXC_TYPE_CASTOUT_ADDR		0x546	/* Uncorrectable castout at known address */
#define	EXC_TYPE_BUS_ERROR		0x156	/* Uncorrectable bus error */

/*  Memory management  */
#define	EXC_TYPE_ACCESS_VIOLATION_1	0x107	/* Access prohibited, subtype 1 */
#define	EXC_TYPE_ACCESS_VIOLATION_2	0x207	/* Access prohibited, subtype 2 */
#define	EXC_TYPE_ACCESS_VIOLATION_3	0x307	/* Access prohibited, subtype 3 */
#define	EXC_TYPE_UNDEFINED_ATTR_1	0x117	/* Undefined attribute, subtype 1 */
#define	EXC_TYPE_UNDEFINED_ATTR_2	0x217	/* Undefined attribute, subtype 2 */
#define	EXC_TYPE_UNDEFINED_ATTR_3	0x317	/* Undefined attribute, subtype 3 */
#define	EXC_TYPE_TLB_CONFLICT		0x127	/* TLB conflict (e.g. multihit) */

/*  PS register fields  */
#define	PS_DI_SHIFT			3
#define	PS_DI_BITS			1
#define	PS_DI_MASK			0x00000008
#define	PS_DI				PS_DI_MASK
#define	PS_RING_SHIFT			4
#define	PS_RING_BITS			1
#define	PS_RING_MASK			0x00000010
#define	PS_RING				PS_RING_MASK
#define	PS_STACK_SHIFT			5
#define	PS_STACK_BITS			3
#define	PS_STACK_MASK			0x000000E0
#define	PS_SS_SHIFT			20
#define	PS_SS_BITS			2
#define	PS_SS_MASK			0x00300000
#define	PS_ENTRYNR_SHIFT		22
#define	PS_ENTRYNR_BITS			1
#define	PS_ENTRYNR_MASK			0x00400000
#define	PS_ENTRYNR			PS_ENTRYNR_MASK

/*  Exception Cause/Type are saved in PS on exception  */
#define	PS_EXCCAUSE_SHIFT		12
#define	PS_EXCCAUSE_BITS		4
#define	PS_EXCCAUSE_MASK		0x0000F000
#define	PS_EXCTYPE_SHIFT		16
#define	PS_EXCTYPE_BITS			4
#define	PS_EXCTYPE_MASK			0x000F0000

/*  PS.STACK values */
#define	PS_STACK_INTERRUPT		0x00000000
#define	PS_STACK_CROSS			0x00000020
#define	PS_STACK_IDLE			0x00000040
#define	PS_STACK_KERNEL			0x00000060
#define	PS_STACK_PAGE			0x000000E0
#define	PS_STACK_FIRSTINT		0x00000080
#define	PS_STACK_FIRSTKER		0x000000A0

/*  PS backward compatibility  */
#define	PS_WOE_ABI			0
#define	PS_UM				0

/*  MS (machine state) register fields  */
#define MS_DISPST_SHIFT			0
#define MS_DISPST_MASK			0x0000001F
#define MS_DE_SHIFT			5
#define MS_DE				0x00000020

/*  MESR register fields:  */
#define MESR_PEND		0x00000003
#define MESR_PEND_SHIFT		0
#define MESR_LOSTI		0x00000010
#define MESR_LOSTI_SHIFT	4
#define MESR_INTPEND		0x00000040
#define MESR_INTPEND_SHIFT	6
#define MESR_INTLOST		0x00000080
#define MESR_INTLOST_SHIFT	7
#define MESR_ERRENAB		0x00000100
#define MESR_ERRENAB_SHIFT	8
#define MESR_ERRTEST		0x00000200
#define MESR_ERRTEST_SHIFT	9
#define MESR_WAYNUM_SHIFT	16
#define MESR_ACCTYPE_SHIFT	20
#define MESR_MEMTYPE_SHIFT	24
#define MESR_ERRTYPE_SHIFT	29

#else /* !XEA3 */

/*  EXCCAUSE register fields:  */
#define EXCCAUSE_EXCCAUSE_SHIFT		0
#define EXCCAUSE_EXCCAUSE_MASK		0x3F

/*  EXCCAUSE register values:  */
/*
 *  General Exception Causes
 *  (values of EXCCAUSE special register set by general exceptions,
 *   which vector to the user, kernel, or double-exception vectors).
 */
#define EXCCAUSE_ILLEGAL		0	/* Illegal Instruction */
#define EXCCAUSE_SYSCALL		1	/* System Call (SYSCALL instruction) */
#define EXCCAUSE_INSTR_ERROR		2	/* Instruction Fetch Error */
# define EXCCAUSE_IFETCHERROR		2	/* (backward compatibility macro, deprecated, avoid) */
#define EXCCAUSE_LOAD_STORE_ERROR	3	/* Load Store Error */
# define EXCCAUSE_LOADSTOREERROR	3	/* (backward compatibility macro, deprecated, avoid) */
#define EXCCAUSE_LEVEL1_INTERRUPT	4	/* Level 1 Interrupt */
#define EXCCAUSE_LEVEL1INTERRUPT	4	/* DEPRECATED but keep for backward compatibility */
#define EXCCAUSE_ALLOCA			5	/* Stack Extension Assist (MOVSP instruction) for alloca */
#define EXCCAUSE_DIVIDE_BY_ZERO		6	/* Integer Divide by Zero */
#define EXCCAUSE_PC_ERROR		7	/* Next PC Value Illegal */
#define EXCCAUSE_PRIVILEGED		8	/* Privileged Instruction */
#define EXCCAUSE_UNALIGNED		9	/* Unaligned Load or Store */
#define EXCCAUSE_EXTREG_PRIVILEGE	10	/* External Register Privilege Error */
#define EXCCAUSE_EXCLUSIVE_ERROR	11	/* Load exclusive to unsupported memory type or unaligned address */
#define EXCCAUSE_INSTR_DATA_ERROR	12	/* PIF Data Error on Instruction Fetch (RB-200x and later) */
#define EXCCAUSE_LOAD_STORE_DATA_ERROR	13	/* PIF Data Error on Load or Store (RB-200x and later) */
#define EXCCAUSE_INSTR_ADDR_ERROR	14	/* PIF Address Error on Instruction Fetch (RB-200x and later) */
#define EXCCAUSE_LOAD_STORE_ADDR_ERROR	15	/* PIF Address Error on Load or Store (RB-200x and later) */
#define EXCCAUSE_ITLB_MISS		16	/* ITLB Miss (no ITLB entry matches, hw refill also missed) */
#define EXCCAUSE_ITLB_MULTIHIT		17	/* ITLB Multihit (multiple ITLB entries match) */
#define EXCCAUSE_INSTR_RING		18	/* Ring Privilege Violation on Instruction Fetch */
/* Reserved				19 */	/* Size Restriction on IFetch (not implemented) */
#define EXCCAUSE_INSTR_PROHIBITED	20	/* Cache Attribute does not allow Instruction Fetch */
/* Reserved				21..23 */
#define EXCCAUSE_DTLB_MISS		24	/* DTLB Miss (no DTLB entry matches, hw refill also missed) */
#define EXCCAUSE_DTLB_MULTIHIT		25	/* DTLB Multihit (multiple DTLB entries match) */
#define EXCCAUSE_LOAD_STORE_RING	26	/* Ring Privilege Violation on Load or Store */
/* Reserved				27 */	/* Size Restriction on Load/Store (not implemented) */
#define EXCCAUSE_LOAD_PROHIBITED	28	/* Cache Attribute does not allow Load */
#define EXCCAUSE_STORE_PROHIBITED	29	/* Cache Attribute does not allow Store */
/* Reserved				30..31 */
#define EXCCAUSE_CP_DISABLED(n)		(32+(n))	/* Access to Coprocessor 'n' when disabled */
#define EXCCAUSE_CP0_DISABLED		32	/* Access to Coprocessor 0 when disabled */
#define EXCCAUSE_CP1_DISABLED		33	/* Access to Coprocessor 1 when disabled */
#define EXCCAUSE_CP2_DISABLED		34	/* Access to Coprocessor 2 when disabled */
#define EXCCAUSE_CP3_DISABLED		35	/* Access to Coprocessor 3 when disabled */
#define EXCCAUSE_CP4_DISABLED		36	/* Access to Coprocessor 4 when disabled */
#define EXCCAUSE_CP5_DISABLED		37	/* Access to Coprocessor 5 when disabled */
#define EXCCAUSE_CP6_DISABLED		38	/* Access to Coprocessor 6 when disabled */
#define EXCCAUSE_CP7_DISABLED		39	/* Access to Coprocessor 7 when disabled */
/* Reserved				40..63 */

/*  PS register fields:  */
#define PS_PNSM_SHIFT		23
#define PS_PNSM_BITS		1
#define PS_PNSM_MASK		0x00800000
#define PS_PNSM			PS_PNSM_MASK
#define PS_WOE_SHIFT		18
#define PS_WOE_BITS		1
#define PS_WOE_MASK		0x00040000
#define PS_WOE			PS_WOE_MASK
#define PS_CALLINC_SHIFT	16
#define PS_CALLINC_BITS		2
#define PS_CALLINC_MASK		0x00030000
#define PS_CALLINC(n)		(((n)&3)<<PS_CALLINC_SHIFT)	/* n = 0..3 */
#define PS_NSM_SHIFT		12
#define PS_NSM_BITS		1
#define PS_NSM_MASK		0x00001000
#define PS_NSM			PS_NSM_MASK
#define PS_OWB_SHIFT		8
#define PS_OWB_BITS		4
#define PS_OWB_MASK		0x00000F00
#define PS_OWB(n)		(((n)&15)<<PS_OWB_SHIFT)	/* n = 0..15 (or 0..7) */
#define PS_RING_SHIFT		6
#define PS_RING_BITS		2
#define PS_RING_MASK		0x000000C0
#define PS_RING(n)		(((n)&3)<<PS_RING_SHIFT)	/* n = 0..3 */
#define PS_UM_SHIFT		5
#define PS_UM_BITS		1
#define PS_UM_MASK		0x00000020
#define PS_UM			PS_UM_MASK
#define PS_EXCM_SHIFT		4
#define PS_EXCM_BITS		1
#define PS_EXCM_MASK		0x00000010
#define PS_EXCM			PS_EXCM_MASK
#define PS_INTLEVEL_SHIFT	0
#define PS_INTLEVEL_BITS	4
#define PS_INTLEVEL_MASK	0x0000000F
#define PS_INTLEVEL(n)		((n)&PS_INTLEVEL_MASK)		/* n = 0..15 */
/*  ABI-derived field values:  */
#ifdef __XTENSA_CALL0_ABI__
#define PS_WOE_ABI		0
#define PS_WOECALL4_ABI		0
#else
#define PS_WOE_ABI		PS_WOE				/* 0x40000 */
#define PS_WOECALL4_ABI		(PS_WOE | PS_CALLINC(1))	/* 0x50000, per call4 */
#endif
/*  Backward compatibility (deprecated):  */
#define PS_PROGSTACK_SHIFT	PS_UM_SHIFT
#define PS_PROGSTACK_MASK	PS_UM_MASK
#define PS_PROG_SHIFT		PS_UM_SHIFT
#define PS_PROG_MASK		PS_UM_MASK
#define PS_PROG			PS_UM

/*  DBREAKCn register fields:  */
#define DBREAKC_MASK_SHIFT		0
#define DBREAKC_MASK_BITS		6
#define DBREAKC_MASK_MASK		0x0000003F
#define DBREAKC_LOADBREAK_SHIFT		30
#define DBREAKC_LOADBREAK_BITS		1
#define DBREAKC_LOADBREAK_MASK		0x40000000
#define DBREAKC_STOREBREAK_SHIFT	31
#define DBREAKC_STOREBREAK_BITS		1
#define DBREAKC_STOREBREAK_MASK		0x80000000

/*  DEBUGCAUSE register fields:  */
#define DEBUGCAUSE_DEBUGINT_SHIFT	5
#define DEBUGCAUSE_DEBUGINT_MASK	0x20	/* debug interrupt */
#define DEBUGCAUSE_BREAKN_SHIFT		4
#define DEBUGCAUSE_BREAKN_MASK		0x10	/* BREAK.N instruction */
#define DEBUGCAUSE_BREAK_SHIFT		3
#define DEBUGCAUSE_BREAK_MASK		0x08	/* BREAK instruction */
#define DEBUGCAUSE_DBREAK_SHIFT		2
#define DEBUGCAUSE_DBREAK_MASK		0x04	/* DBREAK match */
#define DEBUGCAUSE_IBREAK_SHIFT		1
#define DEBUGCAUSE_IBREAK_MASK		0x02	/* IBREAK match */
#define DEBUGCAUSE_ICOUNT_SHIFT		0
#define DEBUGCAUSE_ICOUNT_MASK		0x01	/* ICOUNT would increment to zero */

/*  MESR register fields:  */
#define MESR_MEME		0x00000001	/* memory error */
#define MESR_MEME_SHIFT		0
#define MESR_DME		0x00000002	/* double memory error */
#define MESR_DME_SHIFT		1
#define MESR_RCE		0x00000010	/* recorded memory error */
#define MESR_RCE_SHIFT		4
#define MESR_DLCE		0x00000020
#define MESR_DLCE_SHIFT		5
#define MESR_ILCE		0x00000040
#define MESR_ILCE_SHIFT		6
#define MESR_ERRENAB		0x00000100
#define MESR_ERRENAB_SHIFT	8
#define MESR_ERRTEST		0x00000200
#define MESR_ERRTEST_SHIFT	9
#define MESR_DATEXC		0x00000400
#define MESR_DATEXC_SHIFT	10
#define MESR_INSEXC		0x00000800
#define MESR_INSEXC_SHIFT	11
#define MESR_WAYNUM_SHIFT	16
#define MESR_ACCTYPE_SHIFT	20
#define MESR_MEMTYPE_SHIFT	24
#define MESR_ERRTYPE_SHIFT	30

#endif	/* XCHAL_HAVE_XEA3 */

/*  MEMCTL register fields:  */
#define MEMCTL_L0IBUF_EN_SHIFT	0
#define MEMCTL_L0IBUF_EN	0x00000001	/* enable loop instr. buffer (default 1) */
#define MEMCTL_SNOOP_EN_SHIFT	1
#define MEMCTL_SNOOP_EN		0x00000002	/* enable data snoop responses (default 0) */
#define MEMCTL_ISNOOP_EN_SHIFT	2
#define MEMCTL_ISNOOP_EN	0x00000004	/* enable instruction snoop (default 0) */
#define MEMCTL_BP_EN_SHIFT	3
#define MEMCTL_BP_EN		0x00000008	/* enable branch prediction (default 1) */
#define MEMCTL_DCWU_SHIFT	8
#define MEMCTL_DCWU_BITS	5
#define MEMCTL_DCWU_MASK	0x00001F00	/* Bits  8-12 dcache ways in use */
#define MEMCTL_DCWA_SHIFT	13
#define MEMCTL_DCWA_BITS	5
#define MEMCTL_DCWA_MASK	0x0003E000	/* Bits 13-17 dcache ways allocatable */
#define MEMCTL_ICWU_SHIFT	18
#define MEMCTL_ICWU_BITS	5
#define MEMCTL_DCWU_MASK	0x00001F00	/* Bits  8-12 dcache ways in use */
#define MEMCTL_DCWA_MASK	0x0003E000	/* Bits 13-17 dcache ways allocatable */
#define MEMCTL_ICWU_MASK	0x007C0000	/* Bits 18-22 icache ways in use */
#define MEMCTL_INV_EN_SHIFT	23
#define MEMCTL_INV_EN		0x00800000	/* invalidate cache ways being increased */

#define VECBASE_LOCK		0x1

#define CSRPARITY_PARITY_MASK           0x100   /* Used in test mode as data for parity*/
#define CSRPARITY_TEST_MODE_MASK        0x2     /* When set, CSR Parity Test Mode is enabled */
#define CSRPARITY_ENABLED               0x1     /* When Enable is clear, CSR parity errors are enabled */

/* SPLIT LOCK */
#define XTHAL_REG_STATUS_CMP_ENABLE             0x20
#define XTHAL_REG_STATUS_LOCK_MODE_ENABLE       0x10
#define XTHAL_REG_STATUS_FAULT_INJECTED         0x08
#define XTHAL_REG_STATUS_SEC_CMP_ASSERTED       0x04
#define XTHAL_REG_STATUS_PRI_CMP_ASSERTED       0x02
#define XTHAL_REG_STATUS_FAULT_ASSERTED         0x1
#define XTHAL_REG_ERRINJ_EKY                    0xfff00000
#define XTHAL_REG_ERRINJ_CORE                   0x00010000
#define XTHAL_REG_ERRINJ_BIT_POS_MASK           0x00003fff
#define XTHAL_REG_CLEAR_KEY                     0xfff00000
#define XTHAL_REG_RESET_KEY                     0xfff00000
#define XTHAL_REG_RESET_VALUE                   0x000fffff
#define XTHAL_REG_RESET_MASK                    0x000fffff
#define XTHAL_REG_RC_COUNTER_MASK               0x000fffff
#define XTHAL_REG_CMP_EN_TOGGLE_KEY             0xfff00000

#define XTHAL_FXLK_KEY_SHIFT                    20

/* FlexLock related external register numbers */
#define XTHAL_FXLK_STATUS_REGNO                 0x00100C00
#define XTHAL_FXLK_NUM_OUTPUTS_REGNO            0x00100C04
#define XTHAL_FXLK_ERROR_INJECTION_REGNO        0x00100C08
#define XTHAL_FXLK_CLEAR_REGNO                  0x00100C0C
#define XTHAL_FXLK_RESET_COUNTDOWN_REGNO        0x00100C10
#define XTHAL_FXLK_RC_COUNTER_REGNO             0x00100C14
#define XTHAL_FXLK_CMP_EN_TOGGLE_REGNO          0x00100C18
#define XTHAL_FXLK_NUM_SUB_REGNO                0x00100C1C
#define XTHAL_FXLK_NUM_MEM_REGNO                0x00100C20
#define XTHAL_FXLK_NUM_MLS_REGNO                0x00100C24
#define XTHAL_FXLK_NUM_TOP_REGNO                0x00100C28


/*      WWDT
 *
 */
/* External Register Numbers */
#define XTHAL_WWDT_HEARTBEAT                    0x00100800
#define XTHAL_WWDT_INITIAL                      0x00100804
#define XTHAL_WWDT_BOUND                        0x00100808
#define XTHAL_WWDT_WD_COUNTDOWN                 0x0010080C
#define XTHAL_WWDT_HB_COUNTDOWN                 0x00100810
#define XTHAL_WWDT_STATUS                       0x00100814
#define XTHAL_WWDT_RESET_COUNTDOWN              0x00100818
#define XTHAL_WWDT_KICK                         0x0010081C
#define XTHAL_WWDT_ERROR_INJECTION              0x00100820
#define XTHAL_WWDT_CLEAR                        0x00100824
#define XTHAL_WWDT_RESET_COUNTDOWN_COUNT        0x00100828
#define XTHAL_WWDT_DERR_INJ_DIS_TOGGLE          0x0010082C

#define XTHAL_WWDT_KEYSHIFT                 20

#define XTHAL_WWDT_BOUND_MASK           0x000FFFFF
#define XTHAL_WWDT_BOUND_SHIFT          12
#define XTHAL_WWDT_INITIAL_MASK         0x000FFFFF
#define XTHAL_WWDT_INITIAL_SHIFT        12
#define XTHAL_WWDT_RESET_COUNTDOWN_MASK 0x0000FFFF

#define XTHAL_OPMODE_RMW_FENCE_CUT              0x1
#define XTHAL_OPMODE_RMW_FENCE_CUT_SHIFT        0x0
#define XTHAL_OPMODE_EXCL_FENCE_CUT             0x2
#define XTHAL_OPMODE_EXCL_FENCE_CUT_SHIFT       0x1


#endif
