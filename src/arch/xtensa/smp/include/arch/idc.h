/*
 * Copyright (c) 2018, Intel Corporation
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
 * Author: Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

/**
 * \file arch/xtensa/smp/include/arch/idc.h
 * \brief Xtensa SMP architecture IDC header file
 * \authors Tomasz Lauda <tomasz.lauda@linux.intel.com>
 */

#ifndef __ARCH_IDC_H__
#define __ARCH_IDC_H__

#include <xtos-structs.h>
#include <arch/cpu.h>
#include <platform/interrupt.h>
#include <platform/platform.h>
#include <sof/alloc.h>
#include <sof/ipc.h>
#include <sof/lock.h>
#include <sof/trace.h>

/** \brief IDC trace function. */
#define trace_idc(__e)	trace_event(TRACE_CLASS_IDC, __e)

/** \brief IDC trace value function. */
#define tracev_idc(__e)	tracev_event(TRACE_CLASS_IDC, __e)

/** \brief IDC trace error function. */
#define trace_idc_error(__e)	trace_error(TRACE_CLASS_IDC, __e)


/** \brief IDC send blocking flag. */
#define IDC_BLOCKING		0

/** \brief IDC send non-blocking flag. */
#define IDC_NON_BLOCKING	1

/** \brief IDC send timeout in cycles. */
#define IDC_TIMEOUT	800000

/** \brief ROM wake version parsed by ROM during core wake up. */
#define IDC_ROM_WAKE_VERSION	0x2

/** \brief IDC message type. */
#define IDC_TYPE_SHIFT		24
#define IDC_TYPE_MASK		0x7f
#define IDC_TYPE(x)		(((x) & IDC_TYPE_MASK) << IDC_TYPE_SHIFT)

/** \brief IDC message header. */
#define IDC_HEADER_MASK		0xffffff
#define IDC_HEADER(x)		((x) & IDC_HEADER_MASK)

/** \brief IDC message extension. */
#define IDC_EXTENSION_MASK	0x3fffffff
#define IDC_EXTENSION(x)	((x) & IDC_EXTENSION_MASK)

/** \brief IDC power up message. */
#define IDC_MSG_POWER_UP	(IDC_TYPE(0x1) | \
					IDC_HEADER(IDC_ROM_WAKE_VERSION))
#define IDC_MSG_POWER_UP_EXT	IDC_EXTENSION(SOF_TEXT_START >> 2)

/** \brief IDC power down message. */
#define IDC_MSG_POWER_DOWN	IDC_TYPE(0x2)
#define IDC_MSG_POWER_DOWN_EXT	IDC_EXTENSION(0x0)

/** \brief IDC trigger pipeline message. */
#define IDC_MSG_PPL_COMP_SHIFT		4
#define IDC_MSG_PPL_COMP(x)		((x) << IDC_MSG_PPL_COMP_SHIFT)
#define IDC_MSG_PPL_CMD_MASK		0xf
#define IDC_MSG_PPL_CMD(x)		((x) & IDC_MSG_PPL_CMD_MASK)
#define IDC_MSG_PPL_TRIGGER		IDC_TYPE(0x3)
#define IDC_MSG_PPL_TRIGGER_EXT(x, y)	IDC_EXTENSION( \
						IDC_MSG_PPL_COMP(x) | \
						IDC_MSG_PPL_CMD(y))

/** \brief Decodes IDC message type. */
#define iTS(x)	(((x) >> IDC_TYPE_SHIFT) & IDC_TYPE_MASK)

/** \brief Decodes component id from IDC trigger pipeline message. */
#define iPTComp(x)	((x) >> IDC_MSG_PPL_COMP_SHIFT)

/** \brief Decodes command from IDC trigger pipeline message. */
#define iPTCommand(x)	((x) & IDC_MSG_PPL_CMD_MASK)

/** \brief IDC message. */
struct idc_msg {
	uint32_t header;	/**< header value */
	uint32_t extension;	/**< extension value */
	uint32_t core;		/**< core id */
};

/** \brief IDC data. */
struct idc {
	spinlock_t lock;		/**< lock mechanism */
	uint32_t busy_bit_mask;		/**< busy interrupt mask */
	uint32_t done_bit_mask;		/**< done interrupt mask */
	uint32_t msg_pending;		/**< is message pending */
	struct idc_msg received_msg;	/**< received message */
};

extern struct ipc *_ipc;
extern void cpu_power_down_core(void);

/**
 * \brief Returns IDC data.
 * \return Pointer to pointer of IDC data.
 */
static inline struct idc **idc_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->idc;
}

static inline void idc_enable_interrupts(int target_core, int source_core)
{
	idc_write(IPC_IDCCTL, target_core,
		  IPC_IDCCTL_IDCTBIE(source_core));
	platform_interrupt_unmask(PLATFORM_IDC_INTERRUPT(target_core), 0);
}

/**
 * \brief IDC interrupt handler.
 * \param[in,out] arg Pointer to IDC data.
 */
static void idc_irq_handler(void *arg)
{
	struct idc *idc = arg;
	int core = arch_cpu_get_id();
	uint32_t idctfc;
	uint32_t idctefc;
	uint32_t idcietc;
	uint32_t i;

	tracev_idc("IRQ");

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		idctfc = idc_read(IPC_IDCTFC(i), core);

		if (idctfc & IPC_IDCTFC_BUSY) {
			trace_idc("Nms");

			/* disable BUSY interrupt */
			idc_write(IPC_IDCCTL, core, idc->done_bit_mask);

			idc->received_msg.core = i;
			idc->received_msg.header =
					idctfc & IPC_IDCTFC_MSG_MASK;

			idctefc = idc_read(IPC_IDCTEFC(i), core);
			idc->received_msg.extension =
					idctefc & IPC_IDCTEFC_MSG_MASK;

			idc->msg_pending = 1;

			break;
		}
	}

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		idcietc = idc_read(IPC_IDCIETC(i), core);

		if (idcietc & IPC_IDCIETC_DONE) {
			tracev_idc("Rpy");

			idc_write(IPC_IDCIETC(i), core,
				  idcietc | IPC_IDCIETC_DONE);

			break;
		}
	}
}

/**
 * \brief Sends IDC message.
 * \param[in,out] msg Pointer to IDC message.
 * \param[in] mode Is message blocking or not.
 * \return Error code.
 */
static inline int arch_idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	struct idc *idc = *idc_get();
	int core = arch_cpu_get_id();
	int ret = 0;
	uint32_t timeout = 0;
	uint32_t idcietc;
	uint32_t flags;

	tracev_idc("Msg");

	spin_lock_irq(&idc->lock, flags);

	idc_write(IPC_IDCIETC(msg->core), core, msg->extension);
	idc_write(IPC_IDCITC(msg->core), core, msg->header | IPC_IDCITC_BUSY);

	if (mode == IDC_BLOCKING) {
		do {
			idelay(PLATFORM_DEFAULT_DELAY);
			timeout += PLATFORM_DEFAULT_DELAY;
			idcietc = idc_read(IPC_IDCIETC(msg->core), core);
		} while (!(idcietc & IPC_IDCIETC_DONE) &&
			 timeout < IDC_TIMEOUT);

		if (timeout >= IDC_TIMEOUT) {
			trace_idc_error("eS0");
			ret = -ETIME;
		}
	}

	spin_unlock_irq(&idc->lock, flags);

	return ret;
}

static inline int idc_pipeline_trigger(uint32_t comp_id, uint32_t cmd)
{
	struct ipc_comp_dev *pcm_dev;
	int ret;

	/* check whether component exists */
	pcm_dev = ipc_get_comp(_ipc, comp_id);
	if (!pcm_dev)
		return -ENODEV;

	/* check whether we are executing from the right core */
	if (arch_cpu_get_id() != pcm_dev->cd->pipeline->ipc_pipe.core)
		return -EINVAL;

	/* invalidate pipeline on start */
	if (cmd == COMP_TRIGGER_START)
		pipeline_cache(pcm_dev->cd->pipeline,
			       pcm_dev->cd, COMP_CACHE_INVALIDATE);

	/* trigger pipeline */
	ret = pipeline_trigger(pcm_dev->cd->pipeline, pcm_dev->cd, cmd);

	/* writeback pipeline on stop */
	if (cmd == COMP_TRIGGER_STOP)
		pipeline_cache(pcm_dev->cd->pipeline,
			       pcm_dev->cd, COMP_CACHE_WRITEBACK_INV);

	return ret;
}

/**
 * \brief Executes IDC message based on type.
 * \param[in,out] msg Pointer to IDC message.
 */
static inline void idc_cmd(struct idc_msg *msg)
{
	uint32_t type = iTS(msg->header);

	switch (type) {
	case iTS(IDC_MSG_POWER_DOWN):
		cpu_power_down_core();
		break;
	case iTS(IDC_MSG_PPL_TRIGGER):
		idc_pipeline_trigger(iPTComp(msg->extension),
				     iPTCommand(msg->extension));
		break;
	default:
		trace_idc_error("eTc");
		trace_error_value(msg->header);
	}
}

/**
 * \brief Handles received IDC message.
 * \param[in,out] idc Pointer to IDC data.
 */
static inline void idc_do_cmd(struct idc *idc)
{
	int core = arch_cpu_get_id();
	int initiator = idc->received_msg.core;

	trace_idc("Cmd");

	idc_cmd(&idc->received_msg);

	idc->msg_pending = 0;

	/* clear BUSY bit */
	idc_write(IPC_IDCTFC(initiator), core,
		  idc_read(IPC_IDCTFC(initiator), core) | IPC_IDCTFC_BUSY);

	/* enable BUSY interrupt */
	idc_write(IPC_IDCCTL, core, idc->busy_bit_mask | idc->done_bit_mask);
}

/**
 * \brief Checks for pending IDC messages.
 */
static inline void arch_idc_process_msg_queue(void)
{
	struct idc *idc = *idc_get();

	if (idc->msg_pending)
		idc_do_cmd(idc);
}

/**
 * \brief Returns BUSY interrupt mask based on core id.
 * \param[in] core Core id.
 * \return BUSY interrupt mask.
 */
static inline uint32_t idc_get_busy_bit_mask(int core)
{
	uint32_t busy_mask = 0;
	int i;

	if (core == PLATFORM_MASTER_CORE_ID) {
		for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
			if (i != PLATFORM_MASTER_CORE_ID)
				busy_mask |= IPC_IDCCTL_IDCTBIE(i);
		}
	} else {
		busy_mask = IPC_IDCCTL_IDCTBIE(PLATFORM_MASTER_CORE_ID);
	}

	return busy_mask;
}

/**
 * \brief Returns DONE interrupt mask based on core id.
 * \param[in] core Core id.
 * \return DONE interrupt mask.
 */
static inline uint32_t idc_get_done_bit_mask(int core)
{
	uint32_t done_mask = 0;
	int i;

	if (core == PLATFORM_MASTER_CORE_ID) {
		for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
			if (i != PLATFORM_MASTER_CORE_ID)
				done_mask |= IPC_IDCCTL_IDCIDIE(i);
		}
	} else {
		done_mask = 0;
	}

	return done_mask;
}

/**
 * \brief Initializes IDC data and registers for interrupt.
 */
static inline void arch_idc_init(void)
{
	int core = arch_cpu_get_id();

	trace_idc("IDI");

	/* initialize idc data */
	struct idc **idc = idc_get();
	*idc = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(**idc));
	spinlock_init(&((*idc)->lock));
	(*idc)->busy_bit_mask = idc_get_busy_bit_mask(core);
	(*idc)->done_bit_mask = idc_get_done_bit_mask(core);

	/* configure interrupt */
	interrupt_register(PLATFORM_IDC_INTERRUPT(core),
			   idc_irq_handler, *idc);
	interrupt_enable(PLATFORM_IDC_INTERRUPT(core));

	/* enable BUSY and DONE (only for master core) interrupts */
	idc_write(IPC_IDCCTL, core,
		  (*idc)->busy_bit_mask | (*idc)->done_bit_mask);
}

/**
 * \brief Frees IDC data and unregisters interrupt.
 */
static inline void idc_free(void)
{
	int core = arch_cpu_get_id();
	int i = 0;
	uint32_t idctfc;

	trace_idc("IDF");

	/* disable and unregister interrupt */
	interrupt_disable(PLATFORM_IDC_INTERRUPT(core));
	interrupt_unregister(PLATFORM_IDC_INTERRUPT(core));

	/* clear BUSY bits */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		idctfc = idc_read(IPC_IDCTFC(i), core);
		if (idctfc & IPC_IDCTFC_BUSY)
			idc_write(IPC_IDCTFC(i), core, idctfc);
	}

	rfree(*idc_get());
}

#endif
