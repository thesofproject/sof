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

LOG_MODULE_REGISTER(debug_strem_slot);

struct cpu_mutex {
	struct k_mutex m;
} __aligned(CONFIG_DCACHE_LINE_SIZE);

/* CPU specific mutexes for each circular buffer */
static struct cpu_mutex cpu_mutex[CONFIG_MP_MAX_NUM_CPUS];

static const int debug_stream_slot = CONFIG_SOF_DEBUG_STREAM_SLOT_NUMBER;

static struct debug_stream_slot_hdr *debug_stream_get_slot(void)
{
	return (struct debug_stream_slot_hdr *)ADSP_DW->slots[debug_stream_slot];
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

	*desc = hdr->section_desc[core];
	LOG_DBG("Section %u (desc %u %u %u)", core, desc->core_id, desc->buf_words, desc->offset);

	return (struct debug_stream_circular_buf *) (((uint8_t *)hdr) + desc->offset);
}

int debug_stream_slot_send_record(struct debug_stream_record *rec)
{
	struct debug_stream_section_descriptor desc = { 0 };
	struct debug_stream_circular_buf *buf =
		debug_stream_get_circular_buffer(&desc, arch_proc_id());
	uint32_t record_size = rec->size_words;
	uint32_t record_start, buf_remain;

	LOG_DBG("Sending record %u id %u len %u\n", rec->seqno, rec->id, rec->size_words);

	if (!buf)
		return -ENODEV;

	if (rec->size_words >= desc.buf_words) {
		LOG_ERR("Record too big %u >= %u (desc %u %u %u)", rec->size_words,
			desc.buf_words, desc.core_id, desc.buf_words, desc.offset);
		return -ENOMEM;
	}
	k_mutex_lock(&cpu_mutex[arch_proc_id()].m, K_FOREVER);

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

	k_mutex_unlock(&cpu_mutex[arch_proc_id()].m);

	LOG_DBG("Record %u id %u len %u sent\n", rec->seqno, rec->id, record_size);
	return 0;
}

static int debug_stream_slot_init(void)
{
	struct debug_stream_slot_hdr *hdr = debug_stream_get_slot();
	size_t hdr_size = ALIGN_UP(offsetof(struct debug_stream_slot_hdr,
					    section_desc[CONFIG_MP_MAX_NUM_CPUS]),
				   CONFIG_DCACHE_LINE_SIZE);
	size_t section_area_size = ADSP_DW_SLOT_SIZE - hdr_size;
	size_t section_size = ALIGN_DOWN(section_area_size /
					 CONFIG_MP_MAX_NUM_CPUS,
					 CONFIG_DCACHE_LINE_SIZE);
	size_t offset = hdr_size;
	int i;

	LOG_INF("%u sections of %u bytes, hdr %u, secton area %u\n",
		CONFIG_MP_MAX_NUM_CPUS, section_size, hdr_size,
		section_area_size);

	if (ADSP_DW->descs[debug_stream_slot].type != 0)
		LOG_WRN("Slot %d was not free: %u", debug_stream_slot,
			ADSP_DW->descs[debug_stream_slot].type);
	ADSP_DW->descs[debug_stream_slot].type = ADSP_DW_SLOT_DEBUG_STREAM;

	hdr->hdr.magic = DEBUG_STREAM_IDENTIFIER;
	hdr->hdr.hdr_size = hdr_size;
	hdr->total_size = hdr_size + CONFIG_MP_MAX_NUM_CPUS * section_size;
	hdr->num_sections = CONFIG_MP_MAX_NUM_CPUS;
	for (i = 0; i < CONFIG_MP_MAX_NUM_CPUS; i++) {
		hdr->section_desc[i].core_id = i;
		hdr->section_desc[i].buf_words =
			(section_size - offsetof(struct debug_stream_circular_buf, data[0]))/
			sizeof(uint32_t);
		hdr->section_desc[i].offset = offset;
		LOG_INF("sections %u, size %u, offset %u\n",
			i, section_size, offset);
		offset += section_size;
	}
	for (i = 0; i < CONFIG_MP_MAX_NUM_CPUS; i++) {
		struct debug_stream_section_descriptor desc = { 0 };
		struct debug_stream_circular_buf *buf =
			debug_stream_get_circular_buffer(&desc, i);

		buf->next_seqno = 0;
		buf->w_ptr = 0;
		k_mutex_init(&cpu_mutex[i].m);
		/* The core specific mutexes are now .bss which is uncached so the
		 * following line is commented out. However, since the mutexes are
		 * core specific there should be nothing preventing from having them
		 * in cached memory.
		 *
		 * sys_cache_data_flush_range(&cpu_mutex[i], sizeof(cpu_mutex[i]));
		 */
	}
	LOG_INF("Debug stream slot initialized\n");

	return 0;
}

SYS_INIT(debug_stream_slot_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
