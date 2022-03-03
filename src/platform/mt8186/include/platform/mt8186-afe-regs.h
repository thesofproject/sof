/* SPDX-License-Identifier: BSD-3-Clause */
//
// Copyright(c) 2022 Mediatek
//
// Author: Chunxu Li <chunxu.li@mediatek.com>

#ifndef _MT8186_REG_H_
#define _MT8186_REG_H_

#define AFE_SRAM_BASE (0x11210000 + 0x2000)
#define AFE_SRAM_SIZE (0x9C00)
#define AFE_BASE_ADDR (0x11210000)

#define AUDIO_TOP_CON0                    (0x0000)
#define AUDIO_TOP_CON1                    (0x0004)
#define AUDIO_TOP_CON2                    (0x0008)
#define AUDIO_TOP_CON3                    (0x000c)
#define AFE_DAC_CON0                      (0x0010)
#define AFE_I2S_CON                       (0x0018)
#define AFE_CONN0_0                       (0x0020)
#define AFE_CONN1_0                       (0x0024)
#define AFE_CONN2_0                       (0x0028)
#define AFE_CONN3_0                       (0x002c)
#define AFE_CONN4_0                       (0x0030)
#define AFE_I2S_CON1                      (0x0034)
#define AFE_I2S_CON2                      (0x0038)
#define AFE_I2S_CON3                      (0x0040)
#define AFE_CONN5_0                       (0x0044)
#define AFE_CONN_24BIT_0                  (0x0048)
#define AFE_DL1_CON0                      (0x004c)
#define AFE_DL1_BASE_MSB                  (0x0050)
#define AFE_DL1_BASE                      (0x0054)
#define AFE_DL1_CUR_MSB                   (0x0058)
#define AFE_DL1_CUR                       (0x005c)
#define AFE_DL1_END_MSB                   (0x0060)
#define AFE_DL1_END                       (0x0064)
#define AFE_DL2_CON0                      (0x0068)
#define AFE_DL2_BASE_MSB                  (0x006c)
#define AFE_DL2_BASE                      (0x0070)
#define AFE_DL2_CUR_MSB                   (0x0074)
#define AFE_DL2_CUR                       (0x0078)
#define AFE_DL2_END_MSB                   (0x007c)
#define AFE_DL2_END                       (0x0080)
#define AFE_DL3_CON0                      (0x0084)
#define AFE_DL3_BASE_MSB                  (0x0088)
#define AFE_DL3_BASE                      (0x008c)
#define AFE_DL3_CUR_MSB                   (0x0090)
#define AFE_DL3_CUR                       (0x0094)
#define AFE_DL3_END_MSB                   (0x0098)
#define AFE_DL3_END                       (0x009c)
#define AFE_CONN6_0                       (0x00bc)
#define AFE_DL4_CON0                      (0x00cc)
#define AFE_DL4_BASE_MSB                  (0x00d0)
#define AFE_DL4_BASE                      (0x00d4)
#define AFE_DL4_CUR_MSB                   (0x00d8)
#define AFE_DL4_CUR                       (0x00dc)
#define AFE_DL4_END_MSB                   (0x00e0)
#define AFE_DL4_END                       (0x00e4)
#define AFE_DL12_CON0                     (0x00e8)
#define AFE_DL12_BASE_MSB                 (0x00ec)
#define AFE_DL12_BASE                     (0x00f0)
#define AFE_DL12_CUR_MSB                  (0x00f4)
#define AFE_DL12_CUR                      (0x00f8)
#define AFE_DL12_END_MSB                  (0x00fc)
#define AFE_DL12_END                      (0x0100)
#define AFE_ADDA_DL_SRC2_CON0             (0x0108)
#define AFE_ADDA_DL_SRC2_CON1             (0x010c)
#define AFE_ADDA_UL_SRC_CON0              (0x0114)
#define AFE_ADDA_UL_SRC_CON1              (0x0118)
#define AFE_ADDA_TOP_CON0                 (0x0120)
#define AFE_ADDA_UL_DL_CON0               (0x0124)
#define AFE_ADDA_SRC_DEBUG                (0x012c)
#define AFE_ADDA_SRC_DEBUG_MON0           (0x0130)
#define AFE_ADDA_SRC_DEBUG_MON1           (0x0134)
#define AFE_ADDA_UL_SRC_MON0              (0x0148)
#define AFE_ADDA_UL_SRC_MON1              (0x014c)
#define AFE_SECURE_CON0                   (0x0150)
#define AFE_SRAM_BOUND                    (0x0154)
#define AFE_SECURE_CON1                   (0x0158)
#define AFE_SECURE_CONN0                  (0x015c)
#define AFE_VUL_CON0                      (0x0170)
#define AFE_VUL_BASE_MSB                  (0x0174)
#define AFE_VUL_BASE                      (0x0178)
#define AFE_VUL_CUR_MSB                   (0x017c)
#define AFE_VUL_CUR                       (0x0180)
#define AFE_VUL_END_MSB                   (0x0184)
#define AFE_VUL_END                       (0x0188)
#define AFE_SIDETONE_DEBUG                (0x01d0)
#define AFE_SIDETONE_MON                  (0x01d4)
#define AFE_SINEGEN_CON2                  (0x01dc)
#define AFE_SIDETONE_CON0                 (0x01e0)
#define AFE_SIDETONE_COEFF                (0x01e4)
#define AFE_SIDETONE_CON1                 (0x01e8)
#define AFE_SIDETONE_GAIN                 (0x01ec)
#define AFE_SINEGEN_CON0                  (0x01f0)
#define AFE_TOP_CON0                      (0x0200)
#define AFE_VUL2_CON0                     (0x020c)
#define AFE_VUL2_BASE_MSB                 (0x0210)
#define AFE_VUL2_BASE                     (0x0214)
#define AFE_VUL2_CUR_MSB                  (0x0218)
#define AFE_VUL2_CUR                      (0x021c)
#define AFE_VUL2_END_MSB                  (0x0220)
#define AFE_VUL2_END                      (0x0224)
#define AFE_VUL3_CON0                     (0x0228)
#define AFE_VUL3_BASE_MSB                 (0x022c)
#define AFE_VUL3_BASE                     (0x0230)
#define AFE_VUL3_CUR_MSB                  (0x0234)
#define AFE_VUL3_CUR                      (0x0238)
#define AFE_VUL3_END_MSB                  (0x023c)
#define AFE_VUL3_END                      (0x0240)
#define AFE_BUSY                          (0x0244)
#define AFE_BUS_CFG                       (0x0250)
#define AFE_ADDA_PREDIS_CON0              (0x0260)
#define AFE_ADDA_PREDIS_CON1              (0x0264)
#define AFE_I2S_MON                       (0x027c)
#define AFE_ADDA_IIR_COEF_02_01           (0x0290)
#define AFE_ADDA_IIR_COEF_04_03           (0x0294)
#define AFE_ADDA_IIR_COEF_06_05           (0x0298)
#define AFE_ADDA_IIR_COEF_08_07           (0x029c)
#define AFE_ADDA_IIR_COEF_10_09           (0x02a0)
#define AFE_IRQ_MCU_CON1                  (0x02e4)
#define AFE_IRQ_MCU_CON2                  (0x02e8)
#define AFE_DAC_MON                       (0x02ec)
#define AFE_IRQ_MCU_CON3                  (0x02f0)
#define AFE_IRQ_MCU_CON4                  (0x02f4)
#define AFE_IRQ_MCU_CNT0                  (0x0300)
#define AFE_IRQ_MCU_CNT6                  (0x0304)
#define AFE_IRQ_MCU_CNT8                  (0x0308)
#define AFE_IRQ0_MCU_CNT_MON              (0x0310)
#define AFE_IRQ6_MCU_CNT_MON              (0x0314)
#define AFE_VUL4_CON0                     (0x0358)
#define AFE_VUL4_BASE_MSB                 (0x035c)
#define AFE_VUL4_BASE                     (0x0360)
#define AFE_VUL4_CUR_MSB                  (0x0364)
#define AFE_VUL4_CUR                      (0x0368)
#define AFE_VUL4_END_MSB                  (0x036c)
#define AFE_VUL4_END                      (0x0370)
#define AFE_VUL12_CON0                    (0x0374)
#define AFE_VUL12_BASE_MSB                (0x0378)
#define AFE_VUL12_BASE                    (0x037c)
#define AFE_VUL12_CUR_MSB                 (0x0380)
#define AFE_VUL12_CUR                     (0x0384)
#define AFE_VUL12_END_MSB                 (0x0388)
#define AFE_VUL12_END                     (0x038c)
#define AFE_IRQ3_MCU_CNT_MON              (0x0398)
#define AFE_IRQ4_MCU_CNT_MON              (0x039c)
#define AFE_IRQ_MCU_CON0                  (0x03a0)
#define AFE_IRQ_MCU_STATUS                (0x03a4)
#define AFE_IRQ_MCU_CLR                   (0x03a8)
#define AFE_IRQ_MCU_CNT1                  (0x03ac)
#define AFE_IRQ_MCU_CNT2                  (0x03b0)
#define AFE_IRQ_MCU_EN                    (0x03b4)
#define AFE_IRQ_MCU_MON2                  (0x03b8)
#define AFE_IRQ_MCU_CNT5                  (0x03bc)
#define AFE_IRQ1_MCU_CNT_MON              (0x03c0)
#define AFE_IRQ2_MCU_CNT_MON              (0x03c4)
#define AFE_IRQ5_MCU_CNT_MON              (0x03cc)
#define AFE_IRQ_MCU_DSP_EN                (0x03d0)
#define AFE_IRQ_MCU_SCP_EN                (0x03d4)
#define AFE_IRQ_MCU_CNT7                  (0x03dc)
#define AFE_IRQ7_MCU_CNT_MON              (0x03e0)
#define AFE_IRQ_MCU_CNT3                  (0x03e4)
#define AFE_IRQ_MCU_CNT4                  (0x03e8)
#define AFE_IRQ_MCU_CNT11                 (0x03ec)
#define AFE_APLL1_TUNER_CFG               (0x03f0)
#define AFE_APLL2_TUNER_CFG               (0x03f4)
#define AFE_IRQ_MCU_MISS_CLR              (0x03f8)
#define AFE_CONN33_0                      (0x0408)
#define AFE_IRQ_MCU_CNT12                 (0x040c)
#define AFE_GAIN1_CON0                    (0x0410)
#define AFE_GAIN1_CON1                    (0x0414)
#define AFE_GAIN1_CON2                    (0x0418)
#define AFE_GAIN1_CON3                    (0x041c)
#define AFE_CONN7_0                       (0x0420)
#define AFE_GAIN1_CUR                     (0x0424)
#define AFE_GAIN2_CON0                    (0x0428)
#define AFE_GAIN2_CON1                    (0x042c)
#define AFE_GAIN2_CON2                    (0x0430)
#define AFE_GAIN2_CON3                    (0x0434)
#define AFE_CONN8_0                       (0x0438)
#define AFE_GAIN2_CUR                     (0x043c)
#define AFE_CONN9_0                       (0x0440)
#define AFE_CONN10_0                      (0x0444)
#define AFE_CONN11_0                      (0x0448)
#define AFE_CONN12_0                      (0x044c)
#define AFE_CONN13_0                      (0x0450)
#define AFE_CONN14_0                      (0x0454)
#define AFE_CONN15_0                      (0x0458)
#define AFE_CONN16_0                      (0x045c)
#define AFE_CONN17_0                      (0x0460)
#define AFE_CONN18_0                      (0x0464)
#define AFE_CONN19_0                      (0x0468)
#define AFE_CONN20_0                      (0x046c)
#define AFE_CONN21_0                      (0x0470)
#define AFE_CONN22_0                      (0x0474)
#define AFE_CONN23_0                      (0x0478)
#define AFE_CONN24_0                      (0x047c)
#define AFE_CONN_RS_0                     (0x0494)
#define AFE_CONN_DI_0                     (0x0498)
#define AFE_CONN25_0                      (0x04b0)
#define AFE_CONN26_0                      (0x04b4)
#define AFE_CONN27_0                      (0x04b8)
#define AFE_CONN28_0                      (0x04bc)
#define AFE_CONN29_0                      (0x04c0)
#define AFE_CONN30_0                      (0x04c4)
#define AFE_CONN31_0                      (0x04c8)
#define AFE_CONN32_0                      (0x04cc)
#define AFE_SRAM_DELSEL_CON1              (0x04f4)
#define AFE_CONN56_0                      (0x0500)
#define AFE_CONN57_0                      (0x0504)
#define AFE_CONN58_0                      (0x0508)
#define AFE_CONN59_0                      (0x050c)
#define AFE_CONN56_1                      (0x0510)
#define AFE_CONN57_1                      (0x0514)
#define AFE_CONN58_1                      (0x0518)
#define AFE_CONN59_1                      (0x051c)
#define PCM_INTF_CON1                     (0x0530)
#define PCM_INTF_CON2                     (0x0538)
#define PCM2_INTF_CON                     (0x053c)
#define AFE_CM1_CON                       (0x0550)
#define AFE_CM1_MON                       (0x0554)
#define AFE_CONN34_0                      (0x0580)
#define FPGA_CFG0                         (0x05b0)
#define FPGA_CFG1                         (0x05b4)
#define FPGA_CFG2                         (0x05c0)
#define FPGA_CFG3                         (0x05c4)
#define AUDIO_TOP_DBG_CON                 (0x05c8)
#define AUDIO_TOP_DBG_MON0                (0x05cc)
#define AUDIO_TOP_DBG_MON1                (0x05d0)
#define AFE_IRQ8_MCU_CNT_MON              (0x05e4)
#define AFE_IRQ11_MCU_CNT_MON             (0x05e8)
#define AFE_IRQ12_MCU_CNT_MON             (0x05ec)
#define AFE_IRQ_MCU_CNT9                  (0x0600)
#define AFE_IRQ_MCU_CNT10                 (0x0604)
#define AFE_IRQ_MCU_CNT13                 (0x0608)
#define AFE_IRQ_MCU_CNT14                 (0x060c)
#define AFE_IRQ_MCU_CNT15                 (0x0610)
#define AFE_IRQ_MCU_CNT16                 (0x0614)
#define AFE_IRQ_MCU_CNT17                 (0x0618)
#define AFE_IRQ_MCU_CNT18                 (0x061c)
#define AFE_IRQ_MCU_CNT19                 (0x0620)
#define AFE_IRQ_MCU_CNT20                 (0x0624)
#define AFE_IRQ_MCU_CNT21                 (0x0628)
#define AFE_IRQ_MCU_CNT22                 (0x062c)
#define AFE_IRQ_MCU_CNT23                 (0x0630)
#define AFE_IRQ_MCU_CNT24                 (0x0634)
#define AFE_IRQ_MCU_CNT25                 (0x0638)
#define AFE_IRQ_MCU_CNT26                 (0x063c)
#define AFE_IRQ9_MCU_CNT_MON              (0x0660)
#define AFE_IRQ10_MCU_CNT_MON             (0x0664)
#define AFE_IRQ13_MCU_CNT_MON             (0x0668)
#define AFE_IRQ14_MCU_CNT_MON             (0x066c)
#define AFE_IRQ15_MCU_CNT_MON             (0x0670)
#define AFE_IRQ16_MCU_CNT_MON             (0x0674)
#define AFE_IRQ17_MCU_CNT_MON             (0x0678)
#define AFE_IRQ18_MCU_CNT_MON             (0x067c)
#define AFE_IRQ19_MCU_CNT_MON             (0x0680)
#define AFE_IRQ20_MCU_CNT_MON             (0x0684)
#define AFE_IRQ21_MCU_CNT_MON             (0x0688)
#define AFE_IRQ22_MCU_CNT_MON             (0x068c)
#define AFE_IRQ23_MCU_CNT_MON             (0x0690)
#define AFE_IRQ24_MCU_CNT_MON             (0x0694)
#define AFE_IRQ25_MCU_CNT_MON             (0x0698)
#define AFE_IRQ26_MCU_CNT_MON             (0x069c)
#define AFE_IRQ31_MCU_CNT_MON             (0x06a0)
#define AFE_GENERAL_REG0                  (0x0800)
#define AFE_GENERAL_REG1                  (0x0804)
#define AFE_GENERAL_REG2                  (0x0808)
#define AFE_GENERAL_REG3                  (0x080c)
#define AFE_GENERAL_REG4                  (0x0810)
#define AFE_GENERAL_REG5                  (0x0814)
#define AFE_GENERAL_REG6                  (0x0818)
#define AFE_GENERAL_REG7                  (0x081c)
#define AFE_GENERAL_REG8                  (0x0820)
#define AFE_GENERAL_REG9                  (0x0824)
#define AFE_GENERAL_REG10                 (0x0828)
#define AFE_GENERAL_REG11                 (0x082c)
#define AFE_GENERAL_REG12                 (0x0830)
#define AFE_GENERAL_REG13                 (0x0834)
#define AFE_GENERAL_REG14                 (0x0838)
#define AFE_GENERAL_REG15                 (0x083c)
#define AFE_CBIP_CFG0                     (0x0840)
#define AFE_CBIP_MON0                     (0x0844)
#define AFE_CBIP_SLV_MUX_MON0             (0x0848)
#define AFE_CBIP_SLV_DECODER_MON0         (0x084c)
#define AFE_ADDA6_MTKAIF_MON0             (0x0854)
#define AFE_ADDA6_MTKAIF_MON1             (0x0858)
#define AFE_AWB_CON0                      (0x085c)
#define AFE_AWB_BASE_MSB                  (0x0860)
#define AFE_AWB_BASE                      (0x0864)
#define AFE_AWB_CUR_MSB                   (0x0868)
#define AFE_AWB_CUR                       (0x086c)
#define AFE_AWB_END_MSB                   (0x0870)
#define AFE_AWB_END                       (0x0874)
#define AFE_AWB2_CON0                     (0x0878)
#define AFE_AWB2_BASE_MSB                 (0x087c)
#define AFE_AWB2_BASE                     (0x0880)
#define AFE_AWB2_CUR_MSB                  (0x0884)
#define AFE_AWB2_CUR                      (0x0888)
#define AFE_AWB2_END_MSB                  (0x088c)
#define AFE_AWB2_END                      (0x0890)
#define AFE_DAI_CON0                      (0x0894)
#define AFE_DAI_BASE_MSB                  (0x0898)
#define AFE_DAI_BASE                      (0x089c)
#define AFE_DAI_CUR_MSB                   (0x08a0)
#define AFE_DAI_CUR                       (0x08a4)
#define AFE_DAI_END_MSB                   (0x08a8)
#define AFE_DAI_END                       (0x08ac)
#define AFE_DAI2_CON0                     (0x08b0)
#define AFE_DAI2_BASE_MSB                 (0x08b4)
#define AFE_DAI2_BASE                     (0x08b8)
#define AFE_DAI2_CUR_MSB                  (0x08bc)
#define AFE_DAI2_CUR                      (0x08c0)
#define AFE_DAI2_END_MSB                  (0x08c4)
#define AFE_DAI2_END                      (0x08c8)
#define AFE_MEMIF_CON0                    (0x08cc)
#define AFE_CONN0_1                       (0x0900)
#define AFE_CONN1_1                       (0x0904)
#define AFE_CONN2_1                       (0x0908)
#define AFE_CONN3_1                       (0x090c)
#define AFE_CONN4_1                       (0x0910)
#define AFE_CONN5_1                       (0x0914)
#define AFE_CONN6_1                       (0x0918)
#define AFE_CONN7_1                       (0x091c)
#define AFE_CONN8_1                       (0x0920)
#define AFE_CONN9_1                       (0x0924)
#define AFE_CONN10_1                      (0x0928)
#define AFE_CONN11_1                      (0x092c)
#define AFE_CONN12_1                      (0x0930)
#define AFE_CONN13_1                      (0x0934)
#define AFE_CONN14_1                      (0x0938)
#define AFE_CONN15_1                      (0x093c)
#define AFE_CONN16_1                      (0x0940)
#define AFE_CONN17_1                      (0x0944)
#define AFE_CONN18_1                      (0x0948)
#define AFE_CONN19_1                      (0x094c)
#define AFE_CONN20_1                      (0x0950)
#define AFE_CONN21_1                      (0x0954)
#define AFE_CONN22_1                      (0x0958)
#define AFE_CONN23_1                      (0x095c)
#define AFE_CONN24_1                      (0x0960)
#define AFE_CONN25_1                      (0x0964)
#define AFE_CONN26_1                      (0x0968)
#define AFE_CONN27_1                      (0x096c)
#define AFE_CONN28_1                      (0x0970)
#define AFE_CONN29_1                      (0x0974)
#define AFE_CONN30_1                      (0x0978)
#define AFE_CONN31_1                      (0x097c)
#define AFE_CONN32_1                      (0x0980)
#define AFE_CONN33_1                      (0x0984)
#define AFE_CONN34_1                      (0x0988)
#define AFE_CONN_RS_1                     (0x098c)
#define AFE_CONN_DI_1                     (0x0990)
#define AFE_CONN_24BIT_1                  (0x0994)
#define AFE_CONN_REG                      (0x0998)
#define AFE_CONN35_0                      (0x09a0)
#define AFE_CONN36_0                      (0x09a4)
#define AFE_CONN37_0                      (0x09a8)
#define AFE_CONN38_0                      (0x09ac)
#define AFE_CONN35_1                      (0x09b0)
#define AFE_CONN36_1                      (0x09b4)
#define AFE_CONN37_1                      (0x09b8)
#define AFE_CONN38_1                      (0x09bc)
#define AFE_CONN39_0                      (0x09c0)
#define AFE_CONN40_0                      (0x09c4)
#define AFE_CONN41_0                      (0x09c8)
#define AFE_CONN42_0                      (0x09cc)
#define AFE_CONN39_1                      (0x09e0)
#define AFE_CONN40_1                      (0x09e4)
#define AFE_CONN41_1                      (0x09e8)
#define AFE_CONN42_1                      (0x09ec)
#define AFE_CONN60_0                      (0x0a64)
#define AFE_CONN61_0                      (0x0a68)
#define AFE_CONN62_0                      (0x0a6c)
#define AFE_CONN63_0                      (0x0a70)
#define AFE_CONN64_0                      (0x0a74)
#define AFE_CONN65_0                      (0x0a78)
#define AFE_CONN66_0                      (0x0a7c)
#define AFE_ADDA6_TOP_CON0                (0x0a80)
#define AFE_ADDA6_UL_SRC_CON0             (0x0a84)
#define AFE_ADDA6_UL_SRC_CON1             (0x0a88)
#define AFE_ADDA6_SRC_DEBUG               (0x0a8c)
#define AFE_ADDA6_SRC_DEBUG_MON0          (0x0a90)
#define AFE_ADDA6_ULCF_CFG_02_01          (0x0aa0)
#define AFE_ADDA6_ULCF_CFG_04_03          (0x0aa4)
#define AFE_ADDA6_ULCF_CFG_06_05          (0x0aa8)
#define AFE_ADDA6_ULCF_CFG_08_07          (0x0aac)
#define AFE_ADDA6_ULCF_CFG_10_09          (0x0ab0)
#define AFE_ADDA6_ULCF_CFG_12_11          (0x0ab4)
#define AFE_ADDA6_ULCF_CFG_14_13          (0x0ab8)
#define AFE_ADDA6_ULCF_CFG_16_15          (0x0abc)
#define AFE_ADDA6_ULCF_CFG_18_17          (0x0ac0)
#define AFE_ADDA6_ULCF_CFG_20_19          (0x0ac4)
#define AFE_ADDA6_ULCF_CFG_22_21          (0x0ac8)
#define AFE_ADDA6_ULCF_CFG_24_23          (0x0acc)
#define AFE_ADDA6_ULCF_CFG_26_25          (0x0ad0)
#define AFE_ADDA6_ULCF_CFG_28_27          (0x0ad4)
#define AFE_ADDA6_ULCF_CFG_30_29          (0x0ad8)
#define AFE_ADD6A_UL_SRC_MON0             (0x0ae4)
#define AFE_ADDA6_UL_SRC_MON1             (0x0ae8)
#define AFE_CONN43_0                      (0x0af8)
#define AFE_CONN43_1                      (0x0afc)
#define AFE_MOD_DAI_CON0                  (0x0b00)
#define AFE_MOD_DAI_BASE_MSB              (0x0b04)
#define AFE_MOD_DAI_BASE                  (0x0b08)
#define AFE_MOD_DAI_CUR_MSB               (0x0b0c)
#define AFE_MOD_DAI_CUR                   (0x0b10)
#define AFE_MOD_DAI_END_MSB               (0x0b14)
#define AFE_MOD_DAI_END                   (0x0b18)
#define AFE_AWB_RCH_MON                   (0x0b70)
#define AFE_AWB_LCH_MON                   (0x0b74)
#define AFE_VUL_RCH_MON                   (0x0b78)
#define AFE_VUL_LCH_MON                   (0x0b7c)
#define AFE_VUL12_RCH_MON                 (0x0b80)
#define AFE_VUL12_LCH_MON                 (0x0b84)
#define AFE_VUL2_RCH_MON                  (0x0b88)
#define AFE_VUL2_LCH_MON                  (0x0b8c)
#define AFE_DAI_DATA_MON                  (0x0b90)
#define AFE_MOD_DAI_DATA_MON              (0x0b94)
#define AFE_DAI2_DATA_MON                 (0x0b98)
#define AFE_AWB2_RCH_MON                  (0x0b9c)
#define AFE_AWB2_LCH_MON                  (0x0ba0)
#define AFE_VUL3_RCH_MON                  (0x0ba4)
#define AFE_VUL3_LCH_MON                  (0x0ba8)
#define AFE_VUL4_RCH_MON                  (0x0bac)
#define AFE_VUL4_LCH_MON                  (0x0bb0)
#define AFE_VUL5_RCH_MON                  (0x0bb4)
#define AFE_VUL5_LCH_MON                  (0x0bb8)
#define AFE_VUL6_RCH_MON                  (0x0bbc)
#define AFE_VUL6_LCH_MON                  (0x0bc0)
#define AFE_DL1_RCH_MON                   (0x0bc4)
#define AFE_DL1_LCH_MON                   (0x0bc8)
#define AFE_DL2_RCH_MON                   (0x0bcc)
#define AFE_DL2_LCH_MON                   (0x0bd0)
#define AFE_DL12_RCH1_MON                 (0x0bd4)
#define AFE_DL12_LCH1_MON                 (0x0bd8)
#define AFE_DL12_RCH2_MON                 (0x0bdc)
#define AFE_DL12_LCH2_MON                 (0x0be0)
#define AFE_DL3_RCH_MON                   (0x0be4)
#define AFE_DL3_LCH_MON                   (0x0be8)
#define AFE_DL4_RCH_MON                   (0x0bec)
#define AFE_DL4_LCH_MON                   (0x0bf0)
#define AFE_DL5_RCH_MON                   (0x0bf4)
#define AFE_DL5_LCH_MON                   (0x0bf8)
#define AFE_DL6_RCH_MON                   (0x0bfc)
#define AFE_DL6_LCH_MON                   (0x0c00)
#define AFE_DL7_RCH_MON                   (0x0c04)
#define AFE_DL7_LCH_MON                   (0x0c08)
#define AFE_DL8_RCH_MON                   (0x0c0c)
#define AFE_DL8_LCH_MON                   (0x0c10)
#define AFE_VUL5_CON0                     (0x0c14)
#define AFE_VUL5_BASE_MSB                 (0x0c18)
#define AFE_VUL5_BASE                     (0x0c1c)
#define AFE_VUL5_CUR_MSB                  (0x0c20)
#define AFE_VUL5_CUR                      (0x0c24)
#define AFE_VUL5_END_MSB                  (0x0c28)
#define AFE_VUL5_END                      (0x0c2c)
#define AFE_VUL6_CON0                     (0x0c30)
#define AFE_VUL6_BASE_MSB                 (0x0c34)
#define AFE_VUL6_BASE                     (0x0c38)
#define AFE_VUL6_CUR_MSB                  (0x0c3c)
#define AFE_VUL6_CUR                      (0x0c40)
#define AFE_VUL6_END_MSB                  (0x0c44)
#define AFE_VUL6_END                      (0x0c48)
#define AFE_ADDA_DL_SDM_DCCOMP_CON        (0x0c50)
#define AFE_ADDA_DL_SDM_TEST              (0x0c54)
#define AFE_ADDA_DL_DC_COMP_CFG0          (0x0c58)
#define AFE_ADDA_DL_DC_COMP_CFG1          (0x0c5c)
#define AFE_ADDA_DL_SDM_FIFO_MON          (0x0c60)
#define AFE_ADDA_DL_SRC_LCH_MON           (0x0c64)
#define AFE_ADDA_DL_SRC_RCH_MON           (0x0c68)
#define AFE_ADDA_DL_SDM_OUT_MON           (0x0c6c)
#define AFE_ADDA_DL_SDM_DITHER_CON        (0x0c70)
#define AFE_ADDA_DL_SDM_AUTO_RESET_CON    (0x0c74)
#define AFE_CONNSYS_I2S_CON               (0x0c78)
#define AFE_CONNSYS_I2S_MON               (0x0c7c)
#define AFE_ASRC_2CH_CON0                 (0x0c80)
#define AFE_ASRC_2CH_CON1                 (0x0c84)
#define AFE_ASRC_2CH_CON2                 (0x0c88)
#define AFE_ASRC_2CH_CON3                 (0x0c8c)
#define AFE_ASRC_2CH_CON4                 (0x0c90)
#define AFE_ASRC_2CH_CON5                 (0x0c94)
#define AFE_ASRC_2CH_CON6                 (0x0c98)
#define AFE_ASRC_2CH_CON7                 (0x0c9c)
#define AFE_ASRC_2CH_CON8                 (0x0ca0)
#define AFE_ASRC_2CH_CON9                 (0x0ca4)
#define AFE_ASRC_2CH_CON10                (0x0ca8)
#define AFE_ASRC_2CH_CON12                (0x0cb0)
#define AFE_ASRC_2CH_CON13                (0x0cb4)
#define AFE_ADDA6_IIR_COEF_02_01          (0x0ce0)
#define AFE_ADDA6_IIR_COEF_04_03          (0x0ce4)
#define AFE_ADDA6_IIR_COEF_06_05          (0x0ce8)
#define AFE_ADDA6_IIR_COEF_08_07          (0x0cec)
#define AFE_ADDA6_IIR_COEF_10_09          (0x0cf0)
#define AFE_CONN67_0                      (0x0cf4)
#define AFE_CONN68_0                      (0x0cf8)
#define AFE_CONN69_0                      (0x0cfc)
#define AFE_ADDA_PREDIS_CON2              (0x0d40)
#define AFE_ADDA_PREDIS_CON3              (0x0d44)
#define AFE_CONN44_0                      (0x0d70)
#define AFE_CONN45_0                      (0x0d74)
#define AFE_CONN46_0                      (0x0d78)
#define AFE_CONN47_0                      (0x0d7c)
#define AFE_CONN44_1                      (0x0d80)
#define AFE_CONN45_1                      (0x0d84)
#define AFE_CONN46_1                      (0x0d88)
#define AFE_CONN47_1                      (0x0d8c)
#define AFE_DL1_MON0                      (0x0d90)
#define AFE_DL2_MON0                      (0x0d94)
#define AFE_DL3_MON0                      (0x0d98)
#define AFE_DL4_MON0                      (0x0d9c)
#define AFE_DL5_MON0                      (0x0da0)
#define AFE_DL6_MON0                      (0x0da4)
#define AFE_DL7_MON0                      (0x0da8)
#define AFE_DL8_MON0                      (0x0dac)
#define AFE_DL12_MON0                     (0x0db0)
#define AFE_HD_ENGEN_ENABLE               (0x0dd0)
#define AFE_ADDA_DL_NLE_FIFO_MON          (0x0dfc)
#define AFE_ADDA_MTKAIF_CFG0              (0x0e00)
#define AFE_CONN67_1                      (0x0e04)
#define AFE_CONN68_1                      (0x0e08)
#define AFE_CONN69_1                      (0x0e0c)
#define AFE_ADDA_MTKAIF_SYNCWORD_CFG      (0x0e14)
#define AFE_ADDA_MTKAIF_RX_CFG0           (0x0e20)
#define AFE_ADDA_MTKAIF_RX_CFG1           (0x0e24)
#define AFE_ADDA_MTKAIF_RX_CFG2           (0x0e28)
#define AFE_ADDA_MTKAIF_MON0              (0x0e34)
#define AFE_ADDA_MTKAIF_MON1              (0x0e38)
#define AFE_AUD_PAD_TOP                   (0x0e40)
#define AFE_DL_NLE_R_CFG0                 (0x0e44)
#define AFE_DL_NLE_R_CFG1                 (0x0e48)
#define AFE_DL_NLE_L_CFG0                 (0x0e4c)
#define AFE_DL_NLE_L_CFG1                 (0x0e50)
#define AFE_DL_NLE_R_MON0                 (0x0e54)
#define AFE_DL_NLE_R_MON1                 (0x0e58)
#define AFE_DL_NLE_R_MON2                 (0x0e5c)
#define AFE_DL_NLE_L_MON0                 (0x0e60)
#define AFE_DL_NLE_L_MON1                 (0x0e64)
#define AFE_DL_NLE_L_MON2                 (0x0e68)
#define AFE_DL_NLE_GAIN_CFG0              (0x0e6c)
#define AFE_ADDA6_MTKAIF_CFG0             (0x0e70)
#define AFE_ADDA6_MTKAIF_RX_CFG0          (0x0e74)
#define AFE_ADDA6_MTKAIF_RX_CFG1          (0x0e78)
#define AFE_ADDA6_MTKAIF_RX_CFG2          (0x0e7c)
#define AFE_GENERAL1_ASRC_2CH_CON0        (0x0e80)
#define AFE_GENERAL1_ASRC_2CH_CON1        (0x0e84)
#define AFE_GENERAL1_ASRC_2CH_CON2        (0x0e88)
#define AFE_GENERAL1_ASRC_2CH_CON3        (0x0e8c)
#define AFE_GENERAL1_ASRC_2CH_CON4        (0x0e90)
#define AFE_GENERAL1_ASRC_2CH_CON5        (0x0e94)
#define AFE_GENERAL1_ASRC_2CH_CON6        (0x0e98)
#define AFE_GENERAL1_ASRC_2CH_CON7        (0x0e9c)
#define AFE_GENERAL1_ASRC_2CH_CON8        (0x0ea0)
#define AFE_GENERAL1_ASRC_2CH_CON9        (0x0ea4)
#define AFE_GENERAL1_ASRC_2CH_CON10       (0x0ea8)
#define AFE_GENERAL1_ASRC_2CH_CON12       (0x0eb0)
#define AFE_GENERAL1_ASRC_2CH_CON13       (0x0eb4)
#define GENERAL_ASRC_MODE                 (0x0eb8)
#define GENERAL_ASRC_EN_ON                (0x0ebc)
#define AFE_CONN48_0                      (0x0ec0)
#define AFE_CONN49_0                      (0x0ec4)
#define AFE_CONN50_0                      (0x0ec8)
#define AFE_CONN51_0                      (0x0ecc)
#define AFE_CONN52_0                      (0x0ed0)
#define AFE_CONN53_0                      (0x0ed4)
#define AFE_CONN54_0                      (0x0ed8)
#define AFE_CONN55_0                      (0x0edc)
#define AFE_CONN48_1                      (0x0ee0)
#define AFE_CONN49_1                      (0x0ee4)
#define AFE_CONN50_1                      (0x0ee8)
#define AFE_CONN51_1                      (0x0eec)
#define AFE_CONN52_1                      (0x0ef0)
#define AFE_CONN53_1                      (0x0ef4)
#define AFE_CONN54_1                      (0x0ef8)
#define AFE_CONN55_1                      (0x0efc)
#define AFE_GENERAL2_ASRC_2CH_CON0        (0x0f00)
#define AFE_GENERAL2_ASRC_2CH_CON1        (0x0f04)
#define AFE_GENERAL2_ASRC_2CH_CON2        (0x0f08)
#define AFE_GENERAL2_ASRC_2CH_CON3        (0x0f0c)
#define AFE_GENERAL2_ASRC_2CH_CON4        (0x0f10)
#define AFE_GENERAL2_ASRC_2CH_CON5        (0x0f14)
#define AFE_GENERAL2_ASRC_2CH_CON6        (0x0f18)
#define AFE_GENERAL2_ASRC_2CH_CON7        (0x0f1c)
#define AFE_GENERAL2_ASRC_2CH_CON8        (0x0f20)
#define AFE_GENERAL2_ASRC_2CH_CON9        (0x0f24)
#define AFE_GENERAL2_ASRC_2CH_CON10       (0x0f28)
#define AFE_GENERAL2_ASRC_2CH_CON12       (0x0f30)
#define AFE_GENERAL2_ASRC_2CH_CON13       (0x0f34)
#define AFE_DL5_CON0                      (0x0f4c)
#define AFE_DL5_BASE_MSB                  (0x0f50)
#define AFE_DL5_BASE                      (0x0f54)
#define AFE_DL5_CUR_MSB                   (0x0f58)
#define AFE_DL5_CUR                       (0x0f5c)
#define AFE_DL5_END_MSB                   (0x0f60)
#define AFE_DL5_END                       (0x0f64)
#define AFE_DL6_CON0                      (0x0f68)
#define AFE_DL6_BASE_MSB                  (0x0f6c)
#define AFE_DL6_BASE                      (0x0f70)
#define AFE_DL6_CUR_MSB                   (0x0f74)
#define AFE_DL6_CUR                       (0x0f78)
#define AFE_DL6_END_MSB                   (0x0f7c)
#define AFE_DL6_END                       (0x0f80)
#define AFE_DL7_CON0                      (0x0f84)
#define AFE_DL7_BASE_MSB                  (0x0f88)
#define AFE_DL7_BASE                      (0x0f8c)
#define AFE_DL7_CUR_MSB                   (0x0f90)
#define AFE_DL7_CUR                       (0x0f94)
#define AFE_DL7_END_MSB                   (0x0f98)
#define AFE_DL7_END                       (0x0f9c)
#define AFE_DL8_CON0                      (0x0fa0)
#define AFE_DL8_BASE_MSB                  (0x0fa4)
#define AFE_DL8_BASE                      (0x0fa8)
#define AFE_DL8_CUR_MSB                   (0x0fac)
#define AFE_DL8_CUR                       (0x0fb0)
#define AFE_DL8_END_MSB                   (0x0fb4)
#define AFE_DL8_END                       (0x0fb8)
#define AFE_VUL_MON0                      (0x0fc8)
#define AFE_VUL2_MON0                     (0x0fcc)
#define AFE_VUL3_MON0                     (0x0fd0)
#define AFE_VUL4_MON0                     (0x0fd4)
#define AFE_VUL5_MON0                     (0x0fd8)
#define AFE_VUL6_MON0                     (0x0fdc)
#define AFE_VUL12_MON0                    (0x0fe0)
#define AFE_AWB_MON0                      (0x0fe4)
#define AFE_AWB2_MON0                     (0x0fe8)
#define AFE_MOD_DAI_MON0                  (0x0fec)
#define AFE_DAI_MON0                      (0x0ff0)
#define AFE_DAI2_MON0                     (0x0ff4)
#define AFE_CONN_RS_2                     (0x0ff8)
#define AFE_CONN_DI_2                     (0x0ffc)
#define AFE_CONN_24BIT_2                  (0x1000)
#define AFE_CONN60_1                      (0x11f0)
#define AFE_CONN61_1                      (0x11f4)
#define AFE_CONN62_1                      (0x11f8)
#define AFE_CONN63_1                      (0x11fc)
#define AFE_CONN64_1                      (0x1220)
#define AFE_CONN65_1                      (0x1224)
#define AFE_CONN66_1                      (0x1228)
#define FPGA_CFG4                         (0x1230)
#define FPGA_CFG5                         (0x1234)
#define FPGA_CFG6                         (0x1238)
#define FPGA_CFG7                         (0x123c)
#define FPGA_CFG8                         (0x1240)
#define FPGA_CFG9                         (0x1244)
#define FPGA_CFG10                        (0x1248)
#define FPGA_CFG11                        (0x124c)
#define FPGA_CFG12                        (0x1250)
#define FPGA_CFG13                        (0x1254)
#define ETDM_IN1_CON0                     (0x1430)
#define ETDM_IN1_CON1                     (0x1434)
#define ETDM_IN1_CON2                     (0x1438)
#define ETDM_IN1_CON3                     (0x143c)
#define ETDM_IN1_CON4                     (0x1440)
#define ETDM_IN1_CON5                     (0x1444)
#define ETDM_IN1_CON6                     (0x1448)
#define ETDM_IN1_CON7                     (0x144c)
#define ETDM_IN1_CON8                     (0x1450)
#define ETDM_OUT1_CON0                    (0x1454)
#define ETDM_OUT1_CON1                    (0x1458)
#define ETDM_OUT1_CON2                    (0x145c)
#define ETDM_OUT1_CON3                    (0x1460)
#define ETDM_OUT1_CON4                    (0x1464)
#define ETDM_OUT1_CON5                    (0x1468)
#define ETDM_OUT1_CON6                    (0x146c)
#define ETDM_OUT1_CON7                    (0x1470)
#define ETDM_OUT1_CON8                    (0x1474)
#define ETDM_IN1_MON                      (0x1478)
#define ETDM_OUT1_MON                     (0x147c)
#define ETDM_0_3_COWORK_CON0              (0x18b0)
#define ETDM_0_3_COWORK_CON1              (0x18b4)
#define ETDM_0_3_COWORK_CON3              (0x18bc)

/* AFE_DAC_CON0 */
#define VUL12_ON_SFT                                   31
#define VUL12_ON_MASK                                  0x1
#define VUL12_ON_MASK_SFT                              (0x1 << 31)
#define MOD_DAI_ON_SFT                                 30
#define MOD_DAI_ON_MASK                                0x1
#define MOD_DAI_ON_MASK_SFT                            (0x1 << 30)
#define DAI_ON_SFT                                     29
#define DAI_ON_MASK                                    0x1
#define DAI_ON_MASK_SFT                                (0x1 << 29)
#define DAI2_ON_SFT                                    28
#define DAI2_ON_MASK                                   0x1
#define DAI2_ON_MASK_SFT                               (0x1 << 28)
#define VUL6_ON_SFT                                    23
#define VUL6_ON_MASK                                   0x1
#define VUL6_ON_MASK_SFT                               (0x1 << 23)
#define VUL5_ON_SFT                                    22
#define VUL5_ON_MASK                                   0x1
#define VUL5_ON_MASK_SFT                               (0x1 << 22)
#define VUL4_ON_SFT                                    21
#define VUL4_ON_MASK                                   0x1
#define VUL4_ON_MASK_SFT                               (0x1 << 21)
#define VUL3_ON_SFT                                    20
#define VUL3_ON_MASK                                   0x1
#define VUL3_ON_MASK_SFT                               (0x1 << 20)
#define VUL2_ON_SFT                                    19
#define VUL2_ON_MASK                                   0x1
#define VUL2_ON_MASK_SFT                               (0x1 << 19)
#define VUL_ON_SFT                                     18
#define VUL_ON_MASK                                    0x1
#define VUL_ON_MASK_SFT                                (0x1 << 18)
#define AWB2_ON_SFT                                    17
#define AWB2_ON_MASK                                   0x1
#define AWB2_ON_MASK_SFT                               (0x1 << 17)
#define AWB_ON_SFT                                     16
#define AWB_ON_MASK                                    0x1
#define AWB_ON_MASK_SFT                                (0x1 << 16)
#define DL12_ON_SFT                                    15
#define DL12_ON_MASK                                   0x1
#define DL12_ON_MASK_SFT                               (0x1 << 15)
#define DL8_ON_SFT                                     11
#define DL8_ON_MASK                                    0x1
#define DL8_ON_MASK_SFT                                (0x1 << 11)
#define DL7_ON_SFT                                     10
#define DL7_ON_MASK                                    0x1
#define DL7_ON_MASK_SFT                                (0x1 << 10)
#define DL6_ON_SFT                                     9
#define DL6_ON_MASK                                    0x1
#define DL6_ON_MASK_SFT                                (0x1 << 9)
#define DL5_ON_SFT                                     8
#define DL5_ON_MASK                                    0x1
#define DL5_ON_MASK_SFT                                (0x1 << 8)
#define DL4_ON_SFT                                     7
#define DL4_ON_MASK                                    0x1
#define DL4_ON_MASK_SFT                                (0x1 << 7)
#define DL3_ON_SFT                                     6
#define DL3_ON_MASK                                    0x1
#define DL3_ON_MASK_SFT                                (0x1 << 6)
#define DL2_ON_SFT                                     5
#define DL2_ON_MASK                                    0x1
#define DL2_ON_MASK_SFT                                (0x1 << 5)
#define DL1_ON_SFT                                     4
#define DL1_ON_MASK                                    0x1
#define DL1_ON_MASK_SFT                                (0x1 << 4)
#define AFE_ON_SFT                                     0
#define AFE_ON_MASK                                    0x1
#define AFE_ON_MASK_SFT                                (0x1 << 0)

// AFE_CM1_CON
#define CHANNEL_MERGE0_DEBUG_MODE_SFT                   (31)
#define CHANNEL_MERGE0_DEBUG_MODE_MASK                  (0x1)
#define CHANNEL_MERGE0_DEBUG_MODE_MASK_SFT              (0x1 << 31)
#define VUL3_BYPASS_CM_SFT                              (30)
#define VUL3_BYPASS_CM_MASK                             (0x1)
#define VUL3_BYPASS_CM_MASK_SFT                         (0x1 << 30)
#define CM1_DEBUG_MODE_SEL_SFT                          (29)
#define CM1_DEBUG_MODE_SEL_MASK                         (0x1)
#define CM1_DEBUG_MODE_SEL_MASK_SFT                     (0x1 << 29)
#define CHANNEL_MERGE0_UPDATE_CNT_SFT                   (16)
#define CHANNEL_MERGE0_UPDATE_CNT_MASK                  (0x1fff)
#define CHANNEL_MERGE0_UPDATE_CNT_MASK_SFT              (0x1fff << 16)
#define CM1_FS_SELECT_SFT                               (8)
#define CM1_FS_SELECT_MASK                              (0x1f)
#define CM1_FS_SELECT_MASK_SFT                          (0x1f << 8)
#define CHANNEL_MERGE0_CHNUM_SFT                        (3)
#define CHANNEL_MERGE0_CHNUM_MASK                       (0x1f)
#define CHANNEL_MERGE0_CHNUM_MASK_SFT                   (0x1f << 3)
#define CHANNEL_MERGE0_BYTE_SWAP_SFT                    (1)
#define CHANNEL_MERGE0_BYTE_SWAP_MASK                   (0x1)
#define CHANNEL_MERGE0_BYTE_SWAP_MASK_SFT               (0x1 << 1)
#define CHANNEL_MERGE0_EN_SFT                           (0)
#define CHANNEL_MERGE0_EN_MASK                          (0x1)
#define CHANNEL_MERGE0_EN_MASK_SFT                      (0x1 << 0)

// AFE_CM1_MON
#define AFE_CM0_OUTPUT_CNT_POS				(16)
#define AFE_CM0_OUTPUT_CNT_LEN				(13)
#define AFE_CM0_CUR_CHSET_POS				(5)
#define AFE_CM0_CUR_CHSET_LEN				(4)
#define AFE_CM0_ODD_FLAG_POS				(4)
#define AFE_CM0_ODD_FLAG_LEN				(1)
#define AFE_CM0_BYTE_SWAP_MON_POS			(1)
#define AFE_CM0_BYTE_SWAP_MON_LEN			(1)
#define AFE_CM0_ON_MON_POS				(0)
#define AFE_CM0_ON_MON_LEN				(1)

/* AFE_DL1_CON0 */
#define DL1_MODE_SFT                                   24
#define DL1_MODE_MASK                                  0xf
#define DL1_MODE_MASK_SFT                              (0xf << 24)
#define DL1_MINLEN_SFT                                 20
#define DL1_MINLEN_MASK                                0xf
#define DL1_MINLEN_MASK_SFT                            (0xf << 20)
#define DL1_MAXLEN_SFT                                 16
#define DL1_MAXLEN_MASK                                0xf
#define DL1_MAXLEN_MASK_SFT                            (0xf << 16)
#define DL1_SW_CLEAR_BUF_EMPTY_SFT                     15
#define DL1_SW_CLEAR_BUF_EMPTY_MASK                    0x1
#define DL1_SW_CLEAR_BUF_EMPTY_MASK_SFT                (0x1 << 15)
#define DL1_PBUF_SIZE_SFT                              12
#define DL1_PBUF_SIZE_MASK                             0x3
#define DL1_PBUF_SIZE_MASK_SFT                         (0x3 << 12)
#define DL1_MONO_SFT                                   8
#define DL1_MONO_MASK                                  0x1
#define DL1_MONO_MASK_SFT                              (0x1 << 8)
#define DL1_NORMAL_MODE_SFT                            5
#define DL1_NORMAL_MODE_MASK                           0x1
#define DL1_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DL1_HALIGN_SFT                                 4
#define DL1_HALIGN_MASK                                0x1
#define DL1_HALIGN_MASK_SFT                            (0x1 << 4)
#define DL1_HD_MODE_SFT                                0
#define DL1_HD_MODE_MASK                               0x3
#define DL1_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_DL2_CON0 */
#define DL2_MODE_SFT                                   24
#define DL2_MODE_MASK                                  0xf
#define DL2_MODE_MASK_SFT                              (0xf << 24)
#define DL2_MINLEN_SFT                                 20
#define DL2_MINLEN_MASK                                0xf
#define DL2_MINLEN_MASK_SFT                            (0xf << 20)
#define DL2_MAXLEN_SFT                                 16
#define DL2_MAXLEN_MASK                                0xf
#define DL2_MAXLEN_MASK_SFT                            (0xf << 16)
#define DL2_SW_CLEAR_BUF_EMPTY_SFT                     15
#define DL2_SW_CLEAR_BUF_EMPTY_MASK                    0x1
#define DL2_SW_CLEAR_BUF_EMPTY_MASK_SFT                (0x1 << 15)
#define DL2_PBUF_SIZE_SFT                              12
#define DL2_PBUF_SIZE_MASK                             0x3
#define DL2_PBUF_SIZE_MASK_SFT                         (0x3 << 12)
#define DL2_MONO_SFT                                   8
#define DL2_MONO_MASK                                  0x1
#define DL2_MONO_MASK_SFT                              (0x1 << 8)
#define DL2_NORMAL_MODE_SFT                            5
#define DL2_NORMAL_MODE_MASK                           0x1
#define DL2_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DL2_HALIGN_SFT                                 4
#define DL2_HALIGN_MASK                                0x1
#define DL2_HALIGN_MASK_SFT                            (0x1 << 4)
#define DL2_HD_MODE_SFT                                0
#define DL2_HD_MODE_MASK                               0x3
#define DL2_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_DL3_CON0 */
#define DL3_MODE_SFT                                   24
#define DL3_MODE_MASK                                  0xf
#define DL3_MODE_MASK_SFT                              (0xf << 24)
#define DL3_MINLEN_SFT                                 20
#define DL3_MINLEN_MASK                                0xf
#define DL3_MINLEN_MASK_SFT                            (0xf << 20)
#define DL3_MAXLEN_SFT                                 16
#define DL3_MAXLEN_MASK                                0xf
#define DL3_MAXLEN_MASK_SFT                            (0xf << 16)
#define DL3_SW_CLEAR_BUF_EMPTY_SFT                     15
#define DL3_SW_CLEAR_BUF_EMPTY_MASK                    0x1
#define DL3_SW_CLEAR_BUF_EMPTY_MASK_SFT                (0x1 << 15)
#define DL3_PBUF_SIZE_SFT                              12
#define DL3_PBUF_SIZE_MASK                             0x3
#define DL3_PBUF_SIZE_MASK_SFT                         (0x3 << 12)
#define DL3_MONO_SFT                                   8
#define DL3_MONO_MASK                                  0x1
#define DL3_MONO_MASK_SFT                              (0x1 << 8)
#define DL3_NORMAL_MODE_SFT                            5
#define DL3_NORMAL_MODE_MASK                           0x1
#define DL3_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DL3_HALIGN_SFT                                 4
#define DL3_HALIGN_MASK                                0x1
#define DL3_HALIGN_MASK_SFT                            (0x1 << 4)
#define DL3_HD_MODE_SFT                                0
#define DL3_HD_MODE_MASK                               0x3
#define DL3_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_DL4_CON0 */
#define DL4_MODE_SFT                                   24
#define DL4_MODE_MASK                                  0xf
#define DL4_MODE_MASK_SFT                              (0xf << 24)
#define DL4_MINLEN_SFT                                 20
#define DL4_MINLEN_MASK                                0xf
#define DL4_MINLEN_MASK_SFT                            (0xf << 20)
#define DL4_MAXLEN_SFT                                 16
#define DL4_MAXLEN_MASK                                0xf
#define DL4_MAXLEN_MASK_SFT                            (0xf << 16)
#define DL4_SW_CLEAR_BUF_EMPTY_SFT                     15
#define DL4_SW_CLEAR_BUF_EMPTY_MASK                    0x1
#define DL4_SW_CLEAR_BUF_EMPTY_MASK_SFT                (0x1 << 15)
#define DL4_PBUF_SIZE_SFT                              12
#define DL4_PBUF_SIZE_MASK                             0x3
#define DL4_PBUF_SIZE_MASK_SFT                         (0x3 << 12)
#define DL4_MONO_SFT                                   8
#define DL4_MONO_MASK                                  0x1
#define DL4_MONO_MASK_SFT                              (0x1 << 8)
#define DL4_NORMAL_MODE_SFT                            5
#define DL4_NORMAL_MODE_MASK                           0x1
#define DL4_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DL4_HALIGN_SFT                                 4
#define DL4_HALIGN_MASK                                0x1
#define DL4_HALIGN_MASK_SFT                            (0x1 << 4)
#define DL4_HD_MODE_SFT                                0
#define DL4_HD_MODE_MASK                               0x3
#define DL4_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_DL5_CON0 */
#define DL5_MODE_SFT                                   24
#define DL5_MODE_MASK                                  0xf
#define DL5_MODE_MASK_SFT                              (0xf << 24)
#define DL5_MINLEN_SFT                                 20
#define DL5_MINLEN_MASK                                0xf
#define DL5_MINLEN_MASK_SFT                            (0xf << 20)
#define DL5_MAXLEN_SFT                                 16
#define DL5_MAXLEN_MASK                                0xf
#define DL5_MAXLEN_MASK_SFT                            (0xf << 16)
#define DL5_SW_CLEAR_BUF_EMPTY_SFT                     15
#define DL5_SW_CLEAR_BUF_EMPTY_MASK                    0x1
#define DL5_SW_CLEAR_BUF_EMPTY_MASK_SFT                (0x1 << 15)
#define DL5_PBUF_SIZE_SFT                              12
#define DL5_PBUF_SIZE_MASK                             0x3
#define DL5_PBUF_SIZE_MASK_SFT                         (0x3 << 12)
#define DL5_MONO_SFT                                   8
#define DL5_MONO_MASK                                  0x1
#define DL5_MONO_MASK_SFT                              (0x1 << 8)
#define DL5_NORMAL_MODE_SFT                            5
#define DL5_NORMAL_MODE_MASK                           0x1
#define DL5_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DL5_HALIGN_SFT                                 4
#define DL5_HALIGN_MASK                                0x1
#define DL5_HALIGN_MASK_SFT                            (0x1 << 4)
#define DL5_HD_MODE_SFT                                0
#define DL5_HD_MODE_MASK                               0x3
#define DL5_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_DL6_CON0 */
#define DL6_MODE_SFT                                   24
#define DL6_MODE_MASK                                  0xf
#define DL6_MODE_MASK_SFT                              (0xf << 24)
#define DL6_MINLEN_SFT                                 20
#define DL6_MINLEN_MASK                                0xf
#define DL6_MINLEN_MASK_SFT                            (0xf << 20)
#define DL6_MAXLEN_SFT                                 16
#define DL6_MAXLEN_MASK                                0xf
#define DL6_MAXLEN_MASK_SFT                            (0xf << 16)
#define DL6_SW_CLEAR_BUF_EMPTY_SFT                     15
#define DL6_SW_CLEAR_BUF_EMPTY_MASK                    0x1
#define DL6_SW_CLEAR_BUF_EMPTY_MASK_SFT                (0x1 << 15)
#define DL6_PBUF_SIZE_SFT                              12
#define DL6_PBUF_SIZE_MASK                             0x3
#define DL6_PBUF_SIZE_MASK_SFT                         (0x3 << 12)
#define DL6_MONO_SFT                                   8
#define DL6_MONO_MASK                                  0x1
#define DL6_MONO_MASK_SFT                              (0x1 << 8)
#define DL6_NORMAL_MODE_SFT                            5
#define DL6_NORMAL_MODE_MASK                           0x1
#define DL6_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DL6_HALIGN_SFT                                 4
#define DL6_HALIGN_MASK                                0x1
#define DL6_HALIGN_MASK_SFT                            (0x1 << 4)
#define DL6_HD_MODE_SFT                                0
#define DL6_HD_MODE_MASK                               0x3
#define DL6_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_DL7_CON0 */
#define DL7_MODE_SFT                                   24
#define DL7_MODE_MASK                                  0xf
#define DL7_MODE_MASK_SFT                              (0xf << 24)
#define DL7_MINLEN_SFT                                 20
#define DL7_MINLEN_MASK                                0xf
#define DL7_MINLEN_MASK_SFT                            (0xf << 20)
#define DL7_MAXLEN_SFT                                 16
#define DL7_MAXLEN_MASK                                0xf
#define DL7_MAXLEN_MASK_SFT                            (0xf << 16)
#define DL7_SW_CLEAR_BUF_EMPTY_SFT                     15
#define DL7_SW_CLEAR_BUF_EMPTY_MASK                    0x1
#define DL7_SW_CLEAR_BUF_EMPTY_MASK_SFT                (0x1 << 15)
#define DL7_PBUF_SIZE_SFT                              12
#define DL7_PBUF_SIZE_MASK                             0x3
#define DL7_PBUF_SIZE_MASK_SFT                         (0x3 << 12)
#define DL7_MONO_SFT                                   8
#define DL7_MONO_MASK                                  0x1
#define DL7_MONO_MASK_SFT                              (0x1 << 8)
#define DL7_NORMAL_MODE_SFT                            5
#define DL7_NORMAL_MODE_MASK                           0x1
#define DL7_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DL7_HALIGN_SFT                                 4
#define DL7_HALIGN_MASK                                0x1
#define DL7_HALIGN_MASK_SFT                            (0x1 << 4)
#define DL7_HD_MODE_SFT                                0
#define DL7_HD_MODE_MASK                               0x3
#define DL7_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_DL8_CON0 */
#define DL8_MODE_SFT                                   24
#define DL8_MODE_MASK                                  0xf
#define DL8_MODE_MASK_SFT                              (0xf << 24)
#define DL8_MINLEN_SFT                                 20
#define DL8_MINLEN_MASK                                0xf
#define DL8_MINLEN_MASK_SFT                            (0xf << 20)
#define DL8_MAXLEN_SFT                                 16
#define DL8_MAXLEN_MASK                                0xf
#define DL8_MAXLEN_MASK_SFT                            (0xf << 16)
#define DL8_SW_CLEAR_BUF_EMPTY_SFT                     15
#define DL8_SW_CLEAR_BUF_EMPTY_MASK                    0x1
#define DL8_SW_CLEAR_BUF_EMPTY_MASK_SFT                (0x1 << 15)
#define DL8_PBUF_SIZE_SFT                              12
#define DL8_PBUF_SIZE_MASK                             0x3
#define DL8_PBUF_SIZE_MASK_SFT                         (0x3 << 12)
#define DL8_MONO_SFT                                   8
#define DL8_MONO_MASK                                  0x1
#define DL8_MONO_MASK_SFT                              (0x1 << 8)
#define DL8_NORMAL_MODE_SFT                            5
#define DL8_NORMAL_MODE_MASK                           0x1
#define DL8_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DL8_HALIGN_SFT                                 4
#define DL8_HALIGN_MASK                                0x1
#define DL8_HALIGN_MASK_SFT                            (0x1 << 4)
#define DL8_HD_MODE_SFT                                0
#define DL8_HD_MODE_MASK                               0x3
#define DL8_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_DL12_CON0 */
#define DL12_MODE_SFT                                  24
#define DL12_MODE_MASK                                 0xf
#define DL12_MODE_MASK_SFT                             (0xf << 24)
#define DL12_MINLEN_SFT                                20
#define DL12_MINLEN_MASK                               0xf
#define DL12_MINLEN_MASK_SFT                           (0xf << 20)
#define DL12_MAXLEN_SFT                                16
#define DL12_MAXLEN_MASK                               0xf
#define DL12_MAXLEN_MASK_SFT                           (0xf << 16)
#define DL12_SW_CLEAR_BUF_EMPTY_SFT                    15
#define DL12_SW_CLEAR_BUF_EMPTY_MASK                   0x1
#define DL12_SW_CLEAR_BUF_EMPTY_MASK_SFT               (0x1 << 15)
#define DL12_PBUF_SIZE_SFT                             12
#define DL12_PBUF_SIZE_MASK                            0x3
#define DL12_PBUF_SIZE_MASK_SFT                        (0x3 << 12)
#define DL12_4CH_EN_SFT                                11
#define DL12_4CH_EN_MASK                               0x1
#define DL12_4CH_EN_MASK_SFT                           (0x1 << 11)
#define DL12_MONO_SFT                                  8
#define DL12_MONO_MASK                                 0x1
#define DL12_MONO_MASK_SFT                             (0x1 << 8)
#define DL12_NORMAL_MODE_SFT                           5
#define DL12_NORMAL_MODE_MASK                          0x1
#define DL12_NORMAL_MODE_MASK_SFT                      (0x1 << 5)
#define DL12_HALIGN_SFT                                4
#define DL12_HALIGN_MASK                               0x1
#define DL12_HALIGN_MASK_SFT                           (0x1 << 4)
#define DL12_HD_MODE_SFT                               0
#define DL12_HD_MODE_MASK                              0x3
#define DL12_HD_MODE_MASK_SFT                          (0x3 << 0)

/* AFE_AWB_CON0 */
#define AWB_MODE_SFT                                   24
#define AWB_MODE_MASK                                  0xf
#define AWB_MODE_MASK_SFT                              (0xf << 24)
#define AWB_SW_CLEAR_BUF_FULL_SFT                      15
#define AWB_SW_CLEAR_BUF_FULL_MASK                     0x1
#define AWB_SW_CLEAR_BUF_FULL_MASK_SFT                 (0x1 << 15)
#define AWB_R_MONO_SFT                                 9
#define AWB_R_MONO_MASK                                0x1
#define AWB_R_MONO_MASK_SFT                            (0x1 << 9)
#define AWB_MONO_SFT                                   8
#define AWB_MONO_MASK                                  0x1
#define AWB_MONO_MASK_SFT                              (0x1 << 8)
#define AWB_WR_SIGN_SFT                                6
#define AWB_WR_SIGN_MASK                               0x1
#define AWB_WR_SIGN_MASK_SFT                           (0x1 << 6)
#define AWB_NORMAL_MODE_SFT                            5
#define AWB_NORMAL_MODE_MASK                           0x1
#define AWB_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define AWB_HALIGN_SFT                                 4
#define AWB_HALIGN_MASK                                0x1
#define AWB_HALIGN_MASK_SFT                            (0x1 << 4)
#define AWB_HD_MODE_SFT                                0
#define AWB_HD_MODE_MASK                               0x3
#define AWB_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_AWB2_CON0 */
#define AWB2_MODE_SFT                                  24
#define AWB2_MODE_MASK                                 0xf
#define AWB2_MODE_MASK_SFT                             (0xf << 24)
#define AWB2_SW_CLEAR_BUF_FULL_SFT                     15
#define AWB2_SW_CLEAR_BUF_FULL_MASK                    0x1
#define AWB2_SW_CLEAR_BUF_FULL_MASK_SFT                (0x1 << 15)
#define AWB2_R_MONO_SFT                                9
#define AWB2_R_MONO_MASK                               0x1
#define AWB2_R_MONO_MASK_SFT                           (0x1 << 9)
#define AWB2_MONO_SFT                                  8
#define AWB2_MONO_MASK                                 0x1
#define AWB2_MONO_MASK_SFT                             (0x1 << 8)
#define AWB2_WR_SIGN_SFT                               6
#define AWB2_WR_SIGN_MASK                              0x1
#define AWB2_WR_SIGN_MASK_SFT                          (0x1 << 6)
#define AWB2_NORMAL_MODE_SFT                           5
#define AWB2_NORMAL_MODE_MASK                          0x1
#define AWB2_NORMAL_MODE_MASK_SFT                      (0x1 << 5)
#define AWB2_HALIGN_SFT                                4
#define AWB2_HALIGN_MASK                               0x1
#define AWB2_HALIGN_MASK_SFT                           (0x1 << 4)
#define AWB2_HD_MODE_SFT                               0
#define AWB2_HD_MODE_MASK                              0x3
#define AWB2_HD_MODE_MASK_SFT                          (0x3 << 0)

/* AFE_VUL_CON0 */
#define VUL_MODE_SFT                                   24
#define VUL_MODE_MASK                                  0xf
#define VUL_MODE_MASK_SFT                              (0xf << 24)
#define VUL_SW_CLEAR_BUF_FULL_SFT                      15
#define VUL_SW_CLEAR_BUF_FULL_MASK                     0x1
#define VUL_SW_CLEAR_BUF_FULL_MASK_SFT                 (0x1 << 15)
#define VUL_R_MONO_SFT                                 9
#define VUL_R_MONO_MASK                                0x1
#define VUL_R_MONO_MASK_SFT                            (0x1 << 9)
#define VUL_MONO_SFT                                   8
#define VUL_MONO_MASK                                  0x1
#define VUL_MONO_MASK_SFT                              (0x1 << 8)
#define VUL_WR_SIGN_SFT                                6
#define VUL_WR_SIGN_MASK                               0x1
#define VUL_WR_SIGN_MASK_SFT                           (0x1 << 6)
#define VUL_NORMAL_MODE_SFT                            5
#define VUL_NORMAL_MODE_MASK                           0x1
#define VUL_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define VUL_HALIGN_SFT                                 4
#define VUL_HALIGN_MASK                                0x1
#define VUL_HALIGN_MASK_SFT                            (0x1 << 4)
#define VUL_HD_MODE_SFT                                0
#define VUL_HD_MODE_MASK                               0x3
#define VUL_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_VUL12_CON0 */
#define VUL12_MODE_SFT                                 24
#define VUL12_MODE_MASK                                0xf
#define VUL12_MODE_MASK_SFT                            (0xf << 24)
#define VUL12_SW_CLEAR_BUF_FULL_SFT                    15
#define VUL12_SW_CLEAR_BUF_FULL_MASK                   0x1
#define VUL12_SW_CLEAR_BUF_FULL_MASK_SFT               (0x1 << 15)
#define VUL12_4CH_EN_SFT                               11
#define VUL12_4CH_EN_MASK                              0x1
#define VUL12_4CH_EN_MASK_SFT                          (0x1 << 11)
#define VUL12_R_MONO_SFT                               9
#define VUL12_R_MONO_MASK                              0x1
#define VUL12_R_MONO_MASK_SFT                          (0x1 << 9)
#define VUL12_MONO_SFT                                 8
#define VUL12_MONO_MASK                                0x1
#define VUL12_MONO_MASK_SFT                            (0x1 << 8)
#define VUL12_WR_SIGN_SFT                              6
#define VUL12_WR_SIGN_MASK                             0x1
#define VUL12_WR_SIGN_MASK_SFT                         (0x1 << 6)
#define VUL12_NORMAL_MODE_SFT                          5
#define VUL12_NORMAL_MODE_MASK                         0x1
#define VUL12_NORMAL_MODE_MASK_SFT                     (0x1 << 5)
#define VUL12_HALIGN_SFT                               4
#define VUL12_HALIGN_MASK                              0x1
#define VUL12_HALIGN_MASK_SFT                          (0x1 << 4)
#define VUL12_HD_MODE_SFT                              0
#define VUL12_HD_MODE_MASK                             0x3
#define VUL12_HD_MODE_MASK_SFT                         (0x3 << 0)

/* AFE_VUL2_CON0 */
#define VUL2_MODE_SFT                                  24
#define VUL2_MODE_MASK                                 0xf
#define VUL2_MODE_MASK_SFT                             (0xf << 24)
#define VUL2_SW_CLEAR_BUF_FULL_SFT                     15
#define VUL2_SW_CLEAR_BUF_FULL_MASK                    0x1
#define VUL2_SW_CLEAR_BUF_FULL_MASK_SFT                (0x1 << 15)
#define VUL2_R_MONO_SFT                                9
#define VUL2_R_MONO_MASK                               0x1
#define VUL2_R_MONO_MASK_SFT                           (0x1 << 9)
#define VUL2_MONO_SFT                                  8
#define VUL2_MONO_MASK                                 0x1
#define VUL2_MONO_MASK_SFT                             (0x1 << 8)
#define VUL2_WR_SIGN_SFT                               6
#define VUL2_WR_SIGN_MASK                              0x1
#define VUL2_WR_SIGN_MASK_SFT                          (0x1 << 6)
#define VUL2_NORMAL_MODE_SFT                           5
#define VUL2_NORMAL_MODE_MASK                          0x1
#define VUL2_NORMAL_MODE_MASK_SFT                      (0x1 << 5)
#define VUL2_HALIGN_SFT                                4
#define VUL2_HALIGN_MASK                               0x1
#define VUL2_HALIGN_MASK_SFT                           (0x1 << 4)
#define VUL2_HD_MODE_SFT                               0
#define VUL2_HD_MODE_MASK                              0x3
#define VUL2_HD_MODE_MASK_SFT                          (0x3 << 0)

/* AFE_VUL3_CON0 */
#define VUL3_MODE_SFT                                  24
#define VUL3_MODE_MASK                                 0xf
#define VUL3_MODE_MASK_SFT                             (0xf << 24)
#define VUL3_SW_CLEAR_BUF_FULL_SFT                     15
#define VUL3_SW_CLEAR_BUF_FULL_MASK                    0x1
#define VUL3_SW_CLEAR_BUF_FULL_MASK_SFT                (0x1 << 15)
#define VUL3_R_MONO_SFT                                9
#define VUL3_R_MONO_MASK                               0x1
#define VUL3_R_MONO_MASK_SFT                           (0x1 << 9)
#define VUL3_MONO_SFT                                  8
#define VUL3_MONO_MASK                                 0x1
#define VUL3_MONO_MASK_SFT                             (0x1 << 8)
#define VUL3_WR_SIGN_SFT                               6
#define VUL3_WR_SIGN_MASK                              0x1
#define VUL3_WR_SIGN_MASK_SFT                          (0x1 << 6)
#define VUL3_NORMAL_MODE_SFT                           5
#define VUL3_NORMAL_MODE_MASK                          0x1
#define VUL3_NORMAL_MODE_MASK_SFT                      (0x1 << 5)
#define VUL3_HALIGN_SFT                                4
#define VUL3_HALIGN_MASK                               0x1
#define VUL3_HALIGN_MASK_SFT                           (0x1 << 4)
#define VUL3_HD_MODE_SFT                               0
#define VUL3_HD_MODE_MASK                              0x3
#define VUL3_HD_MODE_MASK_SFT                          (0x3 << 0)

/* AFE_VUL4_CON0 */
#define VUL4_MODE_SFT                                  24
#define VUL4_MODE_MASK                                 0xf
#define VUL4_MODE_MASK_SFT                             (0xf << 24)
#define VUL4_SW_CLEAR_BUF_FULL_SFT                     15
#define VUL4_SW_CLEAR_BUF_FULL_MASK                    0x1
#define VUL4_SW_CLEAR_BUF_FULL_MASK_SFT                (0x1 << 15)
#define VUL4_R_MONO_SFT                                9
#define VUL4_R_MONO_MASK                               0x1
#define VUL4_R_MONO_MASK_SFT                           (0x1 << 9)
#define VUL4_MONO_SFT                                  8
#define VUL4_MONO_MASK                                 0x1
#define VUL4_MONO_MASK_SFT                             (0x1 << 8)
#define VUL4_WR_SIGN_SFT                               6
#define VUL4_WR_SIGN_MASK                              0x1
#define VUL4_WR_SIGN_MASK_SFT                          (0x1 << 6)
#define VUL4_NORMAL_MODE_SFT                           5
#define VUL4_NORMAL_MODE_MASK                          0x1
#define VUL4_NORMAL_MODE_MASK_SFT                      (0x1 << 5)
#define VUL4_HALIGN_SFT                                4
#define VUL4_HALIGN_MASK                               0x1
#define VUL4_HALIGN_MASK_SFT                           (0x1 << 4)
#define VUL4_HD_MODE_SFT                               0
#define VUL4_HD_MODE_MASK                              0x3
#define VUL4_HD_MODE_MASK_SFT                          (0x3 << 0)

/* AFE_VUL5_CON0 */
#define VUL5_MODE_SFT                                  24
#define VUL5_MODE_MASK                                 0xf
#define VUL5_MODE_MASK_SFT                             (0xf << 24)
#define VUL5_SW_CLEAR_BUF_FULL_SFT                     15
#define VUL5_SW_CLEAR_BUF_FULL_MASK                    0x1
#define VUL5_SW_CLEAR_BUF_FULL_MASK_SFT                (0x1 << 15)
#define VUL5_R_MONO_SFT                                9
#define VUL5_R_MONO_MASK                               0x1
#define VUL5_R_MONO_MASK_SFT                           (0x1 << 9)
#define VUL5_MONO_SFT                                  8
#define VUL5_MONO_MASK                                 0x1
#define VUL5_MONO_MASK_SFT                             (0x1 << 8)
#define VUL5_WR_SIGN_SFT                               6
#define VUL5_WR_SIGN_MASK                              0x1
#define VUL5_WR_SIGN_MASK_SFT                          (0x1 << 6)
#define VUL5_NORMAL_MODE_SFT                           5
#define VUL5_NORMAL_MODE_MASK                          0x1
#define VUL5_NORMAL_MODE_MASK_SFT                      (0x1 << 5)
#define VUL5_HALIGN_SFT                                4
#define VUL5_HALIGN_MASK                               0x1
#define VUL5_HALIGN_MASK_SFT                           (0x1 << 4)
#define VUL5_HD_MODE_SFT                               0
#define VUL5_HD_MODE_MASK                              0x3
#define VUL5_HD_MODE_MASK_SFT                          (0x3 << 0)

/* AFE_VUL6_CON0 */
#define VUL6_MODE_SFT                                  24
#define VUL6_MODE_MASK                                 0xf
#define VUL6_MODE_MASK_SFT                             (0xf << 24)
#define VUL6_SW_CLEAR_BUF_FULL_SFT                     15
#define VUL6_SW_CLEAR_BUF_FULL_MASK                    0x1
#define VUL6_SW_CLEAR_BUF_FULL_MASK_SFT                (0x1 << 15)
#define VUL6_R_MONO_SFT                                9
#define VUL6_R_MONO_MASK                               0x1
#define VUL6_R_MONO_MASK_SFT                           (0x1 << 9)
#define VUL6_MONO_SFT                                  8
#define VUL6_MONO_MASK                                 0x1
#define VUL6_MONO_MASK_SFT                             (0x1 << 8)
#define VUL6_WR_SIGN_SFT                               6
#define VUL6_WR_SIGN_MASK                              0x1
#define VUL6_WR_SIGN_MASK_SFT                          (0x1 << 6)
#define VUL6_NORMAL_MODE_SFT                           5
#define VUL6_NORMAL_MODE_MASK                          0x1
#define VUL6_NORMAL_MODE_MASK_SFT                      (0x1 << 5)
#define VUL6_HALIGN_SFT                                4
#define VUL6_HALIGN_MASK                               0x1
#define VUL6_HALIGN_MASK_SFT                           (0x1 << 4)
#define VUL6_HD_MODE_SFT                               0
#define VUL6_HD_MODE_MASK                              0x3
#define VUL6_HD_MODE_MASK_SFT                          (0x3 << 0)

/* AFE_DAI_CON0 */
#define DAI_MODE_SFT                                   24
#define DAI_MODE_MASK                                  0x3
#define DAI_MODE_MASK_SFT                              (0x3 << 24)
#define DAI_SW_CLEAR_BUF_FULL_SFT                      15
#define DAI_SW_CLEAR_BUF_FULL_MASK                     0x1
#define DAI_SW_CLEAR_BUF_FULL_MASK_SFT                 (0x1 << 15)
#define DAI_DUPLICATE_WR_SFT                           10
#define DAI_DUPLICATE_WR_MASK                          0x1
#define DAI_DUPLICATE_WR_MASK_SFT                      (0x1 << 10)
#define DAI_MONO_SFT                                   8
#define DAI_MONO_MASK                                  0x1
#define DAI_MONO_MASK_SFT                              (0x1 << 8)
#define DAI_WR_SIGN_SFT                                6
#define DAI_WR_SIGN_MASK                               0x1
#define DAI_WR_SIGN_MASK_SFT                           (0x1 << 6)
#define DAI_NORMAL_MODE_SFT                            5
#define DAI_NORMAL_MODE_MASK                           0x1
#define DAI_NORMAL_MODE_MASK_SFT                       (0x1 << 5)
#define DAI_HALIGN_SFT                                 4
#define DAI_HALIGN_MASK                                0x1
#define DAI_HALIGN_MASK_SFT                            (0x1 << 4)
#define DAI_HD_MODE_SFT                                0
#define DAI_HD_MODE_MASK                               0x3
#define DAI_HD_MODE_MASK_SFT                           (0x3 << 0)

/* AFE_MOD_DAI_CON0 */
#define MOD_DAI_MODE_SFT                               24
#define MOD_DAI_MODE_MASK                              0x3
#define MOD_DAI_MODE_MASK_SFT                          (0x3 << 24)
#define MOD_DAI_SW_CLEAR_BUF_FULL_SFT                  15
#define MOD_DAI_SW_CLEAR_BUF_FULL_MASK                 0x1
#define MOD_DAI_SW_CLEAR_BUF_FULL_MASK_SFT             (0x1 << 15)
#define MOD_DAI_DUPLICATE_WR_SFT                       10
#define MOD_DAI_DUPLICATE_WR_MASK                      0x1
#define MOD_DAI_DUPLICATE_WR_MASK_SFT                  (0x1 << 10)
#define MOD_DAI_MONO_SFT                               8
#define MOD_DAI_MONO_MASK                              0x1
#define MOD_DAI_MONO_MASK_SFT                          (0x1 << 8)
#define MOD_DAI_WR_SIGN_SFT                            6
#define MOD_DAI_WR_SIGN_MASK                           0x1
#define MOD_DAI_WR_SIGN_MASK_SFT                       (0x1 << 6)
#define MOD_DAI_NORMAL_MODE_SFT                        5
#define MOD_DAI_NORMAL_MODE_MASK                       0x1
#define MOD_DAI_NORMAL_MODE_MASK_SFT                   (0x1 << 5)
#define MOD_DAI_HALIGN_SFT                             4
#define MOD_DAI_HALIGN_MASK                            0x1
#define MOD_DAI_HALIGN_MASK_SFT                        (0x1 << 4)
#define MOD_DAI_HD_MODE_SFT                            0
#define MOD_DAI_HD_MODE_MASK                           0x3
#define MOD_DAI_HD_MODE_MASK_SFT                       (0x3 << 0)

/* AFE_DAI2_CON0 */
#define DAI2_MODE_SFT                                  24
#define DAI2_MODE_MASK                                 0xf
#define DAI2_MODE_MASK_SFT                             (0xf << 24)
#define DAI2_SW_CLEAR_BUF_FULL_SFT                     15
#define DAI2_SW_CLEAR_BUF_FULL_MASK                    0x1
#define DAI2_SW_CLEAR_BUF_FULL_MASK_SFT                (0x1 << 15)
#define DAI2_DUPLICATE_WR_SFT                          10
#define DAI2_DUPLICATE_WR_MASK                         0x1
#define DAI2_DUPLICATE_WR_MASK_SFT                     (0x1 << 10)
#define DAI2_MONO_SFT                                  8
#define DAI2_MONO_MASK                                 0x1
#define DAI2_MONO_MASK_SFT                             (0x1 << 8)
#define DAI2_WR_SIGN_SFT                               6
#define DAI2_WR_SIGN_MASK                              0x1
#define DAI2_WR_SIGN_MASK_SFT                          (0x1 << 6)
#define DAI2_NORMAL_MODE_SFT                           5
#define DAI2_NORMAL_MODE_MASK                          0x1
#define DAI2_NORMAL_MODE_MASK_SFT                      (0x1 << 5)
#define DAI2_HALIGN_SFT                                4
#define DAI2_HALIGN_MASK                               0x1
#define DAI2_HALIGN_MASK_SFT                           (0x1 << 4)
#define DAI2_HD_MODE_SFT                               0
#define DAI2_HD_MODE_MASK                              0x3
#define DAI2_HD_MODE_MASK_SFT                          (0x3 << 0)

/* AFE_MEMIF_CON0 */
#define CPU_COMPACT_MODE_SFT                           2
#define CPU_COMPACT_MODE_MASK                          0x1
#define CPU_COMPACT_MODE_MASK_SFT                      (0x1 << 2)
#define CPU_HD_ALIGN_SFT                               1
#define CPU_HD_ALIGN_MASK                              0x1
#define CPU_HD_ALIGN_MASK_SFT                          (0x1 << 1)
#define SYSRAM_SIGN_SFT                                0
#define SYSRAM_SIGN_MASK                               0x1
#define SYSRAM_SIGN_MASK_SFT                           (0x1 << 0)

/* AFE_IRQ_MCU_CON0 */
#define IRQ31_MCU_ON_SFT                               31
#define IRQ31_MCU_ON_MASK                              0x1
#define IRQ31_MCU_ON_MASK_SFT                          (0x1 << 31)
#define IRQ26_MCU_ON_SFT                               26
#define IRQ26_MCU_ON_MASK                              0x1
#define IRQ26_MCU_ON_MASK_SFT                          (0x1 << 26)
#define IRQ25_MCU_ON_SFT                               25
#define IRQ25_MCU_ON_MASK                              0x1
#define IRQ25_MCU_ON_MASK_SFT                          (0x1 << 25)
#define IRQ24_MCU_ON_SFT                               24
#define IRQ24_MCU_ON_MASK                              0x1
#define IRQ24_MCU_ON_MASK_SFT                          (0x1 << 24)
#define IRQ23_MCU_ON_SFT                               23
#define IRQ23_MCU_ON_MASK                              0x1
#define IRQ23_MCU_ON_MASK_SFT                          (0x1 << 23)
#define IRQ22_MCU_ON_SFT                               22
#define IRQ22_MCU_ON_MASK                              0x1
#define IRQ22_MCU_ON_MASK_SFT                          (0x1 << 22)
#define IRQ21_MCU_ON_SFT                               21
#define IRQ21_MCU_ON_MASK                              0x1
#define IRQ21_MCU_ON_MASK_SFT                          (0x1 << 21)
#define IRQ20_MCU_ON_SFT                               20
#define IRQ20_MCU_ON_MASK                              0x1
#define IRQ20_MCU_ON_MASK_SFT                          (0x1 << 20)
#define IRQ19_MCU_ON_SFT                               19
#define IRQ19_MCU_ON_MASK                              0x1
#define IRQ19_MCU_ON_MASK_SFT                          (0x1 << 19)
#define IRQ18_MCU_ON_SFT                               18
#define IRQ18_MCU_ON_MASK                              0x1
#define IRQ18_MCU_ON_MASK_SFT                          (0x1 << 18)
#define IRQ17_MCU_ON_SFT                               17
#define IRQ17_MCU_ON_MASK                              0x1
#define IRQ17_MCU_ON_MASK_SFT                          (0x1 << 17)
#define IRQ16_MCU_ON_SFT                               16
#define IRQ16_MCU_ON_MASK                              0x1
#define IRQ16_MCU_ON_MASK_SFT                          (0x1 << 16)
#define IRQ15_MCU_ON_SFT                               15
#define IRQ15_MCU_ON_MASK                              0x1
#define IRQ15_MCU_ON_MASK_SFT                          (0x1 << 15)
#define IRQ14_MCU_ON_SFT                               14
#define IRQ14_MCU_ON_MASK                              0x1
#define IRQ14_MCU_ON_MASK_SFT                          (0x1 << 14)
#define IRQ13_MCU_ON_SFT                               13
#define IRQ13_MCU_ON_MASK                              0x1
#define IRQ13_MCU_ON_MASK_SFT                          (0x1 << 13)
#define IRQ12_MCU_ON_SFT                               12
#define IRQ12_MCU_ON_MASK                              0x1
#define IRQ12_MCU_ON_MASK_SFT                          (0x1 << 12)
#define IRQ11_MCU_ON_SFT                               11
#define IRQ11_MCU_ON_MASK                              0x1
#define IRQ11_MCU_ON_MASK_SFT                          (0x1 << 11)
#define IRQ10_MCU_ON_SFT                               10
#define IRQ10_MCU_ON_MASK                              0x1
#define IRQ10_MCU_ON_MASK_SFT                          (0x1 << 10)
#define IRQ9_MCU_ON_SFT                                9
#define IRQ9_MCU_ON_MASK                               0x1
#define IRQ9_MCU_ON_MASK_SFT                           (0x1 << 9)
#define IRQ8_MCU_ON_SFT                                8
#define IRQ8_MCU_ON_MASK                               0x1
#define IRQ8_MCU_ON_MASK_SFT                           (0x1 << 8)
#define IRQ7_MCU_ON_SFT                                7
#define IRQ7_MCU_ON_MASK                               0x1
#define IRQ7_MCU_ON_MASK_SFT                           (0x1 << 7)
#define IRQ6_MCU_ON_SFT                                6
#define IRQ6_MCU_ON_MASK                               0x1
#define IRQ6_MCU_ON_MASK_SFT                           (0x1 << 6)
#define IRQ5_MCU_ON_SFT                                5
#define IRQ5_MCU_ON_MASK                               0x1
#define IRQ5_MCU_ON_MASK_SFT                           (0x1 << 5)
#define IRQ4_MCU_ON_SFT                                4
#define IRQ4_MCU_ON_MASK                               0x1
#define IRQ4_MCU_ON_MASK_SFT                           (0x1 << 4)
#define IRQ3_MCU_ON_SFT                                3
#define IRQ3_MCU_ON_MASK                               0x1
#define IRQ3_MCU_ON_MASK_SFT                           (0x1 << 3)
#define IRQ2_MCU_ON_SFT                                2
#define IRQ2_MCU_ON_MASK                               0x1
#define IRQ2_MCU_ON_MASK_SFT                           (0x1 << 2)
#define IRQ1_MCU_ON_SFT                                1
#define IRQ1_MCU_ON_MASK                               0x1
#define IRQ1_MCU_ON_MASK_SFT                           (0x1 << 1)
#define IRQ0_MCU_ON_SFT                                0
#define IRQ0_MCU_ON_MASK                               0x1
#define IRQ0_MCU_ON_MASK_SFT                           (0x1 << 0)

/* AFE_IRQ_MCU_CON1 */
#define IRQ7_MCU_MODE_SFT                              28
#define IRQ7_MCU_MODE_MASK                             0xf
#define IRQ7_MCU_MODE_MASK_SFT                         (0xf << 28)
#define IRQ6_MCU_MODE_SFT                              24
#define IRQ6_MCU_MODE_MASK                             0xf
#define IRQ6_MCU_MODE_MASK_SFT                         (0xf << 24)
#define IRQ5_MCU_MODE_SFT                              20
#define IRQ5_MCU_MODE_MASK                             0xf
#define IRQ5_MCU_MODE_MASK_SFT                         (0xf << 20)
#define IRQ4_MCU_MODE_SFT                              16
#define IRQ4_MCU_MODE_MASK                             0xf
#define IRQ4_MCU_MODE_MASK_SFT                         (0xf << 16)
#define IRQ3_MCU_MODE_SFT                              12
#define IRQ3_MCU_MODE_MASK                             0xf
#define IRQ3_MCU_MODE_MASK_SFT                         (0xf << 12)
#define IRQ2_MCU_MODE_SFT                              8
#define IRQ2_MCU_MODE_MASK                             0xf
#define IRQ2_MCU_MODE_MASK_SFT                         (0xf << 8)
#define IRQ1_MCU_MODE_SFT                              4
#define IRQ1_MCU_MODE_MASK                             0xf
#define IRQ1_MCU_MODE_MASK_SFT                         (0xf << 4)
#define IRQ0_MCU_MODE_SFT                              0
#define IRQ0_MCU_MODE_MASK                             0xf
#define IRQ0_MCU_MODE_MASK_SFT                         (0xf << 0)

/* AFE_IRQ_MCU_CON2 */
#define IRQ15_MCU_MODE_SFT                             28
#define IRQ15_MCU_MODE_MASK                            0xf
#define IRQ15_MCU_MODE_MASK_SFT                        (0xf << 28)
#define IRQ14_MCU_MODE_SFT                             24
#define IRQ14_MCU_MODE_MASK                            0xf
#define IRQ14_MCU_MODE_MASK_SFT                        (0xf << 24)
#define IRQ13_MCU_MODE_SFT                             20
#define IRQ13_MCU_MODE_MASK                            0xf
#define IRQ13_MCU_MODE_MASK_SFT                        (0xf << 20)
#define IRQ12_MCU_MODE_SFT                             16
#define IRQ12_MCU_MODE_MASK                            0xf
#define IRQ12_MCU_MODE_MASK_SFT                        (0xf << 16)
#define IRQ11_MCU_MODE_SFT                             12
#define IRQ11_MCU_MODE_MASK                            0xf
#define IRQ11_MCU_MODE_MASK_SFT                        (0xf << 12)
#define IRQ10_MCU_MODE_SFT                             8
#define IRQ10_MCU_MODE_MASK                            0xf
#define IRQ10_MCU_MODE_MASK_SFT                        (0xf << 8)
#define IRQ9_MCU_MODE_SFT                              4
#define IRQ9_MCU_MODE_MASK                             0xf
#define IRQ9_MCU_MODE_MASK_SFT                         (0xf << 4)
#define IRQ8_MCU_MODE_SFT                              0
#define IRQ8_MCU_MODE_MASK                             0xf
#define IRQ8_MCU_MODE_MASK_SFT                         (0xf << 0)

/* AFE_IRQ_MCU_CON3 */
#define IRQ23_MCU_MODE_SFT                             28
#define IRQ23_MCU_MODE_MASK                            0xf
#define IRQ23_MCU_MODE_MASK_SFT                        (0xf << 28)
#define IRQ22_MCU_MODE_SFT                             24
#define IRQ22_MCU_MODE_MASK                            0xf
#define IRQ22_MCU_MODE_MASK_SFT                        (0xf << 24)
#define IRQ21_MCU_MODE_SFT                             20
#define IRQ21_MCU_MODE_MASK                            0xf
#define IRQ21_MCU_MODE_MASK_SFT                        (0xf << 20)
#define IRQ20_MCU_MODE_SFT                             16
#define IRQ20_MCU_MODE_MASK                            0xf
#define IRQ20_MCU_MODE_MASK_SFT                        (0xf << 16)
#define IRQ19_MCU_MODE_SFT                             12
#define IRQ19_MCU_MODE_MASK                            0xf
#define IRQ19_MCU_MODE_MASK_SFT                        (0xf << 12)
#define IRQ18_MCU_MODE_SFT                             8
#define IRQ18_MCU_MODE_MASK                            0xf
#define IRQ18_MCU_MODE_MASK_SFT                        (0xf << 8)
#define IRQ17_MCU_MODE_SFT                             4
#define IRQ17_MCU_MODE_MASK                            0xf
#define IRQ17_MCU_MODE_MASK_SFT                        (0xf << 4)
#define IRQ16_MCU_MODE_SFT                             0
#define IRQ16_MCU_MODE_MASK                            0xf
#define IRQ16_MCU_MODE_MASK_SFT                        (0xf << 0)

/* AFE_IRQ_MCU_CON4 */
#define IRQ26_MCU_MODE_SFT                             8
#define IRQ26_MCU_MODE_MASK                            0xf
#define IRQ26_MCU_MODE_MASK_SFT                        (0xf << 8)
#define IRQ25_MCU_MODE_SFT                             4
#define IRQ25_MCU_MODE_MASK                            0xf
#define IRQ25_MCU_MODE_MASK_SFT                        (0xf << 4)
#define IRQ24_MCU_MODE_SFT                             0
#define IRQ24_MCU_MODE_MASK                            0xf
#define IRQ24_MCU_MODE_MASK_SFT                        (0xf << 0)

/* AFE_IRQ_MCU_CLR */
#define IRQ31_MCU_CLR_SFT                              31
#define IRQ31_MCU_CLR_MASK                             0x1
#define IRQ31_MCU_CLR_MASK_SFT                         (0x1 << 31)
#define IRQ26_MCU_CLR_SFT                              26
#define IRQ26_MCU_CLR_MASK                             0x1
#define IRQ26_MCU_CLR_MASK_SFT                         (0x1 << 26)
#define IRQ25_MCU_CLR_SFT                              25
#define IRQ25_MCU_CLR_MASK                             0x1
#define IRQ25_MCU_CLR_MASK_SFT                         (0x1 << 25)
#define IRQ24_MCU_CLR_SFT                              24
#define IRQ24_MCU_CLR_MASK                             0x1
#define IRQ24_MCU_CLR_MASK_SFT                         (0x1 << 24)
#define IRQ23_MCU_CLR_SFT                              23
#define IRQ23_MCU_CLR_MASK                             0x1
#define IRQ23_MCU_CLR_MASK_SFT                         (0x1 << 23)
#define IRQ22_MCU_CLR_SFT                              22
#define IRQ22_MCU_CLR_MASK                             0x1
#define IRQ22_MCU_CLR_MASK_SFT                         (0x1 << 22)
#define IRQ21_MCU_CLR_SFT                              21
#define IRQ21_MCU_CLR_MASK                             0x1
#define IRQ21_MCU_CLR_MASK_SFT                         (0x1 << 21)
#define IRQ20_MCU_CLR_SFT                              20
#define IRQ20_MCU_CLR_MASK                             0x1
#define IRQ20_MCU_CLR_MASK_SFT                         (0x1 << 20)
#define IRQ19_MCU_CLR_SFT                              19
#define IRQ19_MCU_CLR_MASK                             0x1
#define IRQ19_MCU_CLR_MASK_SFT                         (0x1 << 19)
#define IRQ18_MCU_CLR_SFT                              18
#define IRQ18_MCU_CLR_MASK                             0x1
#define IRQ18_MCU_CLR_MASK_SFT                         (0x1 << 18)
#define IRQ17_MCU_CLR_SFT                              17
#define IRQ17_MCU_CLR_MASK                             0x1
#define IRQ17_MCU_CLR_MASK_SFT                         (0x1 << 17)
#define IRQ16_MCU_CLR_SFT                              16
#define IRQ16_MCU_CLR_MASK                             0x1
#define IRQ16_MCU_CLR_MASK_SFT                         (0x1 << 16)
#define IRQ15_MCU_CLR_SFT                              15
#define IRQ15_MCU_CLR_MASK                             0x1
#define IRQ15_MCU_CLR_MASK_SFT                         (0x1 << 15)
#define IRQ14_MCU_CLR_SFT                              14
#define IRQ14_MCU_CLR_MASK                             0x1
#define IRQ14_MCU_CLR_MASK_SFT                         (0x1 << 14)
#define IRQ13_MCU_CLR_SFT                              13
#define IRQ13_MCU_CLR_MASK                             0x1
#define IRQ13_MCU_CLR_MASK_SFT                         (0x1 << 13)
#define IRQ12_MCU_CLR_SFT                              12
#define IRQ12_MCU_CLR_MASK                             0x1
#define IRQ12_MCU_CLR_MASK_SFT                         (0x1 << 12)
#define IRQ11_MCU_CLR_SFT                              11
#define IRQ11_MCU_CLR_MASK                             0x1
#define IRQ11_MCU_CLR_MASK_SFT                         (0x1 << 11)
#define IRQ10_MCU_CLR_SFT                              10
#define IRQ10_MCU_CLR_MASK                             0x1
#define IRQ10_MCU_CLR_MASK_SFT                         (0x1 << 10)
#define IRQ9_MCU_CLR_SFT                               9
#define IRQ9_MCU_CLR_MASK                              0x1
#define IRQ9_MCU_CLR_MASK_SFT                          (0x1 << 9)
#define IRQ8_MCU_CLR_SFT                               8
#define IRQ8_MCU_CLR_MASK                              0x1
#define IRQ8_MCU_CLR_MASK_SFT                          (0x1 << 8)
#define IRQ7_MCU_CLR_SFT                               7
#define IRQ7_MCU_CLR_MASK                              0x1
#define IRQ7_MCU_CLR_MASK_SFT                          (0x1 << 7)
#define IRQ6_MCU_CLR_SFT                               6
#define IRQ6_MCU_CLR_MASK                              0x1
#define IRQ6_MCU_CLR_MASK_SFT                          (0x1 << 6)
#define IRQ5_MCU_CLR_SFT                               5
#define IRQ5_MCU_CLR_MASK                              0x1
#define IRQ5_MCU_CLR_MASK_SFT                          (0x1 << 5)
#define IRQ4_MCU_CLR_SFT                               4
#define IRQ4_MCU_CLR_MASK                              0x1
#define IRQ4_MCU_CLR_MASK_SFT                          (0x1 << 4)
#define IRQ3_MCU_CLR_SFT                               3
#define IRQ3_MCU_CLR_MASK                              0x1
#define IRQ3_MCU_CLR_MASK_SFT                          (0x1 << 3)
#define IRQ2_MCU_CLR_SFT                               2
#define IRQ2_MCU_CLR_MASK                              0x1
#define IRQ2_MCU_CLR_MASK_SFT                          (0x1 << 2)
#define IRQ1_MCU_CLR_SFT                               1
#define IRQ1_MCU_CLR_MASK                              0x1
#define IRQ1_MCU_CLR_MASK_SFT                          (0x1 << 1)
#define IRQ0_MCU_CLR_SFT                               0
#define IRQ0_MCU_CLR_MASK                              0x1
#define IRQ0_MCU_CLR_MASK_SFT                          (0x1 << 0)

/* AFE_IRQ_MCU_EN */
#define IRQ31_MCU_EN_SFT                               31
#define IRQ30_MCU_EN_SFT                               30
#define IRQ29_MCU_EN_SFT                               29
#define IRQ28_MCU_EN_SFT                               28
#define IRQ27_MCU_EN_SFT                               27
#define IRQ26_MCU_EN_SFT                               26
#define IRQ25_MCU_EN_SFT                               25
#define IRQ24_MCU_EN_SFT                               24
#define IRQ23_MCU_EN_SFT                               23
#define IRQ22_MCU_EN_SFT                               22
#define IRQ21_MCU_EN_SFT                               21
#define IRQ20_MCU_EN_SFT                               20
#define IRQ19_MCU_EN_SFT                               19
#define IRQ18_MCU_EN_SFT                               18
#define IRQ17_MCU_EN_SFT                               17
#define IRQ16_MCU_EN_SFT                               16
#define IRQ15_MCU_EN_SFT                               15
#define IRQ14_MCU_EN_SFT                               14
#define IRQ13_MCU_EN_SFT                               13
#define IRQ12_MCU_EN_SFT                               12
#define IRQ11_MCU_EN_SFT                               11
#define IRQ10_MCU_EN_SFT                               10
#define IRQ9_MCU_EN_SFT                                9
#define IRQ8_MCU_EN_SFT                                8
#define IRQ7_MCU_EN_SFT                                7
#define IRQ6_MCU_EN_SFT                                6
#define IRQ5_MCU_EN_SFT                                5
#define IRQ4_MCU_EN_SFT                                4
#define IRQ3_MCU_EN_SFT                                3
#define IRQ2_MCU_EN_SFT                                2
#define IRQ1_MCU_EN_SFT                                1
#define IRQ0_MCU_EN_SFT                                0

/* AFE_IRQ_MCU_SCP_EN */
#define IRQ31_MCU_SCP_EN_SFT                           31
#define IRQ30_MCU_SCP_EN_SFT                           30
#define IRQ29_MCU_SCP_EN_SFT                           29
#define IRQ28_MCU_SCP_EN_SFT                           28
#define IRQ27_MCU_SCP_EN_SFT                           27
#define IRQ26_MCU_SCP_EN_SFT                           26
#define IRQ25_MCU_SCP_EN_SFT                           25
#define IRQ24_MCU_SCP_EN_SFT                           24
#define IRQ23_MCU_SCP_EN_SFT                           23
#define IRQ22_MCU_SCP_EN_SFT                           22
#define IRQ21_MCU_SCP_EN_SFT                           21
#define IRQ20_MCU_SCP_EN_SFT                           20
#define IRQ19_MCU_SCP_EN_SFT                           19
#define IRQ18_MCU_SCP_EN_SFT                           18
#define IRQ17_MCU_SCP_EN_SFT                           17
#define IRQ16_MCU_SCP_EN_SFT                           16
#define IRQ15_MCU_SCP_EN_SFT                           15
#define IRQ14_MCU_SCP_EN_SFT                           14
#define IRQ13_MCU_SCP_EN_SFT                           13
#define IRQ12_MCU_SCP_EN_SFT                           12
#define IRQ11_MCU_SCP_EN_SFT                           11
#define IRQ10_MCU_SCP_EN_SFT                           10
#define IRQ9_MCU_SCP_EN_SFT                            9
#define IRQ8_MCU_SCP_EN_SFT                            8
#define IRQ7_MCU_SCP_EN_SFT                            7
#define IRQ6_MCU_SCP_EN_SFT                            6
#define IRQ5_MCU_SCP_EN_SFT                            5
#define IRQ4_MCU_SCP_EN_SFT                            4
#define IRQ3_MCU_SCP_EN_SFT                            3
#define IRQ2_MCU_SCP_EN_SFT                            2
#define IRQ1_MCU_SCP_EN_SFT                            1
#define IRQ0_MCU_SCP_EN_SFT                            0

/* AFE_IRQ_MCU_DSP_EN */
#define IRQ31_MCU_DSP_EN_SFT                           31
#define IRQ30_MCU_DSP_EN_SFT                           30
#define IRQ29_MCU_DSP_EN_SFT                           29
#define IRQ28_MCU_DSP_EN_SFT                           28
#define IRQ27_MCU_DSP_EN_SFT                           27
#define IRQ26_MCU_DSP_EN_SFT                           26
#define IRQ25_MCU_DSP_EN_SFT                           25
#define IRQ24_MCU_DSP_EN_SFT                           24
#define IRQ23_MCU_DSP_EN_SFT                           23
#define IRQ22_MCU_DSP_EN_SFT                           22
#define IRQ21_MCU_DSP_EN_SFT                           21
#define IRQ20_MCU_DSP_EN_SFT                           20
#define IRQ19_MCU_DSP_EN_SFT                           19
#define IRQ18_MCU_DSP_EN_SFT                           18
#define IRQ17_MCU_DSP_EN_SFT                           17
#define IRQ16_MCU_DSP_EN_SFT                           16
#define IRQ15_MCU_DSP_EN_SFT                           15
#define IRQ14_MCU_DSP_EN_SFT                           14
#define IRQ13_MCU_DSP_EN_SFT                           13
#define IRQ12_MCU_DSP_EN_SFT                           12
#define IRQ11_MCU_DSP_EN_SFT                           11
#define IRQ10_MCU_DSP_EN_SFT                           10
#define IRQ9_MCU_DSP_EN_SFT                            9
#define IRQ8_MCU_DSP_EN_SFT                            8
#define IRQ7_MCU_DSP_EN_SFT                            7
#define IRQ6_MCU_DSP_EN_SFT                            6
#define IRQ5_MCU_DSP_EN_SFT                            5
#define IRQ4_MCU_DSP_EN_SFT                            4
#define IRQ3_MCU_DSP_EN_SFT                            3
#define IRQ2_MCU_DSP_EN_SFT                            2
#define IRQ1_MCU_DSP_EN_SFT                            1
#define IRQ0_MCU_DSP_EN_SFT                            0

#define AFE_IRQ_STATUS_BITS 0x87FFFFFF
#define AFE_IRQ_CNT_SHIFT 0
#define AFE_IRQ_CNT_MASK 0x3ffff

#endif
