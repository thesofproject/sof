/*******************************************************************************
 * INTEL CONFIDENTIAL
 * Copyright 2022 Intel Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to the
 * source code ("Material") are owned by Intel Corporation or its suppliers or
 * licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material may contain trade secrets and
 * proprietary and confidential information of Intel Corporation and its
 * suppliers and licensors, and is protected by worldwide copyright and trade
 * secret laws and treaty provisions. No part of the Material may be used, copied,
 * reproduced, modified, published, uploaded, posted, transmitted, distributed,
 * or disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery of
 * the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be express
 * and approved by Intel in writing.
 *
 * *******************************************************************************/


/*
 * Platform: MTL
 * Source file:SIP_ACE1.x_Registers.xlsm
 */

#ifndef REGS_IPC_H_
#define REGS_IPC_H_

#include <stdint.h>


/**
 * HfIPCx
 * Host Inter-Processor Communication Registers
 *
 * Offset: 0007 3000h + 1000h * x + 400 0000h * f
 * These registers are for DSP inter processor communication with host CPU
 * through host root space registers.
 * Note: These registers are accessible through both the host space and
 * DSP space, as governed by SAI
 * and RS.
 */

#if !defined(_ASMLANGUAGE) && !defined(__ASSEMBLER__)

/**
 * DfIPCxTDR
 * DSP Inter Processor Communication Target Doorbell Request
 *
 * Offset: 000h
 * Block: HfIPCx
 *
 * This register is used for the Tensilica Core as a target to receive message
 * from IPC initiator.
 */
union DfIPCxTDR {
	uint32_t full;
	struct {
			/**
			 * Message
			 * type: RO/V, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Host CPU to Tensilica Core message when setting BUSY to 1 (mirrored
			 * from HIPCIDR.MSG field).
			 * Valid and static when BUSY=1.
			 */
			uint32_t      msg : 31;
			/**
			 * Busy
			 * type: RW/1C, rst: 0b, rst domain: gHUBULPRST
			 *
			 * Host CPU set this bit to initiate a message to Tensilica Core (when
			 * HIPCIDR.BUSY bit is written with the value of 1), and Tensilica Core
			 * clears the bit when accepted the message.
			 * Note:Clearing the BUSY bit only clear the interrupt source to
			 * Tensilica Core. Writing 0 to the DIPCTDA.BUSY bit causes a response
			 * message over internal wire to clear the BUSY bit
			 * on host CPU doorbell register.
			 */
			uint32_t     busy :  1;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxTDR) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxTDA
 * DSP Inter Processor Communication Target Doorbell Acknowledge
 *
 * Offset: 004h
 * Block: HfIPCx
 *
 * This register is used by the Tensilica Core to acknowledge the doorbell
 * request from the IPC initiator.
 */
union DfIPCxTDA {
	uint32_t full;
	struct {
			/**
			 * Message
			 * type: RW, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Tensilica Core response message when writing 0 to BUSY bit.
			 */
			uint32_t      msg : 31;
			/**
			 * Busy
			 * type: WO, rst: 0b, rst domain: nan
			 *
			 * Tensilica Core acknowledges the doorbell by writing 0 to this bit,
			 * with the response message in MSG field. Write 1 has no effect.
			 * Hardware clears the BUSY bit in HIPCIDR register when the response
			 * message has been sent on internal wire.
			 */
			uint32_t     busy :  1;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxTDA) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxIDR
 * DSP Inter Processor Communication Initiator Doorbell Request
 *
 * Offset: 010h
 * Block: HfIPCx
 *
 * This register is used for the Tensilica Core as an initiator to send message
 * to IPC target.
 */
union DfIPCxIDR {
	uint32_t full;
	struct {
			/**
			 * Message
			 * type: RW, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Tensilica Core to host CPU message when setting BUSY to 1.
			 */
			uint32_t      msg : 31;
			/**
			 * Busy
			 * type: RW/1S, rst: 0b, rst domain: gHUBULPRST
			 *
			 * When this bit is cleared, the host CPU is ready to accept a message.
			 * Tensilica Core set this bit to initiate a message to the host CPU,
			 * and the host CPU clears the bit when accepted the message (when
			 * HIPCTDA.BUSY bit is written with the value of 0).
			 */
			uint32_t     busy :  1;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxIDR) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxIDA
 * DSP Inter Processor Communication Initiator Doorbell Acknowledge
 *
 * Offset: 014h
 * Block: HfIPCx
 *
 * This register is used for the IPC target to acknowledge the doorbell request
 * from Tensilica Core as an initiator.
 */
union DfIPCxIDA {
	uint32_t full;
	struct {
			/**
			 * Message
			 * type: RO/V, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Host CPU response message when clearing BUSY to 0 (mirrored from
			 * HIPCTDA.MSG field). Valid and static when DONE=1.
			 */
			uint32_t      msg : 31;
			/**
			 * Done
			 * type: RW/1C, rst: 0b, rst domain: gHUBULPRST
			 *
			 * When this bit is set, host CPU has completed the operation and requests
			 * attention from the Tensilica Core.
			 * Set when DIPCIDR.BUSY bit is cleared.
			 */
			uint32_t     done :  1;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxIDA) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxCST
 * DSP Inter Processor Communication Command and Status Transmit
 *
 * Offset: 020h
 * Block: HfIPCx
 *
 * This register is used for the Tensilica Core to send a one way message to the
 * opposite IPC agent indicating its command and status.
 */
union DfIPCxCST {
	uint32_t full;
	struct {
			/**
			 * Command and Status
			 * type: RW/1S, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Tensilica Core to host CPU command and status.
			 * Hardware clears the bit when the command and status message has been
			 * sent on internal wire.
			 * Firmware should only set any of the bits only when the entire register
			 * is read as zero.
			 */
			uint32_t       cs : 32;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxCST) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxCSR
 * DSP Inter Processor Communication Command and Status Receive
 *
 * Offset: 024h
 * Block: HfIPCx
 *
 * This register is used for the Tensilica Core to receive a one way message from
 * the opposite IPC agent indicating its command and status.
 */
union DfIPCxCSR {
	uint32_t full;
	struct {
			/**
			 * Command and Status
			 * type: RW/1C, rst: 0000 0000h, rst domain: PLTRST
			 *
			 * Host CPU to Tensilica Core command and status.
			 * Hardware sets the bit when the command and status message has been
			 * received on internal wire (when HIPCCST register is written with the
			 * value of 1).
			 */
			uint32_t       cs : 32;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxCSR) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxCTL
 * DSP Inter Processor Communication Control
 *
 * Offset: 028h
 * Block: HfIPCx
 *
 * This register is used for the DSP to control the IPC operation such as
 * interrupt enable.
 */
union DfIPCxCTL {
	uint32_t full;
	struct {
			/**
			 * IPC Target Busy Interrupt Enable
			 * type: RW, rst: 0b, rst domain: DSPLRST
			 *
			 * When set to 1, it allows DIPCTDR.BUSY bit to propagate to cause a DSP
			 * interrupt.
			 */
			uint32_t  ipctbie :  1;
			/**
			 * IPC Initiator Done Interrupt Enable
			 * type: RW, rst: 0b, rst domain: DSPLRST
			 *
			 * When set to 1, it allows DIPCIDA.DONE bit to propagate to cause a DSP
			 * interrupt.
			 */
			uint32_t  ipcidie :  1;
			/**
			 * IPC Command and Status Received Interrupt Enable
			 * type: RW, rst: 0b, rst domain: DSPLRST
			 *
			 * When set to 1, it allows DIPCCSR register to propagate to cause a DSP
			 * interrupt.
			 */
			uint32_t ipccsrie :  1;
			/**
			 * Reserved
			 * type: RO, rst: 0000 0000h, rst domain: nan
			 */
			uint32_t   rsvd31 : 29;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxCTL) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxCAP
 * DSP Inter Processor Communication Capability
 *
 * Offset: 02Ch
 * Block: HfIPCx
 *
 * This register reports the capability of the IPC to Tensilica Core.
 */
union DfIPCxCAP {
	uint32_t full;
	struct {
			/**
			 * Payload Data Count
			 * type: RO, rst: IPCPDC-1, rst domain: nan
			 *
			 * Indicates the number of payload data DW count.
			 * 0-based value.
			 */
			uint32_t      pdc :  5;
			/**
			 * Reserved
			 * type: RO, rst: 0000 0000h, rst domain: nan
			 */
			uint32_t   rsvd31 : 27;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxCAP) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxTDDy
 * DSP Inter Processor Communication Target Doorbell Data y
 *
 * Offset: 100h + 4h * y
 * Block: HfIPCx
 *
 * This register is used for IPC initiator to send extended message data to the
 * Tensilica Core as a target.
 */
union DfIPCxTDDy {
	uint32_t full;
	struct {
			/**
			 * Message Extension
			 * type: RO/V, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Host CPU to Tensilica Core message extension (mirrored from HIPCIDDy
			 * register).
			 * Valid and static when DIPCTDR.BUSY=1.
			 */
			uint32_t   msgext : 32;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxTDDy) == sizeof(uint32_t), wrong_register_size);

/**
 * DfIPCxIDDy
 * DSP Inter Processor Communication Initiator Doorbell Data y
 *
 * Offset: 180h + 4h * y
 * Block: HfIPCx
 *
 * This register is used for the Tensilica Core as an initiator to send extended
 * message data to IPC target.
 */
union DfIPCxIDDy {
	uint32_t full;
	struct {
			/**
			 * Message Extension
			 * type: RW, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Tensilica Core to host CPU message extension.
			 * Firmware should program this field before setting the DIPCIDR.BUSY bit.
			 */
			uint32_t   msgext : 32;
	} part;
};
//STATIC_ASSERT(sizeof(DfIPCxIDDy) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxTDR
 * Host Inter Processor Communication Target Doorbell Request
 *
 * Offset: 200h
 * Block: HfIPCx
 *
 * This register is used for Tensilica Core to send messages to the host CPU as
 * a target.
 */
union HfIPCxTDR {
	uint32_t full;
	struct {
			/**
			 * Message
			 * type: RO/V, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Tensilica Core to host CPU message when setting BUSY to 1 (mirrored
			 * from DIPCIDR.MSG field).
			 * Valid and static when BUSY = 1 .
			 */
			uint32_t      msg : 31;
			/**
			 * Busy
			 * type: RW/1C, rst: 0b, rst domain: gHUBULPRST
			 *
			 * Tensilica Core set this bit to initiate a message to host CPU (when
			 * DIPCIDR.BUSY bit is written with the value of 1), and host CPU clears
			 * the bit when accepted the message.
			 * Note:Clearing the BUSY bit only clear the interrupt source to host CPU.
			 * Writing 0 to the HIPCTDA.BUSY bit causes a response message over
			 * internal wire to clear the BUSY bit on Tensilica Core doorbell register.
			 */
			uint32_t     busy :  1;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxTDR) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxTDA
 * Host Inter Processor Communication Target Acknowledge
 *
 * Offset: 204h
 * Block: HfIPCx
 *
 * This register is used by the host CPU to acknowledge the doorbell request from
 * the Tensilica Core.
 */
union HfIPCxTDA {
	uint32_t full;
	struct {
			/**
			 * Message
			 * type: RW, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Host CPU response message when writing 0 to BUSY bit.
			 */
			uint32_t      msg : 31;
			/**
			 * Busy
			 * type: WO, rst: 0b, rst domain: nan
			 *
			 * Host CPU acknowledges the doorbell by writing 0 to this bit, with the
			 * response message in MSG field.
			 * Write 1 has no effect.
			 * Hardware clears the BUSY bit in DIPCIDR register when the response
			 * message has been sent on internal wire.
			 */
			uint32_t     busy :  1;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxTDA) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxIDR
 * Host Inter Processor Communication Initiator Doorbell Request
 *
 * Offset: 210h
 * Block: HfIPCx
 *
 * This register is used for the host CPU as an initiator to send messages to
 * Tensilica Core.
 */
union HfIPCxIDR {
	uint32_t full;
	struct {
			/**
			 * Message
			 * type: RW, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Host CPU to Tensilica Core message when setting BUSY to 1.
			 */
			uint32_t      msg : 31;
			/**
			 * Busy
			 * type: RW/1S, rst: 0b, rst domain: gHUBULPRST
			 *
			 * When this bit is cleared, the Tensilica Core is ready to accept a
			 * message.  Host CPU set this bit to initiate a message to Tensilica
			 * Core, and Tensilica Core will clear the bit when accepted the
			 * message (when DIPCTDA.BUSY bit is written with the value of 0).
			 */
			uint32_t     busy :  1;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxIDR) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxIDA
 * Host Inter Processor Communication Initiator Doorbell Acknowledge
 *
 * Offset: 214h
 * Block: HfIPCx
 *
 * This register is used for Tensilica Core to acknowledge the doorbell request
 * from the host CPU as an initiator.
 */
union HfIPCxIDA {
	uint32_t full;
	struct {
			/**
			 * Message
			 * type: RO/V, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Tensilica Core response message when clearing BUSY to 0 (mirrored
			 * from DIPCTDA.MSG field).
			 * Valid and static when DONE=1.
			 */
			uint32_t      msg : 31;
			/**
			 * Done
			 * type: RW/1C, rst: 0b, rst domain: gHUBULPRST
			 *
			 * When this bit is set, the Tensilica Core has completed the operation
			 * and requests attention from the host CPU.
			 * Set when HIPCIDR.BUSY bit is cleared.
			 */
			uint32_t     done :  1;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxIDA) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxCST
 * Host Inter Processor Communication Command and Status Transmit
 *
 * Offset: 220h
 * Block: HfIPCx
 *
 * This register is used for the host CPU to send a one way message to the
 * Tensilica Core indicating its command and status.
 */
union HfIPCxCST {
	uint32_t full;
	struct {
			/**
			 * Command and Status
			 * type: RW/1S, rst: 0000 0000h, rst domain: DSPLRST
			 *
			 * Host CPU to Tensilica Core command and status.
			 * Hardware clears the bit when the command and status message has been
			 * sent on internal wire.
			 * SW should only set any of the bits only when the entire register is
			 * read as zero.
			 */
			uint32_t       cs : 32;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxCST) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxCSR
 * Host Inter Processor Communication Command and Status Receive
 *
 * Offset: 224h
 * Block: HfIPCx
 *
 * This register is used for the host CPU to receive a one way message from the
 * Tensilica Core indicating its command and status.
 */
union HfIPCxCSR {
	uint32_t full;
	struct {
			/**
			 * Command and Status
			 * type: RW/1C, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Tensilica Core to host CPU command and status.
			 * Hardware sets the bit when the command and status message has been
			 * received on internal wire (when DIPCCST register is written with the
			 * value of 1).
			 */
			uint32_t       cs : 32;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxCSR) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxCTL
 * Host Inter Processor Communication Control
 *
 * Offset: 228h
 * Block: HfIPCx
 *
 * This register is used for the host CPU to control the IPC operation such as
 * interrupt enable.
 */
union HfIPCxCTL {
	uint32_t full;
	struct {
			/**
			 * IPC Target Busy Interrupt Enable
			 * type: RW, rst: 0b, rst domain: DSPLRST
			 *
			 * When set to 1, it allows HIPCT.BUSY bit to propagate to cause a host
			 * CPU interrupt.
			 */
			uint32_t  ipctbie :  1;
			/**
			 * IPC Initiator Done Interrupt Enable
			 * type: RW, rst: 0b, rst domain: DSPLRST
			 *
			 * When set to 1, it allows HIPCIE.DONE bit to propagate to cause a host
			 * CPU interrupt.
			 */
			uint32_t  ipcidie :  1;
			/**
			 * IPC Command and Status Received Interrupt Enable
			 * type: RW, rst: 0b, rst domain: DSPLRST
			 *
			 * When set to 1, it allows HIPCCSR register to propagate to for cause
			 * a host CPU interrupt.
			 */
			uint32_t ipccsrie :  1;
			/**
			 * Reserved
			 * type: RO, rst: 0000 0000h, rst domain: nan
			 */
			uint32_t   rsvd31 : 29;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxCTL) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxCAP
 * Host Inter Processor Communication Capability
 *
 * Offset: 22Ch
 * Block: HfIPCx
 *
 * This register reports the capability of the IPC to host CPU.
 */
union HfIPCxCAP {
	uint32_t full;
	struct {
			/**
			 * Payload Data Count
			 * type: RO, rst: IPCPDC-1, rst domain: nan
			 *
			 * Indicates the number of payload data DW count.
			 * 0-based value.
			 */
			uint32_t      pdc :  5;
			/**
			 * Reserved
			 * type: RO, rst: 0000 0000h, rst domain: nan
			 */
			uint32_t   rsvd31 : 27;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxCAP) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxTDDy
 * Host Inter Processor Communication Target Doorbell Data y
 *
 * Offset: 300h + 4h * y
 * Block: HfIPCx
 *
 * This register is used for Tensilica Core to send extended message data to the
 * host CPU as a target.
 */
union HfIPCxTDDy {
	uint32_t full;
	struct {
			/**
			 * Message Extension
			 * type: RO/V, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Tensilica Core to host CPU message extension (mirrored from DIPCIDDy
			 * register).
			 * Valid and static when HIPCTDR.BUSY = 1.
			 */
			uint32_t   msgext : 32;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxTDDy) == sizeof(uint32_t), wrong_register_size);

/**
 * HfIPCxIDDy
 * Host Inter Processor Communication Initiator Doorbell Data y
 *
 * Offset: 380h + 4h * y
 * Block: HfIPCx
 *
 * This register is used for the host CPU as an initiator to send extended message
 * data to Tensilica Core.
 */
union HfIPCxIDDy {
	uint32_t full;
	struct {
			/**
			 * Message Extension
			 * type: RW, rst: 0000 0000h, rst domain: gHUBULPRST
			 *
			 * Host CPU to Tensilica Core message extension.
			 * SW should program this field before setting the HIPCIDR.BUSY bit.
			 */
			uint32_t   msgext : 32;
	} part;
};
//STATIC_ASSERT(sizeof(HfIPCxIDDy) == sizeof(uint32_t), wrong_register_size);


#endif  /* (!defined(_ASMLANGUAGE) && !defined(__ASSEMBLER__)) */
#endif  /* REGS_IPC_H_*/
