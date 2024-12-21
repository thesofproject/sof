/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright(c) 2024 MediaTek. All rights reserved.
 *
 * Author: Andrew Perepech <andrew.perepech@mediatek.com>
 */

#ifndef MT_REG_BASE_H
#define MT_REG_BASE_H

#define DSP_REG_BASE                    (0x1D062000) /* DSPCFG base */
#define DSP_TIMER_BASE                  (0x1D060000)
#define DSP_UART0_BASE                  (0x1D061000)
#define DSP_WDT_BASE                    (0x1D062400)
#define DSP_IRQ_BASE                    (0x1D063000)
#define DSP_D_TCM                       (CFG_HIFI4_DTCM_ADDRESS)
#define DSP_I_TCM                       (CFG_HIFI4_ITCM_ADDRESS)
#define DSP_DRAM_BASE                   (CFG_HIFI4_DRAM_ADDRESS)

#define DSP_AUDIO_SRAM_BASE             (0x11221000)
#define DSP_AUDIO_SRAM_SIZE             (0xA000)
#define DSP_REG_REMAP_BASE              (0x1D060000)
#define DSP_REG_REMAP_SIZE              (0x8000)
#define DSP_SYS_REG_BASE                (0x10000000)
#define DSP_SYS_REG_SIZE                (0x1221000)
#define DSP_VER_REG_BASE                (0x08000000)
#define DSP_VER_REG_SIZE                (0x1000)


#define DSP_JTAGMUX          (DSP_REG_BASE + 0x0000)
#define DSP_ALTRESETVEC      (DSP_REG_BASE + 0x0004)
#define DSP_PDEBUGDATA       (DSP_REG_BASE + 0x0008)
#define DSP_PDEBUGBUS0       (DSP_REG_BASE + 0x000c)
#define DSP_PDEBUGBUS1       (DSP_REG_BASE + 0x0010)
#define DSP_PDEBUGINST       (DSP_REG_BASE + 0x0014)
#define DSP_PDEBUGLS0STAT    (DSP_REG_BASE + 0x0018)
#define DSP_PDEBUGLS1STAT    (DSP_REG_BASE + 0x001c)
#define DSP_PDEBUGPC         (DSP_REG_BASE + 0x0020)
#define DSP_RESET_SW         (DSP_REG_BASE + 0x0024)
#define DSP_PFAULTBUS        (DSP_REG_BASE + 0x0028)
#define DSP_PFAULTINFO       (DSP_REG_BASE + 0x002c)
#define DSP_GPR00            (DSP_REG_BASE + 0x0030)
#define DSP_GPR01            (DSP_REG_BASE + 0x0034)
#define DSP_GPR02            (DSP_REG_BASE + 0x0038)
#define DSP_GPR03            (DSP_REG_BASE + 0x003c)
#define DSP_GPR04            (DSP_REG_BASE + 0x0040)
#define DSP_GPR05            (DSP_REG_BASE + 0x0044)
#define DSP_GPR06            (DSP_REG_BASE + 0x0048)
#define DSP_GPR07            (DSP_REG_BASE + 0x004c)
#define DSP_GPR08            (DSP_REG_BASE + 0x0050)
#define DSP_GPR09            (DSP_REG_BASE + 0x0054)
#define DSP_GPR0A            (DSP_REG_BASE + 0x0058)
#define DSP_GPR0B            (DSP_REG_BASE + 0x005c)
#define DSP_GPR0C            (DSP_REG_BASE + 0x0060)
#define DSP_GPR0D            (DSP_REG_BASE + 0x0064)
#define DSP_GPR0E            (DSP_REG_BASE + 0x0068)
#define DSP_GPR0F            (DSP_REG_BASE + 0x006c)
#define DSP_GPR10            (DSP_REG_BASE + 0x0070)
#define DSP_GPR11            (DSP_REG_BASE + 0x0074)
#define DSP_GPR12            (DSP_REG_BASE + 0x0078)
#define DSP_GPR13            (DSP_REG_BASE + 0x007c)
#define DSP_GPR14            (DSP_REG_BASE + 0x0080)
#define DSP_GPR15            (DSP_REG_BASE + 0x0084)
#define DSP_GPR16            (DSP_REG_BASE + 0x0088)
#define DSP_GPR17            (DSP_REG_BASE + 0x008c)
#define DSP_GPR18            (DSP_REG_BASE + 0x0090)
#define DSP_GPR19            (DSP_REG_BASE + 0x0094)
#define DSP_GPR1A            (DSP_REG_BASE + 0x0098)
#define DSP_GPR1B            (DSP_REG_BASE + 0x009c)
#define DSP_GPR1C            (DSP_REG_BASE + 0x00a0)
#define DSP_GPR1D            (DSP_REG_BASE + 0x00a4)
#define DSP_GPR1E            (DSP_REG_BASE + 0x00a8)
#define DSP_GPR1F            (DSP_REG_BASE + 0x00ac)
#define DSP_TCM_OFFSET       (DSP_REG_BASE + 0x00b0)    /* not used */
#define DSP_DDR_OFFSET       (DSP_REG_BASE + 0x00b4)    /* not used */
#define DSP_INTFDSP          (DSP_REG_BASE + 0x00d0)
#define DSP_INTFDSP_CLR      (DSP_REG_BASE + 0x00d4)
#define DSP_SRAM_PD_SW1      (DSP_REG_BASE + 0x00d8)
#define DSP_SRAM_PD_SW2      (DSP_REG_BASE + 0x00dc)
#define DSP_OCD              (DSP_REG_BASE + 0x00e0)
#define DSP_RG_DSP_IRQ_POL   (DSP_REG_BASE + 0x00f0)    /* not used */
#define DSP_DSP_IRQ_EN       (DSP_REG_BASE + 0x00f4)    /* not used */
#define DSP_DSP_IRQ_LEVEL    (DSP_REG_BASE + 0x00f8)    /* not used */
#define DSP_DSP_IRQ_STATUS   (DSP_REG_BASE + 0x00fc)    /* not used */
#define DSP_RG_INT2CIRQ      (DSP_REG_BASE + 0x0114)
#define DSP_RG_INT_POL_CTL0  (DSP_REG_BASE + 0x0120)
#define DSP_RG_INT_EN_CTL0   (DSP_REG_BASE + 0x0130)
#define DSP_RG_INT_LV_CTL0   (DSP_REG_BASE + 0x0140)
#define DSP_RG_INT_STATUS0   (DSP_REG_BASE + 0x0150)
#define DSP_PDEBUGSTATUS0    (DSP_REG_BASE + 0x0200)
#define DSP_PDEBUGSTATUS1    (DSP_REG_BASE + 0x0204)
#define DSP_PDEBUGSTATUS2    (DSP_REG_BASE + 0x0208)
#define DSP_PDEBUGSTATUS3    (DSP_REG_BASE + 0x020c)
#define DSP_PDEBUGSTATUS4    (DSP_REG_BASE + 0x0210)
#define DSP_PDEBUGSTATUS5    (DSP_REG_BASE + 0x0214)
#define DSP_PDEBUGSTATUS6    (DSP_REG_BASE + 0x0218)
#define DSP_PDEBUGSTATUS7    (DSP_REG_BASE + 0x021c)
#define DSP_DSP2PSRAM_PRIORITY           (DSP_REG_BASE + 0x0220)  /* not used */
#define DSP_AUDIO_DSP2SPM_INT            (DSP_REG_BASE + 0x0224)
#define DSP_AUDIO_DSP2SPM_INT_ACK        (DSP_REG_BASE + 0x0228)
#define DSP_AUDIO_DSP_DEBUG_SEL          (DSP_REG_BASE + 0x022C)
#define DSP_AUDIO_DSP_EMI_BASE_ADDR      (DSP_REG_BASE + 0x02E0)  /* not used */
#define DSP_AUDIO_DSP_SHARED_IRAM        (DSP_REG_BASE + 0x02E4)
#define DSP_AUDIO_DSP_CKCTRL_P2P_CK_CON  (DSP_REG_BASE + 0x02F0)
#define DSP_RG_SEMAPHORE00   (DSP_REG_BASE + 0x0300)
#define DSP_RG_SEMAPHORE01   (DSP_REG_BASE + 0x0304)
#define DSP_RG_SEMAPHORE02   (DSP_REG_BASE + 0x0308)
#define DSP_RG_SEMAPHORE03   (DSP_REG_BASE + 0x030C)
#define DSP_RG_SEMAPHORE04   (DSP_REG_BASE + 0x0310)
#define DSP_RG_SEMAPHORE05   (DSP_REG_BASE + 0x0314)
#define DSP_RG_SEMAPHORE06   (DSP_REG_BASE + 0x0318)
#define DSP_RG_SEMAPHORE07   (DSP_REG_BASE + 0x031C)
#define DSP_RESERVED_0       (DSP_REG_BASE + 0x03F0)
#define DSP_RESERVED_1       (DSP_REG_BASE + 0x03F4)  /* use for tickless status */

/* Redefinition for using Special registers */
#define TICKLESS_STATUS_REG  (DSP_RESERVED_1)

/* WDT CONFIGS */
#define ADSP_WDT_MODE			(DSP_REG_BASE + 0x400 + 0x00)
#define ADSP_WDT_LENGTH			(DSP_REG_BASE + 0x400 + 0x04)
#define ADSP_WDT_RESTART		(DSP_REG_BASE + 0x400 + 0x08)
#define ADSP_WDT_STA			(DSP_REG_BASE + 0x400 + 0x0C)
#define ADSP_WDT_SWRST			(DSP_REG_BASE + 0x400 + 0x14)

#define ADSP_WDT_SWRST_KEY		0x1209
#define ADSP_WDT_RESTART_RELOAD		0x1971
#define ADSP_WDT_LENGTH_KEY		0x8

#define WDT_LENGTH_TIMEOUT(n)	((n) << 5)

/* DSP IPI IRQ */
#define CPU2DSP_IRQ		BIT(0)
#define DSP2CPU_IRQ		BIT(1)
#define DSP2SPM_IRQ_B		BIT(2)

#endif /* MT_REG_BASE_H */

