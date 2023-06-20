/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 AMD.All rights reserved.
 *
 * Author:      Basavaraj Hiregoudar <basavaraj.hiregoudar@amd.com>
 *              SaiSurya, Ch <saisurya.chakkaveeravenkatanaga@amd.com>
 */
#ifndef _VANGOGH_OFFSET_HEADER
#define _VANGOGH_OFFSET_HEADER

#define PU_REGISTER_BASE        (0x9FD00000 - 0x01240000)
#define PU_SCRATCH_REG_BASE	(0x9FF00000 - 0x01250000)

/* Registers from ACP_DMA block */
#define ACP_DMA_CNTL_0                                0x1240000
#define ACP_DMA_DESC_BASE_ADDR                        0x12400E0
#define ACP_DMA_DESC_MAX_NUM_DSCR                     0x12400E4
#define ACP_DMA_CH_STS                                0x12400E8

#define ACP_DMA_DSCR_STRT_IDX_0                       0x1240020
#define ACP_DMA_DSCR_CNT_0                            0x1240040
#define ACP_DMA_PRIO_0                                0x1240060

/* Registers from ACP_MISC block */
#define ACP_CLKMUX_SEL				      0x124142C
#define ACP_EXTERNAL_INTR_ENB                         0x1241800
#define ACP_EXTERNAL_INTR_STAT                        0x1241808
#define ACP_DSP0_INTR_CNTL                            0x124180C
#define ACP_DSP0_INTR_STAT                            0x1241810
#define ACP_DSP_SW_INTR_CNTL                          0x1241814
#define ACP_DSP_SW_INTR_STAT                          0x1241818
#define ACP_SW_INTR_TRIG                              0x124181C
#define ACP_SMU_MAILBOX                               0x1241820
#define DSP_INTERRUPT_ROUTING_CTRL_0		      0x1241824
#define DSP_INTERRUPT_ROUTING_CTRL_1                  0x1241828

#define ACP_TIMER                                     0x1241878
#define ACP_TIMER_CNTL                                0x124187C
#define ACP_AXI2DAGB_SEM_0			      0x1241884

#define ACP_SRBM_CLIENT_BASE_ADDR                     0x1241BBC
#define ACP_SRBM_CLIENT_RDDATA                        0x1241BC0
#define ACP_SRBM_CYCLE_STS                            0x1241BC4
#define ACP_SRBM_CLIENT_CONFIG                        0x1241BC8

/* Registers from ACP_AUDIO_BUFFERS block */
#define ACP_I2S_RX_RINGBUFADDR                        0x1242000
#define ACP_I2S_RX_RINGBUFSIZE                        0x1242004
#define ACP_I2S_RX_LINKPOSITIONCNTR                   0x1242008
#define ACP_I2S_RX_FIFOADDR                           0x124200C
#define ACP_I2S_RX_FIFOSIZE                           0x1242010
#define ACP_I2S_RX_DMA_SIZE                           0x1242014
#define ACP_I2S_RX_LINEARPOSITIONCNTR_HIGH            0x1242018
#define ACP_I2S_RX_LINEARPOSITIONCNTR_LOW             0x124201C
#define ACP_I2S_RX_INTR_WATERMARK_SIZE                0x1242020
#define ACP_I2S_TX_RINGBUFADDR                        0x1242024
#define ACP_I2S_TX_RINGBUFSIZE                        0x1242028
#define ACP_I2S_TX_LINKPOSITIONCNTR                   0x124202C
#define ACP_I2S_TX_FIFOADDR                           0x1242030
#define ACP_I2S_TX_FIFOSIZE                           0x1242034
#define ACP_I2S_TX_DMA_SIZE                           0x1242038
#define ACP_I2S_TX_LINEARPOSITIONCNTR_HIGH            0x124203C
#define ACP_I2S_TX_LINEARPOSITIONCNTR_LOW             0x1242040
#define ACP_I2S_TX_INTR_WATERMARK_SIZE                0x1242044
#define ACP_BT_RX_RINGBUFADDR                         0x1242048
#define ACP_BT_RX_RINGBUFSIZE                         0x124204C
#define ACP_BT_RX_LINKPOSITIONCNTR                    0x1242050
#define ACP_BT_RX_FIFOADDR                            0x1242054
#define ACP_BT_RX_FIFOSIZE                            0x1242058
#define ACP_BT_RX_DMA_SIZE                            0x124205C
#define ACP_BT_RX_LINEARPOSITIONCNTR_HIGH             0x1242060
#define ACP_BT_RX_LINEARPOSITIONCNTR_LOW              0x1242064
#define ACP_BT_RX_INTR_WATERMARK_SIZE                 0x1242068
#define ACP_BT_TX_RINGBUFADDR                         0x124206C
#define ACP_BT_TX_RINGBUFSIZE                         0x1242070
#define ACP_BT_TX_LINKPOSITIONCNTR                    0x1242074
#define ACP_BT_TX_FIFOADDR                            0x1242078
#define ACP_BT_TX_FIFOSIZE                            0x124207C
#define ACP_BT_TX_DMA_SIZE                            0x1242080
#define ACP_BT_TX_LINEARPOSITIONCNTR_HIGH             0x1242084
#define ACP_BT_TX_LINEARPOSITIONCNTR_LOW              0x1242088
#define ACP_BT_TX_INTR_WATERMARK_SIZE                 0x124208C
#define ACP_HS_RX_RINGBUFADDR			      0x1242090
#define ACP_HS_RX_RINGBUFSIZE			      0x1242094
#define ACP_HS_RX_LINKPOSITIONCNTR		      0x1242098
#define ACP_HS_RX_FIFOADDR			      0x124209C
#define ACP_HS_RX_FIFOSIZE			      0x12420A0
#define ACP_HS_RX_DMA_SIZE			      0x12420A4
#define ACP_HS_RX_LINEARPOSITIONCNTR_HIGH	      0x12420A8
#define ACP_HS_RX_LINEARPOSITIONCNTR_LOW	      0x12420AC
#define ACP_HS_RX_INTR_WATERMARK_SIZE		      0x12420B0
#define ACP_HS_TX_RINGBUFADDR			      0x12420B4
#define ACP_HS_TX_RINGBUFSIZE			      0x12420B8
#define ACP_HS_TX_LINKPOSITIONCNTR		      0x12420BC
#define ACP_HS_TX_FIFOADDR			      0x12420C0
#define ACP_HS_TX_FIFOSIZE			      0x12420C4
#define ACP_HS_TX_DMA_SIZE			      0x12420C8
#define ACP_HS_TX_LINEARPOSITIONCNTR_HIGH	      0x12420CC
#define ACP_HS_TX_LINEARPOSITIONCNTR_LOW	      0x12420D0
#define ACP_HS_TX_INTR_WATERMARK_SIZE		      0x12420D4

/* Registers from ACP_I2S_TDM block */
#define ACP_I2STDM_IER                                0x1242400
#define ACP_I2STDM_IRER                               0x1242404
#define ACP_I2STDM_RXFRMT                             0x1242408
#define ACP_I2STDM_ITER                               0x124240C
#define ACP_I2STDM_TXFRMT                             0x1242410
#define ACP_I2STDM0_MSTRCLKGEN			      0x1242414
#define ACP_I2STDM1_MSTRCLKGEN			      0x1242418
#define ACP_I2STDM2_MSTRCLKGEN                        0x124241C
#define ACP_I2STDM_REFCLKGEN			      0x1242420

/* Registers from ACP_BT_TDM block */
#define ACP_BTTDM_IER                                 0x1242800
#define ACP_BTTDM_IRER                                0x1242804
#define ACP_BTTDM_RXFRMT                              0x1242808
#define ACP_BTTDM_ITER                                0x124280C
#define ACP_BTTDM_TXFRMT                              0x1242810

/* Registers from ACP_HS_TDM block */
#define ACP_HSTDM_IER				      0x1242814
#define ACP_HSTDM_IRER				      0x1242818
#define ACP_HSTDM_RXFRMT			      0x124281C
#define ACP_HSTDM_ITER				      0x1242820
#define ACP_HSTDM_TXFRMT			      0x1242824

/* Registers from ACP_WOV block */
#define ACP_WOV_VAD_ENABLE                            0x1242C00
#define ACP_WOV_PDM_ENABLE                            0x1242C04
#define ACP_WOV_PDM_DMA_ENABLE                        0x1242C08
#define ACP_WOV_RX_RINGBUFADDR                        0x1242C0C
#define ACP_WOV_RX_RINGBUFSIZE                        0x1242C10
#define ACP_WOV_RX_LINKPOSITIONCNTR                   0x1242C14
#define ACP_WOV_RX_LINEARPOSITIONCNTR_HIGH            0x1242C18
#define ACP_WOV_RX_LINEARPOSITIONCNTR_LOW             0x1242C1C
#define ACP_WOV_RX_INTR_WATERMARK_SIZE                0x1242C20
#define ACP_WOV_PDM_FIFO_FLUSH                        0x1242C24
#define ACP_WOV_PDM_NO_OF_CHANNELS                    0x1242C28
#define ACP_WOV_PDM_DECIMATION_FACTOR                 0x1242C2C
#define ACP_WOV_PDM_VAD_CTRL                          0x1242C30

#define ACP_WOV_MISC_CTRL                             0x1242C5C
#define ACP_WOV_CLK_CTRL                              0x1242C60
#define ACP_PDM_VAD_DYNAMIC_CLK_GATING_EN             0x1242C64
#define ACP_WOV_ERROR_STATUS_REGISTER                 0x1242C68

#define MP1_SMN_C2PMSG_69                             0x58A14
#define MP1_SMN_C2PMSG_85                             0x58A54
#define MP1_SMN_C2PMSG_93                             0x58A74
#endif
