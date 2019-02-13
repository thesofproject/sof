
#include <xtos-structs.h>
#include <arch/cpu.h>
#include <platform/interrupt.h>
#include <platform/platform.h>
#include <sof/alloc.h>
#include <sof/cache.h>
#include <sof/idc.h>
#include <sof/ipc.h>
#include <sof/lock.h>
#include <sof/notifier.h>

#include <arch/idc.h>

extern struct ipc *_ipc;

/**
 * \brief Returns IDC data.
 * \return Pointer to pointer of IDC data.
 */
static struct idc **idc_get(void)
{
	struct core_context *ctx = (struct core_context *)cpu_read_threadptr();

	return &ctx->idc;
}

/**
 * \brief Enables IDC interrupts.
 * \param[in] target_core Target core id.
 * \param[in] source_core Source core id.
 */
void idc_enable_interrupts(int target_core, int source_core)
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

	tracev_idc("idc_irq_handler()");

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		idctfc = idc_read(IPC_IDCTFC(i), core);

		if (idctfc & IPC_IDCTFC_BUSY) {
			trace_idc("idc_irq_handler(), IPC_IDCTFC_BUSY");

			/* disable BUSY interrupt */
			idc_write(IPC_IDCCTL, core, idc->done_bit_mask);

			idc->received_msg.core = i;
			idc->received_msg.header =
					idctfc & IPC_IDCTFC_MSG_MASK;

			idctefc = idc_read(IPC_IDCTEFC(i), core);
			idc->received_msg.extension =
					idctefc & IPC_IDCTEFC_MSG_MASK;

			schedule_task(&idc->idc_task, 0, IDC_DEADLINE);

			break;
		}
	}

	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		idcietc = idc_read(IPC_IDCIETC(i), core);

		if (idcietc & IPC_IDCIETC_DONE) {
			tracev_idc("idc_irq_handler(), IPC_IDCIETC_DONE");

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
int arch_idc_send_msg(struct idc_msg *msg, uint32_t mode)
{
	struct idc *idc = *idc_get();
	int core = arch_cpu_get_id();
	int ret = 0;
	uint32_t timeout = 0;
	uint32_t idcietc;
	uint32_t flags;

	tracev_idc("arch_idc_send_msg()");

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
			trace_idc_error("arch_idc_send_msg() error: timeout");
			ret = -ETIME;
		}
	}

	spin_unlock_irq(&idc->lock, flags);

	return ret;
}

/**
 * \brief Executes IDC pipeline trigger message.
 * \param[in] cmd Trigger command.
 * \return Error code.
 */
static int idc_pipeline_trigger(uint32_t cmd)
{
	struct sof_ipc_stream *data = _ipc->comp_data;
	struct ipc_comp_dev *pcm_dev;
	int ret;

	/* invalidate stream data */
	dcache_invalidate_region(data, sizeof(*data));

	/* check whether component exists */
	pcm_dev = ipc_get_comp(_ipc, data->comp_id);
	if (!pcm_dev)
		return -ENODEV;

	/* check whether we are executing from the right core */
	if (arch_cpu_get_id() != pcm_dev->cd->pipeline->ipc_pipe.core)
		return -EINVAL;

	/* invalidate pipeline on start */
	if (cmd == COMP_TRIGGER_START)
		pipeline_cache(pcm_dev->cd->pipeline,
			       pcm_dev->cd, CACHE_INVALIDATE);

	/* trigger pipeline */
	ret = pipeline_trigger(pcm_dev->cd->pipeline, pcm_dev->cd, cmd);

	/* writeback pipeline on stop */
	if (cmd == COMP_TRIGGER_STOP)
		pipeline_cache(pcm_dev->cd->pipeline,
			       pcm_dev->cd, CACHE_WRITEBACK_INV);

	return ret;
}

/**
 * \brief Executes IDC component command message.
 * \param[in] cmd Component command.
 * \return Error code.
 */
static int idc_component_command(uint32_t cmd)
{
	struct sof_ipc_ctrl_data *data = _ipc->comp_data;
	struct ipc_comp_dev *comp_dev;
	int ret;

	/* invalidate control data */
	dcache_invalidate_region(data, sizeof(*data));
	dcache_invalidate_region(data + 1,
				 data->rhdr.hdr.size - sizeof(*data));

	/* check whether component exists */
	comp_dev = ipc_get_comp(_ipc, data->comp_id);
	if (!comp_dev)
		return -ENODEV;

	/* check whether we are executing from the right core */
	if (arch_cpu_get_id() != comp_dev->cd->pipeline->ipc_pipe.core)
		return -EINVAL;

	/* execute component command */
	ret = comp_cmd(comp_dev->cd, cmd, data, data->rhdr.hdr.size);

	/* writeback control data */
	dcache_writeback_region(data, data->rhdr.hdr.size);

	return ret;
}

/**
 * \brief Executes IDC message based on type.
 * \param[in,out] msg Pointer to IDC message.
 */
static void idc_cmd(struct idc_msg *msg)
{
	uint32_t type = iTS(msg->header);

	switch (type) {
	case iTS(IDC_MSG_POWER_DOWN):
		cpu_power_down_core();
		break;
	case iTS(IDC_MSG_PPL_TRIGGER):
		idc_pipeline_trigger(msg->extension);
		break;
	case iTS(IDC_MSG_COMP_CMD):
		idc_component_command(msg->extension);
		break;
	case iTS(IDC_MSG_NOTIFY):
		notifier_notify();
		break;
	default:
		trace_idc_error("idc_cmd() error: invalid msg->header = %u",
				msg->header);
	}
}

/**
 * \brief Handles received IDC message.
 * \param[in,out] data Pointer to IDC data.
 */
static void idc_do_cmd(void *data)
{
	struct idc *idc = data;
	int core = arch_cpu_get_id();
	int initiator = idc->received_msg.core;

	trace_idc("idc_do_cmd()");

	idc_cmd(&idc->received_msg);

	/* clear BUSY bit */
	idc_write(IPC_IDCTFC(initiator), core,
		  idc_read(IPC_IDCTFC(initiator), core) | IPC_IDCTFC_BUSY);

	/* enable BUSY interrupt */
	idc_write(IPC_IDCCTL, core, idc->busy_bit_mask | idc->done_bit_mask);
}

/**
 * \brief Returns BUSY interrupt mask based on core id.
 * \param[in] core Core id.
 * \return BUSY interrupt mask.
 */
static uint32_t idc_get_busy_bit_mask(int core)
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
static uint32_t idc_get_done_bit_mask(int core)
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
void arch_idc_init(void)
{
	int core = arch_cpu_get_id();

	trace_idc("arch_idc_init()");

	/* initialize idc data */
	struct idc **idc = idc_get();
	*idc = rzalloc(RZONE_SYS, SOF_MEM_CAPS_RAM, sizeof(**idc));
	spinlock_init(&((*idc)->lock));
	(*idc)->busy_bit_mask = idc_get_busy_bit_mask(core);
	(*idc)->done_bit_mask = idc_get_done_bit_mask(core);

	/* process task */
	schedule_task_init(&(*idc)->idc_task, idc_do_cmd, *idc);
	schedule_task_config(&(*idc)->idc_task, TASK_PRI_IDC, core);

	/* configure interrupt */
	interrupt_register(PLATFORM_IDC_INTERRUPT(core), IRQ_AUTO_UNMASK,
			   idc_irq_handler, *idc);
	interrupt_enable(PLATFORM_IDC_INTERRUPT(core));

	/* enable BUSY and DONE (only for master core) interrupts */
	idc_write(IPC_IDCCTL, core,
		  (*idc)->busy_bit_mask | (*idc)->done_bit_mask);
}

/**
 * \brief Frees IDC data and unregisters interrupt.
 */
void idc_free(void)
{
	struct idc *idc = *idc_get();
	int core = arch_cpu_get_id();
	int i = 0;
	uint32_t idctfc;

	trace_idc("idc_free()");

	/* disable and unregister interrupt */
	interrupt_disable(PLATFORM_IDC_INTERRUPT(core));
	interrupt_unregister(PLATFORM_IDC_INTERRUPT(core));

	/* clear BUSY bits */
	for (i = 0; i < PLATFORM_CORE_COUNT; i++) {
		idctfc = idc_read(IPC_IDCTFC(i), core);
		if (idctfc & IPC_IDCTFC_BUSY)
			idc_write(IPC_IDCTFC(i), core, idctfc);
	}

	schedule_task_free(&idc->idc_task);
}
