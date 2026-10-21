// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <adsp_debug_window.h>
#include <adsp_memory.h>
#include <sof/common.h>
#include <rtos/string.h>
#include <user/debug_stream.h>
#include <user/debug_stream_slot.h>
#include <zephyr/kernel.h>

#ifdef CONFIG_USERSPACE
#include <zephyr/internal/syscall_handler.h>
#include <zephyr/sys/math_extras.h>
#endif

LOG_MODULE_REGISTER(debug_stream_slot);

struct cpu_mutex {
	struct k_spinlock l;
} __aligned(CONFIG_DCACHE_LINE_SIZE);

/* CPU specific mutexes for each circular buffer */
static struct cpu_mutex cpu_mutex[CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS];

#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
static struct debug_stream_slot_hdr *dbg_stream_slot;
#else
static const int debug_stream_slot = CONFIG_SOF_DEBUG_STREAM_SLOT_NUMBER;
#endif

static struct debug_stream_slot_hdr *debug_stream_get_slot(void)
{
#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
	if (!dbg_stream_slot) {
		struct adsp_dw_desc slot_desc = { .type = ADSP_DW_SLOT_DEBUG_STREAM, };

		dbg_stream_slot = (struct debug_stream_slot_hdr *)
					adsp_dw_request_slot(&slot_desc, NULL);
	}

	return dbg_stream_slot;
#else
	return (struct debug_stream_slot_hdr *)ADSP_DW->slots[debug_stream_slot];
#endif
}

static
struct debug_stream_circular_buf *
debug_stream_get_circular_buffer(struct debug_stream_section_descriptor *desc, unsigned int core)
{
	struct debug_stream_slot_hdr *hdr = debug_stream_get_slot();

	if (hdr->hdr.magic != DEBUG_STREAM_IDENTIFIER) {
		LOG_ERR("Debug stream slot not initialized.");
		return NULL;
	}

	if (core >= CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS) {
		LOG_DBG("No section for cpu %u >= %u ", core,
			CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS);
		return NULL;
	}

	*desc = hdr->section_desc[core];
	LOG_DBG("Section %u (desc %u %u %u)", core, desc->core_id, desc->buf_words, desc->offset);

	return (struct debug_stream_circular_buf *) (((uint8_t *)hdr) + desc->offset);
}

/*
 * @record_size is the record length in words. It is passed separately and
 * rec->size_words must not be re-read here: the system call validator
 * checked access to exactly @record_size words of user memory.
 */
static int debug_stream_slot_send_sized(struct debug_stream_record *rec, uint32_t record_size)
{
	struct debug_stream_section_descriptor desc = { 0 };
	struct debug_stream_circular_buf *buf =
		debug_stream_get_circular_buffer(&desc, arch_proc_id());
	uint32_t record_start, buf_remain;
	k_spinlock_key_t key;

	LOG_DBG("Sending record len %u", record_size);

	if (!buf)
		return -ENODEV;

	if (record_size >= desc.buf_words) {
		LOG_ERR("Record too big %u >= %u (desc %u %u %u)", record_size,
			desc.buf_words, desc.core_id, desc.buf_words, desc.offset);
		return -ENOMEM;
	}
	key = k_spin_lock(&cpu_mutex[arch_proc_id()].l);

	rec->seqno = buf->next_seqno++;
	rec->size_words = record_size + 1; /* +1 for size at the end of record */
	record_start = buf->w_ptr;
	buf->w_ptr = (record_start + record_size) % desc.buf_words;
	buf_remain = desc.buf_words - record_start;
	if (buf_remain < record_size) {
		uint32_t rec_remain = record_size - buf_remain;
		int ret;

		ret = memcpy_s(&buf->data[record_start], buf_remain * sizeof(uint32_t),
			       rec, buf_remain * sizeof(uint32_t));
		assert(!ret);
		ret = memcpy_s(&buf->data[0], desc.buf_words * sizeof(uint32_t),
			       ((uint32_t *) rec) + buf_remain, rec_remain * sizeof(uint32_t));
		assert(!ret);
	} else {
		int ret;

		ret = memcpy_s(&buf->data[record_start], buf_remain * sizeof(uint32_t),
			       rec, record_size * sizeof(uint32_t));
		assert(!ret);
	}
	/* Write record size again after the record */
	buf->data[buf->w_ptr] = record_size + 1;
	buf->w_ptr = (buf->w_ptr + 1) % desc.buf_words;

	k_spin_unlock(&cpu_mutex[arch_proc_id()].l, key);

	LOG_DBG("Record len %u sent", record_size);
	return 0;
}

int z_impl_debug_stream_slot_send_record(struct debug_stream_record *rec)
{
	return debug_stream_slot_send_sized(rec, rec->size_words);
}

#ifdef CONFIG_USERSPACE
static inline int z_vrfy_debug_stream_slot_send_record(struct debug_stream_record *rec)
{
	uint32_t size_words;
	size_t bytes;

	/* the impl writes seqno and size_words back to the record */
	K_OOPS(K_SYSCALL_MEMORY_WRITE(rec, sizeof(*rec)));
	K_OOPS(k_usermode_from_copy(&size_words, &rec->size_words, sizeof(size_words)));
	K_OOPS(size_mul_overflow(size_words, sizeof(uint32_t), &bytes));
	K_OOPS(K_SYSCALL_MEMORY_READ(rec, MAX(bytes, sizeof(*rec))));

	return debug_stream_slot_send_sized(rec, size_words);
}
#include <zephyr/syscalls/debug_stream_slot_send_record_mrsh.c>
#endif

static int debug_stream_slot_init(void)
{
	struct debug_stream_slot_hdr *hdr = debug_stream_get_slot();
	size_t hdr_size = ALIGN_UP(
		offsetof(struct debug_stream_slot_hdr,
			 section_desc[CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS]),
		CONFIG_DCACHE_LINE_SIZE);
	size_t section_area_size = ADSP_DW_SLOT_SIZE - hdr_size;
	size_t section_size = ALIGN_DOWN(section_area_size /
					 CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS,
					 CONFIG_DCACHE_LINE_SIZE);
	size_t offset = hdr_size;
	int i;

	LOG_INF("%u sections of %u bytes, hdr %u, section area %u",
		CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS, section_size, hdr_size,
		section_area_size);

#ifdef CONFIG_INTEL_ADSP_DEBUG_SLOT_MANAGER
	if (!hdr)
		return -ENOMEM;
#else
	if (ADSP_DW->descs[debug_stream_slot].type != 0)
		LOG_WRN("Slot %d was not free: %u", debug_stream_slot,
			ADSP_DW->descs[debug_stream_slot].type);
	ADSP_DW->descs[debug_stream_slot].type = ADSP_DW_SLOT_DEBUG_STREAM;
#endif

	hdr->hdr.magic = DEBUG_STREAM_IDENTIFIER;
	hdr->hdr.hdr_size = hdr_size;
	hdr->total_size = hdr_size + CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS * section_size;
	hdr->num_sections = CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS;
	for (i = 0; i < CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS; i++) {
		hdr->section_desc[i].core_id = i;
		hdr->section_desc[i].buf_words =
			(section_size - offsetof(struct debug_stream_circular_buf, data[0])) /
			sizeof(uint32_t);
		hdr->section_desc[i].offset = offset;
		LOG_DBG("sections %u, size %u, offset %u", i, section_size, offset);
		offset += section_size;
	}
	for (i = 0; i < CONFIG_SOF_DEBUG_STREAM_SLOT_FORCE_MAX_CPUS; i++) {
		struct debug_stream_section_descriptor desc = { 0 };
		struct debug_stream_circular_buf *buf = debug_stream_get_circular_buffer(&desc, i);

		buf->next_seqno = 0;
		buf->w_ptr = 0;
	}
	LOG_DBG("Debug stream slot initialized");

	return 0;
}

SYS_INIT(debug_stream_slot_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
