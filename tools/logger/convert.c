// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko	<bartoszx.kokoszko@linux.intel.com>
//	   Artur Kloniecki	<arturx.kloniecki@linux.intel.com>

#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <sof/lib/uuid.h>
#include <user/abi_dbg.h>
#include <user/trace.h>
#include "convert.h"

#define CEIL(a, b) ((a+b-1)/b)

#define TRACE_MAX_PARAMS_COUNT		4
#define TRACE_MAX_TEXT_LEN		1024
#define TRACE_MAX_FILENAME_LEN		128
#define TRACE_MAX_IDS_STR		10
#define TRACE_IDS_MASK			((1 << TRACE_ID_LENGTH) - 1)
#define INVALID_TRACE_ID		(-1 & TRACE_IDS_MASK)

struct ldc_entry_header {
	uint32_t level;
	uint32_t component_class;
	uint32_t params_num;
	uint32_t line_idx;
	uint32_t file_name_len;
	uint32_t text_len;
};

struct ldc_entry {
	struct ldc_entry_header header;
	char *file_name;
	char *text;
	uint32_t *params;
};

struct proc_ldc_entry {
	int subst_mask;
	struct ldc_entry_header header;
	char *file_name;
	char *text;
	uintptr_t params[TRACE_MAX_PARAMS_COUNT];
};

static const char *BAD_PTR_STR = "<bad uid ptr %x>";

char *vasprintf(const char *format, va_list args)
{
	va_list args_copy;
	int size;
	char localbuf[1];
	char *result;

	va_copy(args_copy, args);
	size = vsnprintf(localbuf, 1, format, args_copy);
	va_end(args_copy);

	result = calloc(1, size + 1);
	if (result)
		vsnprintf(result, size + 1, format, args);
	return result;
}

char *asprintf(const char *format, ...)
{
	va_list args;
	char *result;

	va_start(args, format);
	result = vasprintf(format, args);
	va_end(args);

	return result;
}

static void log_err(FILE *out_fd, const char *fmt, ...)
{
	static const char prefix[] = "error: ";
	ssize_t needed_size;
	va_list args, args_alloc;
	char *buff;

	va_start(args, fmt);

	va_copy(args_alloc, args);
	needed_size = vsnprintf(NULL, 0, fmt, args_alloc) + 1;
	buff = malloc(needed_size);
	va_end(args_alloc);

	if (buff) {
		vsprintf(buff, fmt, args);
		fprintf(stderr, "%s%s", prefix, buff);

		/* take care about out_fd validity and duplicated logging */
		if (out_fd && out_fd != stderr && out_fd != stdout) {
			fprintf(out_fd, "%s%s", prefix, buff);
			fflush(out_fd);
		}
		free(buff);
	} else {
		fprintf(stderr, "%s", prefix);
		vfprintf(stderr, fmt, args);
	}

	va_end(args);
}

char *format_uid_raw(const struct sof_uuid *uid_val, int use_colors,
		     int name_first)
{
	const char *name = (const char *)(uid_val + 1) + 4;
	char *str;

	str = asprintf("%s%s%s<%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x>%s%s%s",
		use_colors ? KBLU : "",
		name_first ? name : "",
		name_first ? " " : "",
		uid_val->a, uid_val->b, uid_val->c,
		uid_val->d[0], uid_val->d[1], uid_val->d[2],
		uid_val->d[3], uid_val->d[4], uid_val->d[5],
		uid_val->d[6], uid_val->d[7],
		name_first ? "" : " ",
		name_first ? "" : name,
		use_colors ? KNRM : "");
	return str;
}

const char *format_uid(const struct snd_sof_uids_header *uids_dict,
		       uint32_t uid_ptr,
		       int use_colors)
{
	char *str;

	if (uid_ptr < uids_dict->base_address ||
	    uid_ptr >= uids_dict->base_address + uids_dict->data_length) {
		str = calloc(1, strlen(BAD_PTR_STR) + 1 + 6);
		sprintf(str, BAD_PTR_STR, uid_ptr);
	} else {
		const struct sof_uuid *uid_val = (const struct sof_uuid *)
			((uint8_t *)uids_dict + uids_dict->data_offset +
			uid_ptr - uids_dict->base_address);
		str = format_uid_raw(uid_val, use_colors, 1);

	}
	return str;
}

static void process_params(struct proc_ldc_entry *pe,
			   const struct ldc_entry *e,
			   const struct snd_sof_uids_header *uids_dict,
			   int use_colors)
{
	const char *p = e->text;
	const char *t_end = p + strlen(e->text);
	unsigned int par_bit = 1;
	int i;

	pe->subst_mask = 0;
	pe->header =  e->header;
	pe->file_name = e->file_name;
	pe->text = e->text;

	/* scan the text for possible replacements */
	while ((p = strchr(p, '%'))) {
		if (p < t_end - 1 && *(p + 1) == 's')
			pe->subst_mask += par_bit;
		par_bit <<= 1;
		++p;
	}

	for (i = 0; i < e->header.params_num; i++) {
		pe->params[i] = e->params[i];
		if (pe->subst_mask & (1 << i))
			pe->params[i] = (uintptr_t)format_uid(uids_dict,
							      e->params[i],
							      use_colors);
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

static double to_usecs(uint64_t time, double clk)
{
	/* trace timestamp uses CPU system clock at default 25MHz ticks */
	// TODO: support variable clock rates
	return (double)time / clk;
}


static inline void print_table_header(FILE *out_fd)
{
	fprintf(out_fd, "%18s %18s %2s %-18s %-29s  %s\n",
		"TIMESTAMP", "DELTA", "C#", "COMPONENT", "LOCATION",
		"CONTENT");
	fflush(out_fd);
}

#define CASE(x) \
	case(TRACE_CLASS_##x): return #x

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

static
const char *get_component_name(const struct snd_sof_uids_header *uids_dict,
			       uint32_t trace_class, uint32_t uid_ptr)
{
	/* if uid_ptr is non-zero, find name in the ldc file */
	if (uid_ptr) {
		if (uid_ptr < uids_dict->base_address ||
		    uid_ptr >= uids_dict->base_address +
		    uids_dict->data_length)
			return "<uid?>";
		const struct sof_uuid *uid_val = (const struct sof_uuid *)
			((uint8_t *)uids_dict + uids_dict->data_offset +
			 uid_ptr - uids_dict->base_address);

		return (const char *)(uid_val + 1) + sizeof(uint32_t);
	}

	/* otherwise print legacy trace class name */
	switch (trace_class) {
		CASE(IRQ);
		CASE(IPC);
		CASE(PIPE);
		CASE(DAI);
		CASE(DMA);
		CASE(COMP);
		CASE(WAIT);
		CASE(LOCK);
		CASE(MEM);
		CASE(BUFFER);
		CASE(SA);
		CASE(POWER);
		CASE(IDC);
		CASE(CPU);
		CASE(CLK);
		CASE(EDF);
		CASE(SCHEDULE);
		CASE(SCHEDULE_LL);
		CASE(CHMAP);
		CASE(NOTIFIER);
		CASE(MN);
		CASE(PROBE);
		CASE(SMART_AMP);
	default: return "unknown";
	}
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

static void print_entry_params(FILE *out_fd,
	const struct snd_sof_uids_header *uids_dict,
	const struct log_entry_header *dma_log, const struct ldc_entry *entry,
	uint64_t last_timestamp, double clock, int use_colors, int raw_output)
{
	char ids[TRACE_MAX_IDS_STR];
	float dt = to_usecs(dma_log->timestamp - last_timestamp, clock);
	struct proc_ldc_entry proc_entry;

	if (raw_output)
		use_colors = 0;

	if (dt < 0 || dt > 1000.0 * 1000.0 * 1000.0)
		dt = NAN;

	if (dma_log->id_0 != INVALID_TRACE_ID &&
	    dma_log->id_1 != INVALID_TRACE_ID)
		sprintf(ids, "%d.%d", (dma_log->id_0 & TRACE_IDS_MASK),
			(dma_log->id_1 & TRACE_IDS_MASK));
	else
		ids[0] = '\0';

	if (raw_output) {
		const char *entry_fmt = "%s%u %u %s%s%s %.6f %.6f (%s:%u) ";

		fprintf(out_fd, entry_fmt,
			entry->header.level == use_colors ?
				(LOG_LEVEL_CRITICAL ? KRED : KNRM) : "",
			dma_log->core_id,
			entry->header.level,
			get_component_name(uids_dict,
					   entry->header.component_class,
					   dma_log->uid),
			raw_output && strlen(ids) ? "-" : "",
			ids,
			to_usecs(dma_log->timestamp, clock),
			dt,
			format_file_name(entry->file_name, raw_output),
			entry->header.line_idx);
	} else {
		/* timestamp */
		fprintf(out_fd, "%s[%16.6f] (%16.6f)%s ",
			use_colors ? KGRN : "",
			to_usecs(dma_log->timestamp, clock), dt,
			use_colors ? KNRM : "");

		/* core id */
		fprintf(out_fd, "c%d ", dma_log->core_id);

		/* component name and id */
		fprintf(out_fd, "%s%-12s %-5s%s ",
			use_colors ? KYEL : "",
			get_component_name(uids_dict,
					   entry->header.component_class,
					   dma_log->uid),
			ids,
			use_colors ? KNRM : "");

		/* location */
		fprintf(out_fd, "%24s:%-4u  ",
			format_file_name(entry->file_name, raw_output),
			entry->header.line_idx);

		/* level name */
		fprintf(out_fd, "%s%s",
			use_colors ? get_level_color(entry->header.level) : "",
			get_level_name(entry->header.level));
	}

	process_params(&proc_entry, entry, uids_dict, use_colors);

	switch (proc_entry.header.params_num) {
	case 0:
		fprintf(out_fd, "%s", proc_entry.text);
		break;
	case 1:
		fprintf(out_fd, proc_entry.text, proc_entry.params[0]);
		break;
	case 2:
		fprintf(out_fd, proc_entry.text, proc_entry.params[0],
			proc_entry.params[1]);
		break;
	case 3:
		fprintf(out_fd, proc_entry.text, proc_entry.params[0],
			proc_entry.params[1], proc_entry.params[2]);
		break;
	case 4:
		fprintf(out_fd, proc_entry.text, proc_entry.params[0],
			proc_entry.params[1], proc_entry.params[2],
			proc_entry.params[3]);
		break;
	}
	free_proc_ldc_entry(&proc_entry);
	fprintf(out_fd, "%s\n", use_colors ? KNRM : "");
	fflush(out_fd);
}

static int fetch_entry(const struct convert_config *config,
	uint32_t base_address, uint32_t data_offset,
	const struct log_entry_header *dma_log, uint64_t *last_timestamp)
{
	struct ldc_entry entry;
	uint32_t entry_offset;

	int ret;

	entry.file_name = NULL;
	entry.text = NULL;
	entry.params = NULL;

	/* evaluate entry offset in input file */
	entry_offset = dma_log->log_entry_address - base_address;

	/* set file position to beginning of processed entry */
	fseek(config->ldc_fd, entry_offset + data_offset, SEEK_SET);

	/* fetching elf header params */
	ret = fread(&entry.header, sizeof(entry.header), 1, config->ldc_fd);
	if (!ret) {
		ret = -ferror(config->ldc_fd);
		goto out;
	}
	if (entry.header.file_name_len > TRACE_MAX_FILENAME_LEN) {
		log_err(config->out_fd,
			"Invalid filename length or ldc file does not match firmware\n");
		ret = -EINVAL;
		goto out;
	}
	entry.file_name = (char *)malloc(entry.header.file_name_len);

	if (!entry.file_name) {
		log_err(config->out_fd,
			"can't allocate %d byte for entry.file_name\n",
			entry.header.file_name_len);
		ret = -ENOMEM;
		goto out;
	}

	ret = fread(entry.file_name, sizeof(char), entry.header.file_name_len,
		config->ldc_fd);
	if (ret != entry.header.file_name_len) {
		ret = -ferror(config->ldc_fd);
		goto out;
	}

	/* fetching text */
	if (entry.header.text_len > TRACE_MAX_TEXT_LEN) {
		log_err(config->out_fd,
			"Invalid text length.\n");
		ret = -EINVAL;
		goto out;
	}
	entry.text = (char *)malloc(entry.header.text_len);
	if (!entry.text) {
		log_err(config->out_fd,
			"can't allocate %d byte for entry.text\n",
			entry.header.text_len);
		ret = -ENOMEM;
		goto out;
	}
	ret = fread(entry.text, sizeof(char), entry.header.text_len, config->ldc_fd);
	if (ret != entry.header.text_len) {
		ret = -ferror(config->ldc_fd);
		goto out;
	}

	/* fetching entry params from dma dump */
	if (entry.header.params_num > TRACE_MAX_PARAMS_COUNT) {
		log_err(config->out_fd,
			"Invalid number of parameters.\n");
		ret = -EINVAL;
		goto out;
	}
	entry.params = (uint32_t *)malloc(sizeof(uint32_t) *
		entry.header.params_num);
	if (!entry.params) {
		log_err(config->out_fd,
			"can't allocate %d byte for entry.params\n",
			(int)(sizeof(uint32_t) * entry.header.params_num));
		ret = -ENOMEM;
		goto out;
	}

	if (config->serial_fd < 0) {
		ret = fread(entry.params, sizeof(uint32_t),
			    entry.header.params_num, config->in_fd);
		if (ret != entry.header.params_num) {
			ret = -ferror(config->in_fd);
			goto out;
		}
	} else {
		size_t size = sizeof(uint32_t) * entry.header.params_num;
		uint8_t *n;

		for (n = (uint8_t *)entry.params; size;
		     n += ret, size -= ret) {
			ret = read(config->serial_fd, n, size);
			if (ret < 0) {
				ret = -errno;
				goto out;
			}
			if (ret != size)
				log_err(config->out_fd,
					"Partial read of %u bytes of %lu.\n",
					ret, size);
		}
	}

	/* printing entry content */
	print_entry_params(config->out_fd,
			   config->uids_dict,
			   dma_log, &entry, *last_timestamp,
			   config->clock, config->use_colors,
			   config->raw_output);
	*last_timestamp = dma_log->timestamp;

	/* set f_ldc file position to the beginning */
	rewind(config->ldc_fd);

	ret = 0;
out:
	/* free alocated memory */
	free(entry.params);
	free(entry.text);
	free(entry.file_name);

	return ret;
}

static int serial_read(const struct convert_config *config,
	struct snd_sof_logs_header *snd, uint64_t *last_timestamp)
{
	struct log_entry_header dma_log;
	size_t len;
	uint8_t *n;
	int ret;

	for (len = 0, n = (uint8_t *)&dma_log; len < sizeof(dma_log);
		n += sizeof(uint32_t)) {

		ret = read(config->serial_fd, n, sizeof(*n) * sizeof(uint32_t));
		if (ret < 0)
			return -errno;

		/* In the beginning we read 1 spurious byte */
		if (ret < sizeof(*n) * sizeof(uint32_t))
			n -= sizeof(uint32_t);
		else
			len += ret;
	}

	/* Skip all trace_point() values, although this test isn't 100% reliable */
	while ((dma_log.log_entry_address < snd->base_address) ||
	       dma_log.log_entry_address > snd->base_address + snd->data_length) {
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
		fprintf(config->out_fd, "Trace point %s", s);

		memmove(&dma_log, c + 9, sizeof(dma_log) - 9);

		c = (uint8_t *)(&dma_log + 1) - 9;
		for (len = 9; len; len -= ret, c += ret) {
			ret = read(config->serial_fd, c, len);
			if (ret < 0)
				return ret;
		}
	}

	/* fetching entry from elf dump */
	return fetch_entry(config, snd->base_address, snd->data_offset,
			   &dma_log, last_timestamp);
}

static int logger_read(const struct convert_config *config,
	struct snd_sof_logs_header *snd)
{
	struct log_entry_header dma_log;
	int ret = 0;
	uint64_t last_timestamp = 0;

	if (!config->raw_output)
		print_table_header(config->out_fd);

	if (config->serial_fd >= 0)
		/* Wait for CTRL-C */
		for (;;) {
			ret = serial_read(config, snd, &last_timestamp);
			if (ret < 0)
				return ret;
		}

	while (!ferror(config->in_fd)) {
		/* getting entry parameters from dma dump */
		ret = fread(&dma_log, sizeof(dma_log), 1, config->in_fd);
		if (!ret) {
			if (config->trace && !ferror(config->in_fd)) {
				freopen(NULL, "r", config->in_fd);
				continue;
			}

			return -ferror(config->in_fd);
		}

		/* checking if received trace address is located in
		 * entry section in elf file.
		 */
		if ((dma_log.log_entry_address < snd->base_address) ||
			dma_log.log_entry_address > snd->base_address + snd->data_length) {
			/* in case the address is not correct input fd should be
			 * move forward by one DWORD, not entire struct dma_log
			 */
			fseek(config->in_fd, -(sizeof(dma_log) - sizeof(uint32_t)), SEEK_CUR);
			continue;
		}

		/* fetching entry from elf dump */
		ret = fetch_entry(config, snd->base_address, snd->data_offset,
				  &dma_log, &last_timestamp);
		if (ret)
			break;
	}

	return ret;
}

/* fw verification */
static int verify_fw_ver(const struct convert_config *config,
			 const struct snd_sof_logs_header *snd)
{
	struct sof_ipc_fw_version ver;
	int count, ret = 0;

	if (!config->version_fd)
		return 0;

	/* here fw verification should be exploited */
	count = fread(&ver, sizeof(ver), 1, config->version_fd);
	if (!count) {
		log_err(config->out_fd, "Error while reading %s.\n",
			config->version_file);
		return -ferror(config->version_fd);
	}

	ret = memcmp(&ver, &snd->version, sizeof(struct sof_ipc_fw_version));
	if (ret) {
		log_err(config->out_fd,
			"fw version in %s file does not coincide with fw version in %s file.\n",
			config->ldc_file, config->version_file);
		return -EINVAL;
	}

	/* logger and version_file abi dbg verification */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_DBG_VERSION,
					 ver.abi_version)) {
		log_err(config->out_fd,
			"abi version in %s file does not coincide with abi version used by logger.\n",
			config->version_file);
		log_err(config->out_fd,
			"logger ABI Version is %d:%d:%d\n",
			SOF_ABI_VERSION_MAJOR(SOF_ABI_DBG_VERSION),
			SOF_ABI_VERSION_MINOR(SOF_ABI_DBG_VERSION),
			SOF_ABI_VERSION_PATCH(SOF_ABI_DBG_VERSION));
		log_err(config->out_fd,
			"version_file ABI Version is %d:%d:%d\n",
			SOF_ABI_VERSION_MAJOR(ver.abi_version),
			SOF_ABI_VERSION_MINOR(ver.abi_version),
			SOF_ABI_VERSION_PATCH(ver.abi_version));
		return -EINVAL;
	}
	return 0;
}

static int dump_ldc_info(struct convert_config *config,
			 const struct snd_sof_logs_header *snd)
{
	struct snd_sof_uids_header *uids_dict = config->uids_dict;
	ssize_t remaining = uids_dict->data_length;
	FILE *out_fd = config->out_fd;
	uint32_t *name_len_ptr;
	ssize_t entry_size;
	uintptr_t uid_addr;
	uintptr_t uid_ptr;
	int cnt = 0;
	char *name;

	fprintf(out_fd, "logger ABI Version is\t%d:%d:%d\n",
		SOF_ABI_VERSION_MAJOR(SOF_ABI_DBG_VERSION),
		SOF_ABI_VERSION_MINOR(SOF_ABI_DBG_VERSION),
		SOF_ABI_VERSION_PATCH(SOF_ABI_DBG_VERSION));
	fprintf(out_fd, "ldc_file ABI Version is\t%d:%d:%d\n",
		SOF_ABI_VERSION_MAJOR(snd->version.abi_version),
		SOF_ABI_VERSION_MINOR(snd->version.abi_version),
		SOF_ABI_VERSION_PATCH(snd->version.abi_version));
	fprintf(out_fd, "\n");
	fprintf(out_fd, "Components uuid dictionary size:\t%ld bytes\n",
		remaining);
	fprintf(out_fd, "Components uuid base address:   \t0x%X\n",
		uids_dict->base_address);
	fprintf(out_fd, "Components uuid entries:\n");
	fprintf(out_fd, "\t%10s  %38s %s\n", "ADDRESS", "UUID", "NAME");

	uid_ptr = (uintptr_t)uids_dict + uids_dict->data_offset;

	while (remaining > 0) {
		name = format_uid_raw((struct sof_uuid *)uid_ptr, 0, 0);
		uid_addr = uid_ptr - (uintptr_t)uids_dict -
			    uids_dict->data_offset + uids_dict->base_address;
		fprintf(out_fd, "\t0x%lX  %s\n", uid_addr, name);

		if (name) {
			free(name);
			name = NULL;
		}
		/* just after sof_uuid there is name_len in uint32_t format */
		name_len_ptr = (uint32_t *)(uid_ptr + sizeof(struct sof_uuid));
		entry_size = ALIGN_UP(*name_len_ptr + sizeof(struct sof_uuid) +
				      sizeof(uint32_t), 4);
		remaining -= entry_size;
		uid_ptr += entry_size;
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

	count = fread(&snd, sizeof(snd), 1, config->ldc_fd);
	if (!count) {
		log_err(config->out_fd, "Error while reading %s.\n",
			config->ldc_file);
		return -ferror(config->ldc_fd);
	}

	if (strncmp((char *) snd.sig, SND_SOF_LOGS_SIG, SND_SOF_LOGS_SIG_SIZE)) {
		log_err(config->out_fd,
			"Invalid ldc file signature.\n");
		return -EINVAL;
	}

	ret = verify_fw_ver(config, &snd);
	if (ret)
		return ret;

	/* default logger and ldc_file abi verification */
	if (SOF_ABI_VERSION_INCOMPATIBLE(SOF_ABI_DBG_VERSION,
					 snd.version.abi_version)) {
		log_err(config->out_fd,
			"abi version in %s file does not coincide with abi version used by logger.\n",
			config->ldc_file);
		log_err(config->out_fd, "logger ABI Version is %d:%d:%d\n",
			SOF_ABI_VERSION_MAJOR(SOF_ABI_DBG_VERSION),
			SOF_ABI_VERSION_MINOR(SOF_ABI_DBG_VERSION),
			SOF_ABI_VERSION_PATCH(SOF_ABI_DBG_VERSION));
		log_err(config->out_fd, "ldc_file ABI Version is %d:%d:%d\n",
			SOF_ABI_VERSION_MAJOR(snd.version.abi_version),
			SOF_ABI_VERSION_MINOR(snd.version.abi_version),
			SOF_ABI_VERSION_PATCH(snd.version.abi_version));
		return -EINVAL;
	}

	/* read uuid section header */
	fseek(config->ldc_fd, snd.data_offset + snd.data_length, SEEK_SET);
	count = fread(&uids_hdr, sizeof(uids_hdr), 1, config->ldc_fd);
	if (!count) {
		log_err(config->out_fd,
			"Error while reading uuids header from %s.\n",
			config->ldc_file);
		return -ferror(config->ldc_fd);
	}
	if (strncmp((char *)uids_hdr.sig, SND_SOF_UIDS_SIG,
		    SND_SOF_UIDS_SIG_SIZE)) {
		log_err(config->out_fd,
			"invalid uuid section signature.\n");
		return -EINVAL;
	}
	config->uids_dict = calloc(1, sizeof(uids_hdr) + uids_hdr.data_length);
	if (!config->uids_dict) {
		log_err(config->out_fd,
			"failed to alloc memory for uuids.\n");
		return -ENOMEM;
	}
	memcpy(config->uids_dict, &uids_hdr, sizeof(uids_hdr));
	count = fread(config->uids_dict + 1, uids_hdr.data_length, 1,
		      config->ldc_fd);
	if (!count) {
		log_err(config->out_fd,
			"failed to read uuid section data.\n");
		return -ferror(config->ldc_fd);
	}

	if (config->dump_ldc)
		return dump_ldc_info(config, &snd);

	return logger_read(config, &snd);
}
