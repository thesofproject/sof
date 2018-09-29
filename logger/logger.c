#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sof/uapi/logging.h>

#define CEIL(a, b) ((a+b-1)/b)

/* elf signature params */
#define SND_SOF_LOGS_SIG_SIZE	4
#define SND_SOF_LOGS_SIG	"Logs"

struct snd_sof_logs_header {
	/* "Logs" */
	unsigned char sig[SND_SOF_LOGS_SIG_SIZE];
	/* address of log entries section */
	uint32_t base_address;
	/* amount of bytes following this header */
	uint32_t data_length;
	/* offset to first entry in this file */
	uint32_t data_offset;
};

struct ldc_entry_header {			
	uint32_t level;
	uint32_t component_id;
	uint32_t params_num;
	uint32_t line_idx;
	uint32_t file_name_len;	
};

struct ldc_entry {
	struct ldc_entry_header header;
	char *file_name;
	uint32_t text_len;
	char *text;
	uint32_t *params;
}; 

struct dma_log {
	struct log_entry_header header;
	uint32_t address;
};

static int fetch_entry(FILE *f_ldc, FILE *f_in, uint32_t base_address,
	uint32_t data_offset, struct dma_log dma_log);
static void print_table_header(void);
static void print_entry_params(struct dma_log dma_log, struct ldc_entry);
static void usage(char *name);

static inline void print_table_header(void)
{
	fprintf(stdout, "%10s %8s %8s %14s %16s %24s\t%s\n", 
		"ADDRESS", 
		"CORE_ID", 
		"LEVEL", 
		"COMPONENT_ID", 
		"TIMESTAMP", 
		"FILE_NAME", 
		"CONTENT");
}
							   			   
static void print_entry_params(struct dma_log dma_log, 
	struct ldc_entry entry)
{
	fprintf(stdout, "%10x %8u %8u %14u %16lu %20s:%u\t",
		dma_log.address,
		dma_log.header.core_id,
		entry.header.level,
		entry.header.component_id,
		dma_log.header.timestamp,
		entry.file_name,
		entry.header.line_idx);
	
	switch (entry.header.params_num){
	case 0:
		fprintf(stdout, "%s", entry.text);
		break;
	case 1:
		fprintf(stdout, entry.text, entry.params[0]);
		break;
	case 2:
		fprintf(stdout, entry.text, entry.params[0],
			entry.params[1]);
		break;	
	case 3:
		fprintf(stdout, entry.text, entry.params[0], entry.params[1],
			entry.params[2]);
		break;
	}
	fprintf(stdout, "\n");
}

static void usage(char *name)
{
	fprintf(stdout, "Usage %s <option(s)> <file(s)>\n", name);
	fprintf(stdout, "%s:\t \t\t\tParse traces logs\n", name);
	fprintf(stdout, "%s:\t -l *.ldc_file\t-i in_file\n", name);
	fprintf(stdout, "%s:\t -t\t\t\tDisplay dma trace data\n", name);
	exit(0);
}


static int fetch_entry(FILE *f_ldc, FILE *f_in, uint32_t base_address,
	uint32_t data_offset, struct dma_log dma_log)
{

	struct ldc_entry entry;
	long int padding;

	uint32_t entry_offset;
	uint32_t text_len;

	int ret;

	entry.file_name = NULL;
	entry.text = NULL;
	entry.params = NULL;
	
	/* evaluate entry offset in input file */
	entry_offset = dma_log.address - base_address;
	
	/* set file position to beginning of processed entry */
	fseek(f_ldc, entry_offset + data_offset, SEEK_SET);
	
	/* fetching elf header params */
	ret = fread(&entry.header, sizeof(entry.header), 1, f_ldc);
	if (!ret) {
		ret = -ferror(f_ldc);
		goto out;
	}
	
	entry.file_name = (char *) malloc(entry.header.file_name_len);
	if (!entry.file_name){
		fprintf(stderr, "error: can't allocate %d byte for "
			"entry.file_name\n", entry.header.file_name_len);
		ret = -ENOMEM;
		goto out;
	}
	
	ret = fread(entry.file_name, sizeof(char), entry.header.file_name_len,
		f_ldc);
	if (ret != entry.header.file_name_len) {
		ret = -ferror(f_ldc);
		goto out;
	}

	/* padding - sequences of chars are aligned to DWORDS */
	fseek(f_ldc, CEIL(entry.header.file_name_len, sizeof(uint32_t)) *
		sizeof(uint32_t) - entry.header.file_name_len, SEEK_CUR);
	
	/* fetching text length */
	ret = fread(&entry.text_len, sizeof(entry.text_len), 1, f_ldc);
	if (!ret) {
		ret = -ferror(f_ldc);
		goto out;
	}
	
	/* fetching text */
	entry.text = (char *) malloc(entry.text_len);
	if (entry.text == NULL) {
		fprintf(stderr, "error: can't allocate %d byte for "
			"entry.text\n", entry.text_len);
		ret = -ENOMEM;
		goto out;
	}
	
	ret = fread(entry.text, sizeof(char), entry.text_len, f_ldc);
	if (ret != entry.text_len) {
		ret = -ferror(f_ldc);
		goto out;
	}
	
	/* fetching entry params from dma dump */
	entry.params = (uint32_t *) malloc(sizeof(uint32_t) *
		entry.header.params_num);
	ret = fread(entry.params, sizeof(uint32_t), entry.header.params_num, 
		f_in);
	if (ret != entry.header.params_num) {
		ret = -ferror(f_in);
		goto out;
	}
	
	/* printing entry content */
	print_entry_params(dma_log, entry);
	
	/* set f_ldc file position to the beginning */
	rewind(f_ldc);
	
	ret = 0;
out:
	/* free alocated memory */
	free(entry.params);
	free(entry.text);
	free(entry.file_name);

	return ret;
}

static int logger_read(const char *in_file, FILE *f_ldc, struct snd_sof_logs_header *snd)
{
	struct dma_log dma_log;
	FILE *f_in = NULL;
	int ret = 0;

	f_in = fopen(in_file, "r");

	if (f_in == NULL) {
		fprintf(stderr, "Error while opening %s. \n", in_file);
		ret = errno;
		goto out;
	}

	print_table_header();

	while (!feof(f_in)) {

		/* getting entry parameters from dma dump */
		ret = fread(&dma_log, sizeof(dma_log), 1, f_in);
		if (!ret) {
			ret = -ferror(f_in);
			goto out;
		}

		/* checking log address */
		if ((dma_log.address < snd->base_address) ||
			(dma_log.address > (snd->base_address + snd->data_length)))
			continue;

		/* fetching entry from elf dump*/
		ret = fetch_entry(f_ldc, f_in, snd->base_address,
			snd->data_offset, dma_log);
		if (ret)
			break;
	}

out:
	if (f_ldc) fclose(f_ldc);
	if (f_in) fclose(f_in);

	return ret;

}

int main(int argc, char *argv[])
{
	struct snd_sof_logs_header snd;
	const char *ldc_file = NULL;
	const char *in_file = NULL;
	int opt, trace = 0, ret = 0;
	FILE *f_ldc = NULL;
	
	while ((opt = getopt(argc, argv, "l:i:th")) != -1) {
		switch (opt) {
		case 'l':
			ldc_file = optarg;
			break;
		case 'i':
			in_file = optarg;
			break;
		case 't':
			trace = 1;
			break;
		case 'h':
		default:
			usage(argv[0]);
		}
	}

	if (!ldc_file) {
		fprintf(stderr, "error: invalid ldc file.\n");
		usage(argv[0]);
		return -EINVAL;
	}

	f_ldc = fopen(ldc_file, "r");

	if (f_ldc == NULL) {
		fprintf(stderr, "Error while opening %s. \n", ldc_file);
		ret = errno;
		goto out;
	}

	/* set file positions to the beginning */
	rewind(f_ldc);

	/* veryfing ldc signature */
	ret = fread(&snd, sizeof(snd), 1, f_ldc);
	if (!ret) {
		ret = -ferror(f_ldc);
		goto out;
	}

	if (strncmp(snd.sig, SND_SOF_LOGS_SIG, SND_SOF_LOGS_SIG_SIZE)) {
		fprintf(stderr, "Error: Invalid ldc file signature. \n");
		ret = -EINVAL;
		goto out;
	}

	/* dma trace requested */
	if (trace)
		return logger_read("/sys/kernel/debug/sof/trace", f_ldc, &snd);

	/* default option with no infile is to dump errors/debug data */
	if (!in_file)
		return logger_read("/sys/kernel/debug/sof/etrace", f_ldc, &snd);

	return logger_read(in_file, f_ldc, &snd);

out:
	if (f_ldc) fclose(f_ldc);

	return ret;
}
