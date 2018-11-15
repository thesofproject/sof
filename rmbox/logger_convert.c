/*
 * debug log converter, using new logger format.
 *
 * Copyright (c) 2018, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <sof/uapi/ipc.h>
#include "convert.h"

#define CEIL(a, b) ((a+b-1)/b)

#define TRACE_MAX_PARAMS_COUNT		4
#define TRACE_MAX_TEXT_LEN		1024
#define TRACE_MAX_FILENAME_LEN		128
#define TRACE_MAX_IDS_STR		10
#define TRACE_IDS_MASK			((1 << TRACE_ID_LENGTH) - 1)

/* logs file signature */
#define SND_SOF_LOGS_SIG_SIZE	4
#define SND_SOF_LOGS_SIG	"Logs"

/*
* Logs dictionary file header.
*/
struct snd_sof_logs_header {
	unsigned char sig[SND_SOF_LOGS_SIG_SIZE]; /* "Logs" */
	uint32_t base_address;  /* address of log entries section */
	uint32_t data_length;   /* amount of bytes following this header */
	uint32_t data_offset;   /* offset to first entry in this file */
	struct sof_ipc_fw_version version;
};

struct ldc_entry_header {
	uint32_t level;
	uint32_t component_class;
	uint32_t has_ids;
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

static inline void print_table_header(FILE *out_fd)
{
	fprintf(out_fd, "%5s %6s %12s %7s %16s %16s %24s\t%s\n",
		"CORE",
		"LEVEL",
		"COMP_ID",
		"",
		"TIMESTAMP",
		"DELTA",
		"FILE_NAME",
		"CONTENT");
	fflush(out_fd);
}

#define CASE(x) \
	case(TRACE_CLASS_##x): return #x

static const char * get_component_name(uint32_t component_id) {
	switch (component_id) {
		CASE(IRQ);
		CASE(IPC);
		CASE(PIPE);
		CASE(HOST);
		CASE(DAI);
		CASE(DMA);
		CASE(SSP);
		CASE(COMP);
		CASE(WAIT);
		CASE(LOCK);
		CASE(MEM);
		CASE(MIXER);
		CASE(BUFFER);
		CASE(VOLUME);
		CASE(SWITCH);
		CASE(MUX);
		CASE(SRC);
		CASE(TONE);
		CASE(EQ_FIR);
		CASE(EQ_IIR);
		CASE(SA);
		CASE(DMIC);
		CASE(POWER);
	default: return "unknown";
	}
}

static void print_entry_params(FILE *out_fd, struct log_entry_header dma_log,
	struct ldc_entry entry, uint64_t last_timestamp, double clock)
{	
	char ids[TRACE_MAX_IDS_STR];
	float dt = to_usecs(dma_log.timestamp - last_timestamp, clock);

	if (dt < 0 || dt > 1000.0 * 1000.0 * 1000.0)
		dt = NAN;
	
	if (entry.header.has_ids)
		sprintf(ids, "%d.%d", (dma_log.id_0 & TRACE_IDS_MASK),
		        (dma_log.id_1 & TRACE_IDS_MASK));

	fprintf(out_fd, "%s%5u %6u %12s %-7s %16.6f %16.6f %20s:%-4u\t",
		entry.header.level == LOG_LEVEL_CRITICAL ? KRED : KNRM,
		dma_log.core_id,
		entry.header.level,
		get_component_name(entry.header.component_class),
		entry.header.has_ids ? ids : "",
		to_usecs(dma_log.timestamp, clock),
		dt,
		entry.file_name,
		entry.header.line_idx);

	switch (entry.header.params_num) {
	case 0:
		fprintf(out_fd, "%s", entry.text);
		break;
	case 1:
		fprintf(out_fd, entry.text, entry.params[0]);
		break;
	case 2:
		fprintf(out_fd, entry.text, entry.params[0], entry.params[1]);
		break;
	case 3:
		fprintf(out_fd, entry.text, entry.params[0], entry.params[1],
			entry.params[2]);
		break;
	case 4:
		fprintf(out_fd, entry.text, entry.params[0], entry.params[1],
			entry.params[2], entry.params[3]);
		break;
	}
	fprintf(out_fd, "%s\n", KNRM);
	fflush(out_fd);
}

static int fetch_entry(struct convert_config *config, uint32_t base_address,
	uint32_t data_offset, struct log_entry_header dma_log, uint64_t *last_timestamp)
{
	struct ldc_entry entry;
	uint32_t entry_offset;

	int ret;

	entry.file_name = NULL;
	entry.text = NULL;
	entry.params = NULL;

	/* evaluate entry offset in input file */
	entry_offset = dma_log.log_entry_address - base_address;

	/* set file position to beginning of processed entry */
	fseek(config->ldc_fd, entry_offset + data_offset, SEEK_SET);

	/* fetching elf header params */
	ret = fread(&entry.header, sizeof(entry.header), 1, config->ldc_fd);
	if (!ret) {
		ret = -ferror(config->ldc_fd);
		goto out;
	}
	if (entry.header.file_name_len > TRACE_MAX_FILENAME_LEN) {
		fprintf(stderr, "Error: Invalid filename length. \n");
		ret = -EINVAL;
		goto out;
	}
	entry.file_name = (char *)malloc(entry.header.file_name_len);

	if (!entry.file_name) {
		fprintf(stderr, "error: can't allocate %d byte for "
			"entry.file_name\n", entry.header.file_name_len);
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
		fprintf(stderr, "Error: Invalid text length. \n");
		ret = -EINVAL;
		goto out;
	}
	entry.text = (char *)malloc(entry.header.text_len);
	if (entry.text == NULL) {
		fprintf(stderr, "error: can't allocate %d byte for "
			"entry.text\n", entry.header.text_len);
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
		fprintf(stderr, "Error: Invalid number of parameters. \n");
		ret = -EINVAL;
		goto out;
	}
	entry.params = (uint32_t *)malloc(sizeof(uint32_t) *
		entry.header.params_num);
	if (entry.params == NULL) {
		fprintf(stderr, "error: can't allocate %d byte for "
			"entry.params\n", (int)(sizeof(uint32_t) *
			entry.header.params_num));
		ret = -ENOMEM;
		goto out;
	}
	ret = fread(entry.params, sizeof(uint32_t), entry.header.params_num,
		config->in_fd);
	if (ret != entry.header.params_num) {
		ret = -ferror(config->in_fd);
		goto out;
	}

	/* printing entry content */
	print_entry_params(config->out_fd, dma_log, entry, *last_timestamp,
		config->clock);
	*last_timestamp = dma_log.timestamp;

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

static int logger_read(struct convert_config *config,
	struct snd_sof_logs_header *snd)
{
	struct log_entry_header dma_log;
	int ret = 0;
	print_table_header(config->out_fd);
	uint64_t last_timestamp = 0;

	while (!feof(config->in_fd)) {
		/* getting entry parameters from dma dump */
		ret = fread(&dma_log, sizeof(dma_log), 1, config->in_fd);
		if (!ret) {
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
			dma_log, &last_timestamp);
		if (ret)
			break;
	}

	return ret;
}

int convert(struct convert_config *config) {
	struct snd_sof_logs_header snd;
	int count, ret = 0;

	count = fread(&snd, sizeof(snd), 1, config->ldc_fd);
	if (!count) {
		fprintf(stderr, "Error while reading %s. \n", config->ldc_file);
		return -ferror(config->ldc_fd);
	}

	if (strncmp((char *) snd.sig, SND_SOF_LOGS_SIG, SND_SOF_LOGS_SIG_SIZE)) {
		fprintf(stderr, "Error: Invalid ldc file signature. \n");
		return -EINVAL;
	}

	/* fw verification */
	if (config->version_fd) {
		struct sof_ipc_fw_version ver;

		/* here fw verification should be exploited */
		count = fread(&ver, sizeof(ver), 1, config->version_fd);
		if (!count) {
			fprintf(stderr, "Error while reading %s. \n", config->version_file);
			return -ferror(config->version_fd);
		}

		ret = memcmp(&ver, &snd.version, sizeof(struct sof_ipc_fw_version));
		if (ret) {
			fprintf(stderr, "Error: fw version in %s file "
				"does not coincide with fw version in "
				"%s file. \n", config->ldc_file, config->version_file);
			return -EINVAL;
		}
	}

	return logger_read(config, &snd);
}
