// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2022 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>
#include <sof/lib/uuid.h>
#include <sof/ipc/msg.h>
#include <sof/lib/mailbox.h>
#include <sof/ipc/common.h>
#include <sof/ipc/schedule.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/audio/component_ext.h>

// 6c8f0d53-ff77-4ca1-b825-c0c4e1b0d322
SOF_DEFINE_REG_UUID(ipc_task_posix);

static struct ipc *global_ipc;

// Not an ISR, called from the native_posix fuzz interrupt.  Left
// alone for general hygiene.  This is how a IPC interrupt would look
// if we had one.
static void posix_ipc_isr(void *arg)
{
	ipc_schedule_process(global_ipc);
}

// External symbols set up by the fuzzing layer
extern uint8_t *posix_fuzz_buf, posix_fuzz_sz;

// Lots of space.  Should really synchronize with the -max_len
// parameter to libFuzzer (defaults to 4096), but that requires
// thinking/experimentation about how much fuzzing we want to do at a
// time...
static uint8_t fuzz_in[65536];
static size_t fuzz_in_sz;

// The protocol here is super simple: the first byte is a message size
// in units of 16 bits (the buffer maximum defaults to 384 bytes, and
// I didn't want to waste space early in the buffer lest I confuse the
// fuzzing heuristics).  We then copy that much of the input buffer
// (subject to clamping obviously) into the incoming IPC message
// buffer and invoke the ISR.  Any remainder will be delivered
// synchronously as another message after receipt of "complete_cmd()"
// from the SOF engine, etc...  Eventually we'll receive another fuzz
// input after some amount of simulated time has passed (c.f.
// CONFIG_ZEPHYR_POSIX_FUZZ_TICKS)
static void fuzz_isr(const void *arg)
{
	size_t rem, i, n = MIN(posix_fuzz_sz, sizeof(fuzz_in) - fuzz_in_sz);

	for (i = 0; i < n; i++)
		fuzz_in[fuzz_in_sz++] = posix_fuzz_buf[i];

	if (fuzz_in_sz == 0)
		return;

	if (!global_ipc->comp_data)
		return;

	size_t maxsz = SOF_IPC_MSG_MAX_SIZE - 4, msgsz = fuzz_in[0] * 2;

	n = MIN(msgsz, MIN(fuzz_in_sz - 1, maxsz));
	rem = fuzz_in_sz - (n + 1);

	memset(global_ipc->comp_data, 0, maxsz);
	memcpy(global_ipc->comp_data, &fuzz_in[1], n);
	memmove(&fuzz_in[0], &fuzz_in[n + 1], rem);
	fuzz_in_sz = rem;

#ifdef CONFIG_IPC_MAJOR_3
	bool comp_new = false;
	int comp_idx = 0;

	// One special case: a first byte of 0xff (which is in the
	// otherwise-ignored size value at the front of the command --
	// we rewrite those) is interpreted as a "component new"
	// command, which we format specially, with a driver index
	// specified by the second byte (modulo the number of
	// registered drivers).  This command involves matching
	// against a UUID value, which even fuzzing isn't able to
	// discover at runtime.  So unless we whitebox this, no
	// components will be created.
	if (n > 2 && ((uint8_t *)global_ipc->comp_data)[0] == 0xff) {
		comp_new = true;
		comp_idx = ((uint8_t *)global_ipc->comp_data)[1];
	}

	// The first dword is a size value which fuzzing will stumble
	// on only rarely, fill it in manually.
	*(uint32_t *)global_ipc->comp_data = msgsz;

	struct sof_ipc_comp *cc = global_ipc->comp_data;

	// "Adjust" the command to represent a "comp new" command per
	// above.  Basically we want to copy in the UUID value for one
	// of the runtime-enumerated drivers based on data already
	// randomized in the fuzz command.
	if (comp_new) {
		struct {
			struct sof_ipc_comp comp;
			struct sof_ipc_comp_ext ext;
		} *cmd = global_ipc->comp_data;

		// Set global/command type fields to TPLG_MSG/TPLG_COMP_NEW
		cmd->comp.hdr.cmd &= 0x0000ffff;
		cmd->comp.hdr.cmd |= SOF_IPC_GLB_TPLG_MSG;
		cmd->comp.hdr.cmd |= SOF_IPC_TPLG_COMP_NEW;

		// We have only one core available in native_posix
		cmd->comp.core = 0;

		// Fix up cmd size and ext_data_length to match
		if (cmd->comp.hdr.size < sizeof(*cmd))
			cmd->comp.hdr.size = sizeof(*cmd);
		cmd->comp.ext_data_length = cmd->comp.hdr.size - sizeof(cmd->comp);

		// Extract the list of available component drivers (do
		// it every time; in practice I don't think this
		// changes at runtime but in principle it might in the
		// future)
		int ndrvs = 0;
		static struct comp_driver_info *drvs[256];
		struct list_item *iter;
		struct comp_driver_list *dlist = comp_drivers_get();
		list_for_item(iter, &dlist->list) {
			struct comp_driver_info *inf =
				container_of(iter, struct comp_driver_info, list);
			drvs[ndrvs++] = inf;
		}

		struct comp_driver_info *di = drvs[comp_idx % ndrvs];
		memcpy(cmd->ext.uuid, di->drv->uid, sizeof(cmd->ext.uuid));
	}
#endif

	posix_ipc_isr(NULL);
}

// This API is... confounded by its history.  With IPC3, the job of
// this function is to get a newly-received IPC message header (!)
// into the comp_data buffer on the IPC object, the rest of the
// message (including the header!) into the mailbox region (obviously
// on Intel that's a shared memory region where data was already
// written by the host kernel) and then call ipc_cmd() with the same
// pointer.  With IPC3, this copy is done inside mailbox_validate().
//
// On IPC4, the header is copied out by calling
// ipc_compact_read_msg(), which then calls back into our code via
// ipc_platform_compact_read_msg(), writing 8 bytes unconditionally on
// the header object it receives, which is then returned here, and
// then passed to ipc_cmd().
enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_cmd_hdr *hdr;

#ifdef CONFIG_IPC_MAJOR_4
	hdr = ipc_compact_read_msg();
#else
	memcpy(posix_hostbox, global_ipc->comp_data, SOF_IPC_MSG_MAX_SIZE);
	hdr = mailbox_validate();
#endif

	ipc_cmd(hdr);
	return SOF_TASK_STATE_COMPLETED;
}

int ipc_platform_compact_read_msg(struct ipc_cmd_hdr *hdr, int words)
{
	if (words != 2)
		return 0;

	memcpy(hdr, global_ipc->comp_data, 8);
	return 2;
}

// Re-raise the interrupt if there's still fuzz data to process
void ipc_platform_complete_cmd(struct ipc *ipc)
{
	extern void posix_sw_set_pending_IRQ(unsigned int IRQn);

	if (fuzz_in_sz > 0) {
		posix_fuzz_sz = 0;
		posix_sw_set_pending_IRQ(CONFIG_ZEPHYR_POSIX_FUZZ_IRQ);
	}
}

int ipc_platform_send_msg(const struct ipc_msg *msg)
{
	// IPC4 will send zero-length messages with a null buffer
	// pointer, which otherwise gets detected as an error by
	// memcpy_s underneath mailbox_dspbox_write()
	if (IS_ENABLED(CONFIG_IPC_MAJOR_4) && msg->tx_size == 0)
		return 0;

	// There is no host, just write to the mailbox to validate the buffer
	mailbox_dspbox_write(0, msg->tx_data, msg->tx_size);
	return 0;
}

void ipc_platform_send_msg_direct(const struct ipc_msg *msg)
{
	/* TODO: add support */
}

int platform_ipc_init(struct ipc *ipc)
{
	IRQ_CONNECT(CONFIG_ZEPHYR_POSIX_FUZZ_IRQ, 0, fuzz_isr, NULL, 0);
	irq_enable(CONFIG_ZEPHYR_POSIX_FUZZ_IRQ);

	global_ipc = ipc;
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_posix_uuid),
			       &ipc_task_ops, ipc, 0, 0);

	return 0;
}
