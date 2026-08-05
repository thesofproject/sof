// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2022 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>
#include <platform/fuzz.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/msg.h>
#include <sof/lib/mailbox.h>
#include <sof/ipc/common.h>
#include <sof/ipc/schedule.h>
#include <sof/ipc/topology.h>
#include <sof/schedule/edf_schedule.h>
#include <sof/audio/component_ext.h>
#include <stdbool.h>
#include <stddef.h>
#include <errno.h>

// 6c8f0d53-ff77-4ca1-b825-c0c4e1b0d322
SOF_DEFINE_REG_UUID(ipc_task_posix);

static struct ipc *global_ipc;

#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
// Not an ISR, called from the native_posix fuzz interrupt.  Left
// alone for general hygiene.  This is how a IPC interrupt would look
// if we had one.
static void posix_ipc_isr(void *arg)
{
	ipc_schedule_process(global_ipc);
}

// External symbols set up by the fuzzing layer in fuzz.c.
extern const uint8_t *posix_fuzz_buf;
extern size_t posix_fuzz_sz;

// Lots of space.  Should really synchronize with the -max_len
// parameter to libFuzzer (defaults to 4096), but that requires
// thinking/experimentation about how much fuzzing we want to do at a
// time...
static uint8_t fuzz_in[65536];
static size_t fuzz_in_sz;

/*
 * posix_ipc_teardown - drop all IPC-tracked objects left over from the
 * previous fuzz testcase so the next one starts from a clean topology.
 *
 * Walking and freeing comp_list in the same pass is unsafe because the
 * SOF free helpers unlink the entry from the list, so we snapshot the
 * IDs of one type into a local array first and then call the typed
 * free function for each one. Passes run in dependency order
 * (COMPONENT -> BUFFER -> PIPELINE) to keep the SOF topology layer
 * from dereferencing already-freed parents.
 *
 * Each typed pass drains in batches: it snapshots up to
 * POSIX_TEARDOWN_BATCH ids, frees them, and repeats until no entry of
 * that type is left, so the number of objects a testcase may create is
 * not bounded by the snapshot size.
 *
 * A final force-drain pass then walks anything still on the list with
 * list_for_item_safe() and removes it with list_item_del() + rfree()
 * directly. That keeps the harness future-proof against new COMP_TYPE_*
 * values and against entries a typed free could not release: whatever
 * the reason, comp_list is guaranteed empty on return. The inner union
 * object (cd/cb/pipeline) is leaked in that edge case, which is
 * acceptable for a fuzzing harness.
 */
#define POSIX_TEARDOWN_BATCH 256

static void posix_ipc_teardown(void)
{
	static const uint16_t free_order[] = {
		COMP_TYPE_COMPONENT,
#if CONFIG_IPC_MAJOR_3
		/*
		 * IPC4 never stores COMP_TYPE_BUFFER in comp_list
		 * (see ipc4/helper.c) and ipc_buffer_free() is not
		 * compiled for IPC4, so skip the buffer pass there.
		 */
		COMP_TYPE_BUFFER,
#endif
		COMP_TYPE_PIPELINE,
	};
	uint32_t ids[POSIX_TEARDOWN_BATCH];
	struct ipc_comp_dev *icd;
	struct list_item *pos, *tmp;
	int n;

	if (!global_ipc)
		return;

	/*
	 * Pre-pass: prepare components and pipelines for the ordered free
	 * passes below.
	 *
	 * ipc_comp_free() refuses to free a component unless its state is
	 * COMP_STATE_READY.  A fuzz testcase that ran INIT_INSTANCE followed
	 * by SET_PIPELINE_STATE:RUNNING will leave components in PREPARE,
	 * PAUSED, or ACTIVE state.  Force every component back to READY so
	 * the free pass can proceed without silently skipping entries.
	 *
	 * For pipelines with an active scheduler task, cancel the task before
	 * ipc_pipeline_free() calls schedule_task_free() on it.  Without this,
	 * schedule_task_free() blocks waiting for the task to complete, which
	 * on native_sim can stall for up to 100 LL periods.
	 */
	list_for_item(pos, &global_ipc->comp_list) {
		icd = container_of(pos, struct ipc_comp_dev, list);
		if (icd->type == COMP_TYPE_COMPONENT && icd->cd) {
			icd->cd->state = COMP_STATE_READY;
			/* Ensure buffer lists are valid so ipc_comp_free()
			 * does not bail at the uninitialized-list check.
			 * A component whose init failed partway may have
			 * NULL list pointers.
			 */
			if (!icd->cd->bsource_list.next)
				list_init(&icd->cd->bsource_list);
			if (!icd->cd->bsink_list.next)
				list_init(&icd->cd->bsink_list);
		}
		if (icd->type == COMP_TYPE_PIPELINE &&
		    icd->pipeline && icd->pipeline->pipe_task)
			schedule_task_cancel(icd->pipeline->pipe_task);
	}

	for (int pass = 0; pass < (int)ARRAY_SIZE(free_order); pass++) {
		uint16_t type = free_order[pass];

		/*
		 * Drain this type in batches.  The ipc_*_free() helpers
		 * unlink each entry internally, so snapshot up to
		 * POSIX_TEARDOWN_BATCH ids, free them, and repeat until no
		 * entry of this type is left.  A batch that frees nothing
		 * (e.g. an entry ipc_comp_free() refuses to release) ends the
		 * loop so it cannot spin forever; the force-drain pass below
		 * removes any such straggler.
		 */
		do {
			int freed = 0;

			n = 0;
			list_for_item(pos, &global_ipc->comp_list) {
				icd = container_of(pos, struct ipc_comp_dev,
						   list);
				if (icd->type == type &&
				    n < POSIX_TEARDOWN_BATCH)
					ids[n++] = icd->id;
			}

			for (int i = 0; i < n; i++) {
				int ret = -EINVAL;

				switch (type) {
				case COMP_TYPE_COMPONENT:
					ret = ipc_comp_free(global_ipc, ids[i]);
					break;
#if CONFIG_IPC_MAJOR_3
				case COMP_TYPE_BUFFER:
					ret = ipc_buffer_free(global_ipc, ids[i]);
					break;
#endif
				case COMP_TYPE_PIPELINE:
					ret = ipc_pipeline_free(global_ipc,
								ids[i]);
					break;
				}
				if (!ret)
					freed++;
			}

			if (!freed)
				break;
		} while (n == POSIX_TEARDOWN_BATCH);
	}

	/*
	 * Force-drain anything still on the list: unknown/future
	 * COMP_TYPE_* values or entries a typed free could not release.
	 * Walk with the _safe variant because each entry is unlinked as it
	 * is visited.  The inner union pointer leaks, but comp_list is empty
	 * on return regardless of how many objects remained, so the next
	 * testcase never observes a stale entry.
	 */
	list_for_item_safe(pos, tmp, &global_ipc->comp_list) {
		icd = container_of(pos, struct ipc_comp_dev, list);
		list_item_del(&icd->list);
		rfree(icd);
	}
}

/*
 * Testcase-isolation helpers used by the libFuzzer entry point in
 * fuzz.c. They keep ownership of the cross-call state in one module
 * so a new testcase never observes leftovers from a previous one that
 * failed to drain inside the simulator tick budget.
 */
void posix_fuzz_case_begin(void)
{
	posix_ipc_teardown();
	fuzz_in_sz = 0;
}

bool posix_fuzz_case_pending(void)
{
	return posix_fuzz_sz != 0 || fuzz_in_sz != 0;
}

void posix_fuzz_case_abort(void)
{
	posix_fuzz_sz = 0;
	fuzz_in_sz = 0;
}

// The framing protocol is super simple: the first two bytes are a
// little-endian message size in bytes (capped below at
// SOF_IPC_MSG_MAX_SIZE - 4 so it always fits in the IPC message
// buffer).  We then copy that much of the input buffer (subject to
// clamping obviously) into the incoming IPC message buffer and invoke
// the ISR.  Any remainder will be delivered synchronously as another
// message after receipt of "complete_cmd()" from the SOF engine,
// etc...  Eventually we'll receive another fuzz input after some
// amount of simulated time has passed (c.f.
// CONFIG_ZEPHYR_POSIX_FUZZ_TICKS).
//
// The historical encoding used a single byte multiplied by 2, which
// capped each message at 510 bytes.  That was fine for IPC3
// (SOF_IPC_MSG_MAX_SIZE = 384) but limited IPC4 (max 4096) to ~12%
// of its envelope, hiding all large_config / vendor_config /
// pipeline-state-data paths from coverage feedback.  Widening to two
// bytes lets the fuzzer reach the full IPC4 message range; existing
// corpora are reinterpreted but libFuzzer recovers within minutes.
#define POSIX_FUZZ_HDR_LEN 2

static void fuzz_isr(const void *arg)
{
	size_t rem, i, n = MIN(posix_fuzz_sz, sizeof(fuzz_in) - fuzz_in_sz);

	for (i = 0; i < n; i++)
		fuzz_in[fuzz_in_sz++] = posix_fuzz_buf[i];

	if (fuzz_in_sz < POSIX_FUZZ_HDR_LEN)
		return;

	if (!global_ipc->comp_data)
		return;

	size_t maxsz = SOF_IPC_MSG_MAX_SIZE - 4;
	size_t msgsz = (size_t)fuzz_in[0] | ((size_t)fuzz_in[1] << 8);

	msgsz = MIN(msgsz, maxsz);
	n = MIN(msgsz, fuzz_in_sz - POSIX_FUZZ_HDR_LEN);
	rem = fuzz_in_sz - (n + POSIX_FUZZ_HDR_LEN);

	memset(global_ipc->comp_data, 0, maxsz);
	memcpy(global_ipc->comp_data, &fuzz_in[POSIX_FUZZ_HDR_LEN], n);
	memmove(&fuzz_in[0], &fuzz_in[n + POSIX_FUZZ_HDR_LEN], rem);
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
#endif

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
//
// The harness also mirrors the framed message into MAILBOX_HOSTBOX so
// that handlers reading payload directly from the hostbox region
// (large_config_set/get, set_dx, set_pipeline_state, vendor_config and
// friends in ipc4/handler-user.c and ipc4/handler-kernel.c) observe
// the fuzz bytes rather than stale or zero-filled memory.
//
// The two IPC majors split header and payload differently:
//
//  * IPC3 carries the header in-band at the start of the message, and
//    mailbox_validate() walks the full message starting from offset 0
//    of the hostbox. The full message is mirrored as-is.
//
//  * IPC4 splits the 8-byte compact header (consumed via
//    ipc_compact_read_msg()) from the payload, which on real hardware
//    lives in HOSTBOX. The harness therefore mirrors only the
//    post-header bytes, so the first dword of MAILBOX_HOSTBOX matches
//    the first dword of the IPC4 payload (e.g. pipelines_count for
//    SET_PIPELINE_STATE) instead of header bits.
//
// posix_hostbox is sized to SOF_IPC_MSG_MAX_SIZE (see
// platform/lib/memory.h), so the copy is always in bounds for both
// IPC3 and IPC4 message envelopes.
enum task_state ipc_platform_do_cmd(struct ipc *ipc)
{
	struct ipc_cmd_hdr *hdr;

#ifdef CONFIG_IPC_MAJOR_4
	memset(posix_hostbox, 0, SOF_IPC_MSG_MAX_SIZE);
	memcpy(posix_hostbox,
	       (const uint8_t *)global_ipc->comp_data + sizeof(struct ipc_cmd_hdr),
	       SOF_IPC_MSG_MAX_SIZE - sizeof(struct ipc_cmd_hdr));
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
#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
	extern void posix_sw_set_pending_IRQ(unsigned int IRQn);

	if (fuzz_in_sz > 0) {
		posix_fuzz_sz = 0;
		posix_sw_set_pending_IRQ(CONFIG_ZEPHYR_POSIX_FUZZ_IRQ);
	}
#endif
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
#ifdef CONFIG_ARCH_POSIX_LIBFUZZER
	IRQ_CONNECT(CONFIG_ZEPHYR_POSIX_FUZZ_IRQ, 0, fuzz_isr, NULL, 0);
	irq_enable(CONFIG_ZEPHYR_POSIX_FUZZ_IRQ);
#endif

	global_ipc = ipc;
	schedule_task_init_edf(&ipc->ipc_task, SOF_UUID(ipc_task_posix_uuid),
			       &ipc_task_ops, ipc, 0, 0);

	return 0;
}
