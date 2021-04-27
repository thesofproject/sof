// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko	<bartoszx.kokoszko@linux.intel.com>
//	   Artur Kloniecki	<arturx.kloniecki@linux.intel.com>

#include <assert.h>
#include <endian.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <sof/lib/uuid.h>
#include <time.h>
#include <user/abi_dbg.h>
#include <user/trace.h>
#include "convert.h"
#include "filter.h"
#include "misc.h"

#define CEIL(a, b) ((a+b-1)/b)

#define TRACE_MAX_PARAMS_COUNT		4
#define TRACE_MAX_TEXT_LEN		1024
#define TRACE_MAX_FILENAME_LEN		128
#define TRACE_MAX_IDS_STR		10
#define TRACE_IDS_MASK			((1 << TRACE_ID_LENGTH) - 1)
#define INVALID_TRACE_ID		(-1 & TRACE_IDS_MASK)

/** Dictionary entry. This MUST match the start of the linker output
 * defined by _DECLARE_LOG_ENTRY().
 */
struct ldc_entry_header {
	uint32_t level;
	uint32_t component_class;
	uint32_t params_num;
	uint32_t line_idx;
	uint32_t file_name_len;
	uint32_t text_len;
};

/** Dictionary entry + unformatted parameters */
struct ldc_entry {
	struct ldc_entry_header header;
	char *file_name;
	char *text;
	uint32_t *params;
};

/** Dictionary entry + formatted parameters */
struct proc_ldc_entry {
	int subst_mask;
	struct ldc_entry_header header;
	char *file_name;
	char *text;
	uintptr_t params[TRACE_MAX_PARAMS_COUNT];
};

static const char *BAD_PTR_STR = "<bad uid ptr %x>";

#define UUID_LOWER "%s%s%s<%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x>%s%s%s"
#define UUID_UPPER "%s%s%s<%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X>%s%s%s"

/* pointer to config for global context */
struct convert_config *global_config;

static int read_entry_from_ldc_file(struct ldc_entry *entry, uint32_t log_entry_address);

char *format_uid_raw(const struct sof_uuid_entry *uid_entry, int use_colors, int name_first,
		     bool be, bool upper)
{
	const struct sof_uuid *uid_val = &uid_entry->id;
	uint32_t a = be ? htobe32(uid_val->a) : uid_val->a;
	uint16_t b = be ? htobe16(uid_val->b) : uid_val->b;
	uint16_t c = be ? htobe16(uid_val->c) : uid_val->c;
	char *str;

	str = asprintf(upper ? UUID_UPPER : UUID_LOWER,
		use_colors ? KBLU : "",
		name_first ? uid_entry->name : "",
		name_first ? " " : "",
		a, b, c,
		uid_val->d[0], uid_val->d[1], uid_val->d[2],
		uid_val->d[3], uid_val->d[4], uid_val->d[5],
		uid_val->d[6], uid_val->d[7],
		name_first ? "" : " ",
		name_first ? "" : uid_entry->name,
		use_colors ? KNRM : "");
	return str;
}

static const struct sof_uuid_entry *get_uuid_entry(uint32_t uid_ptr)
{
	const struct snd_sof_uids_header *uids_dict = global_config->uids_dict;

	return (const struct sof_uuid_entry *)
	       ((uint8_t *)uids_dict + uids_dict->data_offset + uid_ptr -
	       uids_dict->base_address);
}

/*
 * Use uids dictionary content, to convert address of uuid `entry` from logger
 * memory space to corresponding uuid key address used in firmware trace system
 * (with base UUID_ENTRY_ELF_BASE, 0x1FFFA000 as usual). Function get_uuid_entry
 * works in oppopsite direction.
 */
uint32_t get_uuid_key(const struct sof_uuid_entry *entry)
{
	const struct snd_sof_uids_header *uids_dict = global_config->uids_dict;

	/*
	 * uids_dict->data_offset and uids_dict->base_address are both constants,
	 * related with given ldc file.
	 * Uuid address used in firmware, points unusable memory region,
	 * so its treated as key value.
	 */
	return (uintptr_t)entry - (uintptr_t)uids_dict -
		uids_dict->data_offset + uids_dict->base_address;
}

static const char *format_uid(uint32_t uid_ptr, int use_colors, bool be, bool upper)
{
	const struct snd_sof_uids_header *uids_dict = global_config->uids_dict;
	const struct sof_uuid_entry *uid_entry;
	char *str;

	if (uid_ptr < uids_dict->base_address ||
	    uid_ptr >= uids_dict->base_address + uids_dict->data_length) {
		str = calloc(1, strlen(BAD_PTR_STR) + 1 + 6);
		sprintf(str, BAD_PTR_STR, uid_ptr);
	} else {
		uid_entry = get_uuid_entry(uid_ptr);
		str = format_uid_raw(uid_entry, use_colors, 1, be, upper);
	}

	return str;
}

/* fmt should point '%pUx`, return pointer to UUID string or zero */
static const char *asprintf_uuid(const char *fmt, uint32_t uuid_key, bool use_colors, int *fmt_len)
{
	const char *fmt_end = fmt + strlen(fmt);
	int be, upper;
	int len = 4; /* assure full formating, with x */

	if (fmt + 2 >= fmt_end || fmt[1] != 'p' || fmt[2] != 'U') {
		*fmt_len = 0;
		return NULL;
	}
	/* check 'x' value */
	switch (fmt + 3 < fmt_end ? fmt[3] : 0) {
	case 'b':
		be = 1;
		upper = 0;
		break;
	case 'B':
		be = 1;
		upper = 1;
		break;
	case 'l':
		be = 0;
		upper = 0;
		break;
	case 'L':
		be = 0;
		upper = 1;
		break;
	default:
		be = 0;
		upper = 0;
		--len;
		break;
	}
	*fmt_len = len;
	return format_uid(uuid_key, use_colors, be, upper);
}

static const char *asprintf_entry_text(uint32_t entry_address)
{
	struct ldc_entry entry;
	int ret;

	ret = read_entry_from_ldc_file(&entry, entry_address);
	if (ret)
		return NULL;

	free(entry.file_name);

	return entry.text;
}

/** printf-like formatting from the binary ldc_entry input to the
 *  formatted proc_lpc_entry output. Also copies the unmodified
 *  ldc_entry_header from input to output.
 *
 * @param[out] pe copy of the header + formatted output
 * @param[in] e copy of the dictionary entry with unformatted,
    uint32_t params have been inserted.
   @param[in] use_colors whether to use ANSI terminal codes
*/
static void process_params(struct proc_ldc_entry *pe,
			   const struct ldc_entry *e,
			   int use_colors)
{
	char *p = e->text;
	const char *t_end = p + strlen(e->text);
	int uuid_fmt_len;
	int i = 0;

	pe->subst_mask = 0;
	pe->header =  e->header;
	pe->file_name = e->file_name;
	pe->text = e->text;

	/*
	 * Scan the text for possible replacements. We follow the Linux kernel
	 * that uses %pUx formats for UUID / GUID printing, where 'x' is
	 * optional and can be one of 'b', 'B', 'l' (default), and 'L'.
	 * For decoding log entry text from pointer %pQ is used.
	 */
	while ((p = strchr(p, '%'))) {
		/* % can't be the last char */
		if (p + 1 >= t_end) {
			log_err("Invalid format string");
			break;
		}

		/* scan format string */
		if (p[1] == '%') {
			/* Skip "%%" */
			p += 2;
		} else if (p[1] == 's') {
			/* %s format specifier */
			/* check for string printing, because it leads to logger crash */
			log_err("String printing is not supported\n");
			pe->params[i] = (uintptr_t)asprintf("<String @ 0x%08x>", e->params[i]);
			pe->subst_mask |= 1 << i;
			++i;
			p += 2;
		} else if (p + 2 < t_end && p[1] == 'p' && p[2] == 'U') {
			/* %pUx format specifier */
			/* substitute UUID entry address with formatted string pointer from heap */
			pe->params[i] = (uintptr_t)asprintf_uuid(p, e->params[i], use_colors,
								 &uuid_fmt_len);
			pe->subst_mask |= 1 << i;
			++i;
			/* replace uuid formatter with %s */
			p[1] = 's';
			memmove(&p[2], &p[uuid_fmt_len], (int)(t_end - &p[uuid_fmt_len]) + 1);
			p += uuid_fmt_len - 2;
			t_end -= uuid_fmt_len - 2;
		} else if (p + 2 < t_end && p[1] == 'p' && p[2] == 'Q') {
			/* %pQ format specifier */
			/* substitute log entry address with formatted entry text */
			pe->params[i] = (uintptr_t)asprintf_entry_text(e->params[i]);
			pe->subst_mask |= 1 << i;
			++i;

			/* replace entry formatter with %s */
			p[1] = 's';
			memmove(&p[2], &p[3], t_end - &p[2]);
			p++;
			t_end--;
		} else {
			/* arguments different from %pU and %pQ should be passed without
			 * modification
			 */
			pe->params[i] = e->params[i];
			++i;
			p += 2;
		}
	}
}

static void free_proc_ldc_entry(struct proc_ldc_entry *pe)
{
	int i;

	for (i = 0; i < TRACE_MAX_PARAMS_COUNT; i++) {
		if (pe->subst_mask & (1 << i))
			free((void *)pe->params[i]);
		pe->params[i] = 0;
	}
}

static double to_usecs(uint64_t time)
{
	/* trace timestamp uses CPU system clock at default 25MHz ticks */
	// TODO: support variable clock rates
	return (double)time / global_config->clock;
}

/** Justified timestamp width for printf format string */
static unsigned int timestamp_width(unsigned int precision)
{
	/* 64bits yields less than 20 digits precision. As reported by
	 * gcc 9.3, this avoids a very long precision causing snprintf()
	 * to truncate time_fmt
	 */
	assert(precision >= 0 && precision < 20);
	/*
	 * 12 digits for units is enough for 1M seconds = 11 days which
	 * should be enough for most test runs.
	 *
	 * Add 1 for the comma when there is one.
	 */
	return 12 + (precision > 0 ? 1 : 0) + precision;
}

static inline void print_table_header(void)
{
	FILE *out_fd = global_config->out_fd;
	int hide_location = global_config->hide_location;
	char time_fmt[32];

	char date_string[64];
	const time_t epoc_secs = time(NULL);
	/* See SOF_IPC_TRACE_DMA_PARAMS_EXT in the kernel sources */
	struct timespec ktime;
	const int gettime_ret = clock_gettime(CLOCK_MONOTONIC, &ktime);

	if (gettime_ret) {
		log_err("clock_gettime() failed: %s\n",
			strerror(gettime_ret));
		exit(1);
	}

	if (global_config->time_precision >= 0) {
		const unsigned int ts_width =
			timestamp_width(global_config->time_precision);
		snprintf(time_fmt, sizeof(time_fmt), "%%-%ds(us)%%%ds  ",
			 ts_width, ts_width);
		fprintf(out_fd, time_fmt, " TIMESTAMP", "DELTA");
	}

	fprintf(out_fd, "%2s %-18s ", "C#", "COMPONENT");
	if (!hide_location)
		fprintf(out_fd, "%-29s ", "LOCATION");
	fprintf(out_fd, "%s", "CONTENT");

	if (global_config->time_precision >= 0) {
		/* e.g.: ktime=4263.487s @ 2021-04-27 14:21:13 -0700 PDT */
		fprintf(out_fd, "\tktime=%lu.%03lus",
			ktime.tv_sec, ktime.tv_nsec / 1000000);
		if (strftime(date_string, sizeof(date_string),
			     "%F %X %z %Z", localtime(&epoc_secs)))
			fprintf(out_fd, "  @  %s", date_string);
	}

	fprintf(out_fd, "\n");
	fflush(out_fd);
}

static const char *get_level_color(uint32_t level)
{
	switch (level) {
	case LOG_LEVEL_CRITICAL:
		return KRED;
	case LOG_LEVEL_WARNING:
		return KYEL;
	default:
		return KNRM;
	}
}

static const char *get_level_name(uint32_t level)
{
	switch (level) {
	case LOG_LEVEL_CRITICAL:
		return "ERROR ";
	case LOG_LEVEL_WARNING:
		return "WARN ";
	default:
		return ""; /* info is usual, do not print anything */
	}
}

static const char *get_component_name(uint32_t trace_class, uint32_t uid_ptr)
{
	const struct snd_sof_uids_header *uids_dict = global_config->uids_dict;
	const struct sof_uuid_entry *uid_entry;

	/* if uid_ptr is non-zero, find name in the ldc file */
	if (uid_ptr) {
		if (uid_ptr < uids_dict->base_address ||
		    uid_ptr >= uids_dict->base_address +
		    uids_dict->data_length)
			return "<uid?>";
		uid_entry = get_uuid_entry(uid_ptr);
		return uid_entry->name;
	}

	/* do not resolve legacy (deprecated) trace class name */
	return "unknown";
}

/* remove superfluous leading file path and shrink to last 20 chars */
static char *format_file_name(char *file_name_raw, int full_name)
{
	char *name;
	int len;

	/* most/all string should have "src" */
	name = strstr(file_name_raw, "src");
	if (!name)
		name = file_name_raw;

	if (full_name)
		return name;
	/* keep the last 24 chars */
	len = strlen(name);
	if (len > 24) {
		char *sep_pos = NULL;

		name += (len - 24);
		sep_pos = strchr(name, '/');
		if (!sep_pos)
			return name;
		while (--sep_pos >= name)
			*sep_pos = '.';
	}
	return name;
}

/** Formats and outputs one entry from the trace + the corresponding
 * ldc_entry from the dictionary passed as arguments. Expects the log
 * variables to have already been copied into the ldc_entry.
 */
static void print_entry_params(const struct log_entry_header *dma_log,
			       const struct ldc_entry *entry, uint64_t last_timestamp)
{
	static int entry_number = 1;
	static uint64_t timestamp_origin;

	FILE *out_fd = global_config->out_fd;
	int use_colors = global_config->use_colors;
	int raw_output = global_config->raw_output;
	int hide_location = global_config->hide_location;
	int time_precision = global_config->time_precision;

	char ids[TRACE_MAX_IDS_STR];
	float dt = to_usecs(dma_log->timestamp - last_timestamp);
	struct proc_ldc_entry proc_entry;
	static char time_fmt[32];
	int ret;

	if (raw_output)
		use_colors = 0;

	/* Something somewhere went wrong */
	if (dt > 1000.0 * 1000.0 * 1000.0)
		dt = NAN;

	if (dma_log->timestamp < last_timestamp) {
		fprintf(out_fd,
			"\n\t\t --- negative DELTA: wrap, IPC_TRACE, other? ---\n\n");
		entry_number = 1;
	}

	/* The first entry:
	 *  - is never shown with a relative TIMESTAMP (to itself!?)
	 *  - shows a zero DELTA
	 */
	if (entry_number == 1) {
		entry_number++;
		/* Display absolute (and random) timestamps */
		timestamp_origin = 0;
		dt = 0;
	} else if (entry_number == 2) {
		entry_number++;
		if (global_config->relative_timestamps == 1)
			/* Switch to relative timestamps from now on. */
			timestamp_origin = last_timestamp;
	} /* We don't need the exact entry_number after 3 */

	if (dma_log->id_0 != INVALID_TRACE_ID &&
	    dma_log->id_1 != INVALID_TRACE_ID)
		sprintf(ids, "%d.%d", (dma_log->id_0 & TRACE_IDS_MASK),
			(dma_log->id_1 & TRACE_IDS_MASK));
	else
		ids[0] = '\0';

	if (raw_output) { /* "raw" means script-friendly (not all hex) */
		const char *entry_fmt = "%s%u %u %s%s%s ";

		if (time_precision >= 0)
			snprintf(time_fmt, sizeof(time_fmt), "%%.%df %%.%df ",
				 time_precision, time_precision);

		fprintf(out_fd, entry_fmt,
			entry->header.level == use_colors ?
				(LOG_LEVEL_CRITICAL ? KRED : KNRM) : "",
			dma_log->core_id,
			entry->header.level,
			get_component_name(entry->header.component_class, dma_log->uid),
			raw_output && strlen(ids) ? "-" : "",
			ids);
		if (time_precision >= 0)
			fprintf(out_fd, time_fmt,
				to_usecs(dma_log->timestamp - timestamp_origin), dt);
		if (!hide_location)
			fprintf(out_fd, "(%s:%u) ",
				format_file_name(entry->file_name, raw_output),
				entry->header.line_idx);
	} else {
		if (time_precision >= 0) {
			const unsigned int ts_width = timestamp_width(time_precision);

			snprintf(time_fmt, sizeof(time_fmt),
				 "%%s[%%%d.%df] (%%%d.%df)%%s ",
				 ts_width, time_precision, ts_width, time_precision);

			fprintf(out_fd, time_fmt,
				use_colors ? KGRN : "",
				to_usecs(dma_log->timestamp - timestamp_origin), dt,
				use_colors ? KNRM : "");
		}

		/* core id */
		fprintf(out_fd, "c%d ", dma_log->core_id);

		/* component name and id */
		fprintf(out_fd, "%s%-12s %-5s%s ",
			use_colors ? KYEL : "",
			get_component_name(entry->header.component_class, dma_log->uid),
			ids,
			use_colors ? KNRM : "");

		/* location */
		if (!hide_location)
			fprintf(out_fd, "%24s:%-4u ",
				format_file_name(entry->file_name, raw_output),
				entry->header.line_idx);

		/* level name */
		fprintf(out_fd, "%s%s",
			use_colors ? get_level_color(entry->header.level) : "",
			get_level_name(entry->header.level));
	}

	/* Minimal, printf-like formatting */
	process_params(&proc_entry, entry, use_colors);

	switch (proc_entry.header.params_num) {
	case 0:
		ret = fprintf(out_fd, "%s", proc_entry.text);
		break;
	case 1:
		ret = fprintf(out_fd, proc_entry.text, proc_entry.params[0]);
		break;
	case 2:
		ret = fprintf(out_fd, proc_entry.text, proc_entry.params[0], proc_entry.params[1]);
		break;
	case 3:
		ret = fprintf(out_fd, proc_entry.text, proc_entry.params[0], proc_entry.params[1],
			      proc_entry.params[2]);
		break;
	case 4:
		ret = fprintf(out_fd, proc_entry.text, proc_entry.params[0], proc_entry.params[1],
			      proc_entry.params[2], proc_entry.params[3]);
		break;
	default:
		log_err("Unsupported number of arguments for '%s'", proc_entry.text);
		ret = 0; /* don't log ferror */
		break;
	}
	free_proc_ldc_entry(&proc_entry);
	/* log format text comes from ldc file (may be invalid), so error check is needed here */
	if (ret < 0)
		log_err("trace fprintf failed for '%s', %d '%s'",
			proc_entry.text, ferror(out_fd), strerror(ferror(out_fd)));
	fprintf(out_fd, "%s\n", use_colors ? KNRM : "");
	fflush(out_fd);
}

static int read_entry_from_ldc_file(struct ldc_entry *entry, uint32_t log_entry_address)
{
	uint32_t base_address = global_config->logs_header->base_address;
	uint32_t data_offset = global_config->logs_header->data_offset;

	int ret;

	/* evaluate entry offset in input file */
	uint32_t entry_offset = (log_entry_address - base_address) + data_offset;

	entry->file_name = NULL;
	entry->text = NULL;
	entry->params = NULL;

	/* set file position to beginning of processed entry */
	fseek(global_config->ldc_fd, entry_offset, SEEK_SET);

	/* fetching elf header params */
	ret = fread(&entry->header, sizeof(entry->header), 1, global_config->ldc_fd);
	if (!ret) {
		ret = -ferror(global_config->ldc_fd);
		goto out;
	}
	if (entry->header.file_name_len > TRACE_MAX_FILENAME_LEN) {
		log_err("Invalid filename length or ldc file does not match firmware\n");
		ret = -EINVAL;
		goto out;
	}
	entry->file_name = (char *)malloc(entry->header.file_name_len);

	if (!entry->file_name) {
		log_err("can't allocate %d byte for entry.file_name\n",
			entry->header.file_name_len);
		ret = -ENOMEM;
		goto out;
	}

	ret = fread(entry->file_name, sizeof(char), entry->header.file_name_len,
		    global_config->ldc_fd);
	if (ret != entry->header.file_name_len) {
		ret = -ferror(global_config->ldc_fd);
		goto out;
	}

	/* fetching text */
	if (entry->header.text_len > TRACE_MAX_TEXT_LEN) {
		log_err("Invalid text length.\n");
		ret = -EINVAL;
		goto out;
	}
	entry->text = (char *)malloc(entry->header.text_len);
	if (!entry->text) {
		log_err("can't allocate %d byte for entry.text\n", entry->header.text_len);
		ret = -ENOMEM;
		goto out;
	}
	ret = fread(entry->text, sizeof(char), entry->header.text_len, global_config->ldc_fd);
	if (ret != entry->header.text_len) {
		ret = -ferror(global_config->ldc_fd);
		goto out;
	}

	return 0;

out:
	free(entry->text);
	entry->text = NULL;
	free(entry->file_name);
	entry->file_name = NULL;

	return ret;
}

/** Gets the dictionary entry matching the log entry argument, reads
 * from the log the variable number of arguments needed by this entry
 * and passes everything to print_entry_params() to finish processing
 * this log entry. So not just "fetch" but everything else after it too.
 *
 * @param[in] dma_log protocol header from any trace (not just from the
 * "DMA" trace)
 * @param[out] last_timestamp timestamp found for this entry
 */
static int fetch_entry(const struct log_entry_header *dma_log, uint64_t *last_timestamp)
{
	struct ldc_entry entry;
	int ret;

	ret = read_entry_from_ldc_file(&entry, dma_log->log_entry_address);
	if (ret < 0)
		goto out;

	/* fetching entry params from dma dump */
	if (entry.header.params_num > TRACE_MAX_PARAMS_COUNT) {
		log_err("Invalid number of parameters.\n");
		ret = -EINVAL;
		goto out;
	}
	entry.params = (uint32_t *)malloc(sizeof(uint32_t) * entry.header.params_num);
	if (!entry.params) {
		log_err("can't allocate %d byte for entry.params\n",
			(int)(sizeof(uint32_t) * entry.header.params_num));
		ret = -ENOMEM;
		goto out;
	}

	if (global_config->serial_fd < 0) {
		ret = fread(entry.params, sizeof(uint32_t), entry.header.params_num,
			    global_config->in_fd);
		if (ret != entry.header.params_num) {
			ret = -ferror(global_config->in_fd);
			goto out;
		}
	} else {
		size_t size = sizeof(uint32_t) * entry.header.params_num;
		uint8_t *n;

		/* Repeatedly read() how much we still miss until we got
		 * enough for the number of params needed by this
		 * particular statement.
		 */
		for (n = (uint8_t *)entry.params; size; n += ret, size -= ret) {
			ret = read(global_config->serial_fd, n, size);
			if (ret < 0) {
				ret = -errno;
				goto out;
			}
			if (ret != size)
				log_err("Partial read of %u bytes of %lu, reading more\n",
					ret, size);
		}
	}

	/* printing entry content */
	print_entry_params(dma_log, &entry, *last_timestamp);
	*last_timestamp = dma_log->timestamp;

	/* set f_ldc file position to the beginning */
	rewind(global_config->ldc_fd);

	ret = 0;
out:
	/* free alocated memory */
	free(entry.params);
	free(entry.text);
	free(entry.file_name);

	return ret;
}

static int serial_read(uint64_t *last_timestamp)
{
	struct log_entry_header dma_log;
	size_t len;
	uint8_t *n;
	int ret;

	for (len = 0, n = (uint8_t *)&dma_log; len < sizeof(dma_log); n += sizeof(uint32_t)) {
		ret = read(global_config->serial_fd, n, sizeof(*n) * sizeof(uint32_t));
		if (ret < 0)
			return -errno;

		/* In the beginning we read 1 spurious byte */
		if (ret < sizeof(*n) * sizeof(uint32_t))
			n -= sizeof(uint32_t);
		else
			len += ret;
	}

	/* Skip all trace_point() values, although this test isn't 100% reliable */
	while ((dma_log.log_entry_address < global_config->logs_header->base_address) ||
	       dma_log.log_entry_address > global_config->logs_header->base_address +
	       global_config->logs_header->data_length) {
		/*
		 * 8 characters and a '\n' come from the serial port, append a
		 * '\0'
		 */
		char s[10];
		uint8_t *c;
		size_t len;

		c = (uint8_t *)&dma_log;

		memcpy(s, c, sizeof(s) - 1);
		s[sizeof(s) - 1] = '\0';
		fprintf(global_config->out_fd, "Trace point %s", s);

		memmove(&dma_log, c + 9, sizeof(dma_log) - 9);

		c = (uint8_t *)(&dma_log + 1) - 9;
		for (len = 9; len; len -= ret, c += ret) {
			ret = read(global_config->serial_fd, c, len);
			if (ret < 0)
				return ret;
		}
	}

	/* fetching entry from elf dump and complete processing this log
	 * line
	 */
	return fetch_entry(&dma_log, last_timestamp);
}

/** Main logger loop */
static int logger_read(void)
{
	struct log_entry_header dma_log;
	int ret = 0;
	uint64_t last_timestamp = 0;

	bool ldc_address_OK = false;
	unsigned int skipped_dwords = 0;

	if (!global_config->raw_output)
		print_table_header();

	if (global_config->serial_fd >= 0)
		/* Wait for CTRL-C */
		for (;;) {
			ret = serial_read(&last_timestamp);
			if (ret < 0)
				return ret;
		}

	/* One iteration per log statement */
	while (!ferror(global_config->in_fd)) {
		/* getting entry parameters from dma dump */
		ret = fread(&dma_log, sizeof(dma_log), 1, global_config->in_fd);
		if (ret != 1) {
			/*
			 * use ferror (not errno) to check fread fail -
			 * see https://www.gnu.org/software/gnulib/manual/html_node/fread.html
			 */
			ret = -ferror(global_config->in_fd);
			if (ret) {
				log_err("in %s(), fread(..., %s) failed: %s(%d)\n",
					__func__, global_config->in_file,
					strerror(-ret), ret);
				return ret;
			}
			/* for trace mode, try to reopen */
			if (global_config->trace) {
				if (freopen(NULL, "rb", global_config->in_fd)) {
					continue;
				} else {
					log_err("in %s(), freopen(..., %s) failed: %s(%d)\n",
						__func__, global_config->in_file,
						strerror(errno), errno);
					return -errno;
				}
			} else {
				/* EOF */
				if (!feof(global_config->in_fd))
					log_err("file '%s' is unaligned with trace entry size (%ld)\n",
						global_config->in_file, sizeof(dma_log));
				break;
			}
		}

		/* checking if received trace address is located in
		 * entry section in elf file.
		 */
		if (dma_log.log_entry_address < global_config->logs_header->base_address ||
		    dma_log.log_entry_address > global_config->logs_header->base_address +
		    global_config->logs_header->data_length) {
			/* Finding uninitialized and incomplete log statements in the
			 * mailbox ring buffer is routine. Take note in both cases but
			 * report errors only for the DMA trace.
			 */
			if (global_config->trace && ldc_address_OK) {
				/* FIXME: make this a log_err() */
				fprintf(global_config->out_fd,
					"warn: log_entry_address %#8x is not in dictionary range!\n",
					dma_log.log_entry_address);
				fprintf(global_config->out_fd,
					"warn: Seeking forward 4 bytes at a time until re-synchronize.\n");
			}
			ldc_address_OK = false;
			/* When the address is not correct, move forward by one DWORD (not
			 * entire struct dma_log)
			 */
			fseek(global_config->in_fd, -(sizeof(dma_log) - sizeof(uint32_t)),
			      SEEK_CUR);
			skipped_dwords++;
			continue;

		} else if (!ldc_address_OK) {
			 /* Just found a valid address (again) */

			/* At this point, skipped_dwords can be == 0
			 * only when we just started to run.
			 */
			if (skipped_dwords != 0) {
				fprintf(global_config->out_fd,
					"\nFound valid LDC address after skipping %zu bytes (one line uses %zu + 0 to 16 bytes)\n",
				       sizeof(uint32_t) * skipped_dwords, sizeof(dma_log));
			}

			ldc_address_OK = true;
			skipped_dwords = 0;
		}

		/* fetching entry from elf dump */
		ret = fetch_entry(&dma_log, &last_timestamp);
		if (ret)
			break;
	}

	/* End of (etrace) file */
	fprintf(global_config->out_fd,
		"Skipped %zu bytes after the last statement",
		sizeof(uint32_t) * skipped_dwords);

	/* maximum 4 arguments supported */
	if (skipped_dwords < sizeof(dma_log) + 4 * sizeof(uint32_t))
		fprintf(global_config->out_fd,
			". Wrap possible, check the start of the output for later logs");

	fprintf(global_config->out_fd, ".\n");

	return ret;
}

/* fw verification */
static int verify_fw_ver(void)
{
	struct sof_ipc_fw_version ver;
	int count;

	if (!global_config->version_fd)
		return 0;

	/* here fw verification should be exploited */
	count = fread(&ver, sizeof(ver), 1, global_config->version_fd);
	if (!count) {
		log_err("Error while reading %s.\n", global_config->version_file);
		return -ferror(global_config->version_fd);
	}

	/* compare source hash value from version file and ldc file */
	if (ver.src_hash != global_config->logs_header->version.src_hash) {
		log_err("src hash value from version file (0x%x) differ from src hash version saved in dictionary (0x%x).\n",
			ver.src_hash, global_config->logs_header->version.src_hash);
		return -EINVAL;
	}
	return 0;
}

static int dump_ldc_info(void)
{
	struct snd_sof_uids_header *uids_dict = global_config->uids_dict;
	ssize_t remaining = uids_dict->data_length;
	const struct sof_uuid_entry *uid_ptr;
	FILE *out_fd = global_config->out_fd;
	uintptr_t uid_addr;
	int cnt = 0;
	char *name;

	fprintf(out_fd, "logger ABI Version is\t%d:%d:%d\n",
		SOF_ABI_VERSION_MAJOR(SOF_ABI_DBG_VERSION),
		SOF_ABI_VERSION_MINOR(SOF_ABI_DBG_VERSION),
		SOF_ABI_VERSION_PATCH(SOF_ABI_DBG_VERSION));
	fprintf(out_fd, "ldc_file ABI Version is\t%d:%d:%d\n",
		SOF_ABI_VERSION_MAJOR(global_config->logs_header->version.abi_version),
		SOF_ABI_VERSION_MINOR(global_config->logs_header->version.abi_version),
		SOF_ABI_VERSION_PATCH(global_config->logs_header->version.abi_version));
	fprintf(out_fd, "\n");
	fprintf(out_fd, "Components uuid dictionary size:\t%ld bytes\n",
		remaining);
	fprintf(out_fd, "Components uuid base address:   \t0x%x\n",
		uids_dict->base_address);
	fprintf(out_fd, "Components uuid entries:\n");
	fprintf(out_fd, "\t%10s  %38s %s\n", "ADDRESS", "UUID", "NAME");

	uid_ptr = (const struct sof_uuid_entry *)
		  ((uintptr_t)uids_dict + uids_dict->data_offset);

	while (remaining > 0) {
		name = format_uid_raw(&uid_ptr[cnt], 0, 0, false, false);
		uid_addr = get_uuid_key(&uid_ptr[cnt]);
		fprintf(out_fd, "\t0x%lX  %s\n", uid_addr, name);

		if (name) {
			free(name);
			name = NULL;
		}
		remaining -= sizeof(struct sof_uuid_entry);
		++cnt;
	}

	fprintf(out_fd, "\t-------------------------------------------------- cnt: %d\n",
		cnt);

	return 0;
}

int convert(struct convert_config *config)
{
	struct snd_sof_logs_header snd;
	struct snd_sof_uids_header uids_hdr;
	int count, ret = 0;

	config->logs_header = &snd;
	global_config = config;

	count = fread(&snd, sizeof(snd), 1, config->ldc_fd);
	if (!count) {
		log_err("Error while reading %s.\n", config->ldc_file);
		return -ferror(config->ldc_fd);
	}

	if (strncmp((char *) snd.sig, SND_SOF_LOGS_SIG, SND_SOF_LOGS_SIG_SIZE)) {
		log_err("Invalid ldc file signature.\n");
		return -EINVAL;
	}

	ret = verify_fw_ver();
	if (ret)
		return ret;

	/* default logger and ldc_file abi verification */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_DBG_VERSION,
					 snd.version.abi_version)) {
		log_err("abi version in %s file does not coincide with abi version used by logger.\n",
			config->ldc_file);
		log_err("logger ABI Version is %d:%d:%d\n",
			SOF_ABI_VERSION_MAJOR(SOF_ABI_DBG_VERSION),
			SOF_ABI_VERSION_MINOR(SOF_ABI_DBG_VERSION),
			SOF_ABI_VERSION_PATCH(SOF_ABI_DBG_VERSION));
		log_err("ldc_file ABI Version is %d:%d:%d\n",
			SOF_ABI_VERSION_MAJOR(snd.version.abi_version),
			SOF_ABI_VERSION_MINOR(snd.version.abi_version),
			SOF_ABI_VERSION_PATCH(snd.version.abi_version));
		return -EINVAL;
	}

	/* read uuid section header */
	fseek(config->ldc_fd, snd.data_offset + snd.data_length, SEEK_SET);
	count = fread(&uids_hdr, sizeof(uids_hdr), 1, config->ldc_fd);
	if (!count) {
		log_err("Error while reading uuids header from %s.\n", config->ldc_file);
		return -ferror(config->ldc_fd);
	}
	if (strncmp((char *)uids_hdr.sig, SND_SOF_UIDS_SIG,
		    SND_SOF_UIDS_SIG_SIZE)) {
		log_err("invalid uuid section signature.\n");
		return -EINVAL;
	}
	config->uids_dict = calloc(1, sizeof(uids_hdr) + uids_hdr.data_length);
	if (!config->uids_dict) {
		log_err("failed to alloc memory for uuids.\n");
		return -ENOMEM;
	}
	memcpy(config->uids_dict, &uids_hdr, sizeof(uids_hdr));
	count = fread(config->uids_dict + 1, uids_hdr.data_length, 1,
		      config->ldc_fd);
	if (!count) {
		log_err("failed to read uuid section data.\n");
		ret = -ferror(config->ldc_fd);
		goto out;
	}

	if (config->dump_ldc) {
		ret = dump_ldc_info();
		goto out;
	}

	if (config->filter_config) {
		ret = filter_update_firmware();
		if (ret) {
			log_err("failed to apply trace filter, %d.\n", ret);
			goto out;
		}
	}

	ret = logger_read();
out:
	free(config->uids_dict);
	return ret;
}
