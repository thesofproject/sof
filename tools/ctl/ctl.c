// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include "kernel/abi.h"
#include "kernel/header.h"
#include "ipc/stream.h"
#include "ipc/control.h"

#define BUFFER_TAG_OFFSET	0
#define BUFFER_SIZE_OFFSET	1
#define BUFFER_ABI_OFFSET	2

struct ctl_data {
	/* the input file name */
	char *input_file;

	/* the input file descriptor */
	int in_fd;
	/* the output file descriptor */
	int out_fd;

	/* cached buffer for input/output */
	unsigned int *buffer;
	int buffer_size;
	int ctrl_size;

	/* flag for input/output format, binary or CSV */
	bool binary;
	/* flag for input/output format, with or without abi header */
	bool no_abi;
	/* component specific type, default 0 */
	uint32_t type;
	/* set or get control value */
	bool set;
	/* print ABI header */
	bool print_abi_header;
	int print_abi_size;

	/* name of sound card device */
	char *dev;
	char *cname;

	/* alsa ctl_elem pointers */
	snd_ctl_t *ctl;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *value;
};

static void usage(char *name)
{
	fprintf(stdout, "Usage:\t %s [-D <device>] [-c <control name>]", name);
	fprintf(stdout, " [-s <data>]\n");
	fprintf(stdout, "\t %s [-D <device>] [-n <control id>]", name);
	fprintf(stdout, " [-s <data>]\n");
	fprintf(stdout, "\t %s -g <size>\n", name);
	fprintf(stdout, "\t %s -h\n", name);
	fprintf(stdout, "\nWhere:\n");
	fprintf(stdout, " -D device name (default is hw:0)\n");
	fprintf(stdout, " -g <size> generates");
	fprintf(stdout, " the current ABI header with given payload size\n");
	fprintf(stdout, " -c control name e.g.");
	fprintf(stdout, " numid=22,name=\\\"EQIIR1.0 EQIIR\\\"\"\n");
	fprintf(stdout, " -n control id e.g. 22\n");
	fprintf(stdout, " -s set data using ASCII CSV input file\n");
	fprintf(stdout, " -b set/get control in binary mode(e.g. for set, use binary input file, for get, dump out in hex format)\n");
	fprintf(stdout, " -r no abi header for the input file, or not dumping abi header for get.\n");
	fprintf(stdout, " -o specify the output file.\n");
	fprintf(stdout, " -t specify the component specified type.\n");
}

static void header_init(struct ctl_data *ctl_data)
{
	struct sof_abi_hdr *hdr =
		(struct sof_abi_hdr *)&ctl_data->buffer[BUFFER_ABI_OFFSET];

	hdr->magic = SOF_ABI_MAGIC;
	hdr->type = ctl_data->type;
	hdr->abi = SOF_ABI_VERSION;
}

static int read_setup(struct ctl_data *ctl_data)
{
	struct sof_abi_hdr *hdr =
		(struct sof_abi_hdr *)&ctl_data->buffer[BUFFER_ABI_OFFSET];
	int n_max = ctl_data->ctrl_size / sizeof(unsigned int);
	char *mode = ctl_data->binary ? "rb" : "r";
	int abi_size = 0;
	unsigned int x;
	int separator;
	int n = 0;
	FILE *fh;

	/* open input file */
	fh = fdopen(ctl_data->in_fd, mode);
	if (!fh) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		return -errno;
	}

	/* create abi header*/
	if (ctl_data->no_abi) {
		header_init(ctl_data);
		abi_size = sizeof(struct sof_abi_hdr) / sizeof(int);
	}

	if (ctl_data->binary) {
		n = fread(&ctl_data->buffer[BUFFER_ABI_OFFSET + abi_size],
			  sizeof(int), n_max - abi_size, fh);

		goto read_done;
	}

	/* reading for ASCII CSV txt */
	while (fscanf(fh, "%u", &x) != EOF) {
		if (n < n_max)
			ctl_data->buffer[BUFFER_ABI_OFFSET + abi_size + n] = x;

		if (n > 0)
			fprintf(stdout, ",");

		fprintf(stdout, "%u", x);
		separator = fgetc(fh);
		while (separator != ',' && separator != EOF)
			separator = fgetc(fh);

		n++;
	}

	fprintf(stdout, "\n");

read_done:
	if (ctl_data->no_abi) {
		hdr->size = n * sizeof(int);
		n += abi_size;
	}

	if (n > n_max) {
		fprintf(stderr, "Warning: Read of %d exceeded control size. ",
			4 * n);
		fprintf(stderr, "Please check the data file.\n");
	}

	fclose(fh);
	return n;
}

static void header_dump(struct ctl_data *ctl_data)
{
	struct sof_abi_hdr *hdr =
		(struct sof_abi_hdr *)&ctl_data->buffer[BUFFER_ABI_OFFSET];

	fprintf(stdout, "hdr: magic 0x%8.8x\n", hdr->magic);
	fprintf(stdout, "hdr: type %d\n", hdr->type);
	fprintf(stdout, "hdr: size %d bytes\n", hdr->size);
	fprintf(stdout, "hdr: abi %d:%d:%d\n",
		SOF_ABI_VERSION_MAJOR(hdr->abi),
		SOF_ABI_VERSION_MINOR(hdr->abi),
		SOF_ABI_VERSION_PATCH(hdr->abi));
}

/* dump binary data out with 16bit hex format */
static void hex_data_dump(struct ctl_data *ctl_data)
{
	unsigned int int_offset;
	uint16_t *config;
	int n;
	int i;

	/* calculate the dumping units */
	n = ctl_data->buffer[BUFFER_SIZE_OFFSET] / sizeof(uint16_t);

	/* exclude the type and size header */
	int_offset = 2;

	/* exclude abi header if '-r' specified */
	if (ctl_data->no_abi) {
		int_offset += sizeof(struct sof_abi_hdr) /
			      sizeof(uint32_t);
		n -= sizeof(struct sof_abi_hdr) /
		     sizeof(uint16_t);
	}

	/* get the dumping start address */
	config = (uint16_t *)&ctl_data->buffer[int_offset];

	/* Print out in 16bit hex format */
	for (i = 0; i < n; i++) {
		if (!(i % 8))
			fprintf(stdout, "%08lx ",
				i * sizeof(uint16_t));
		fprintf(stdout, "%04x ", config[i]);
		if ((i % 8) == 7)
			fprintf(stdout, "\n");
	}
	fprintf(stdout, "\n");
}

/* dump binary data out with CSV txt format */
static void csv_data_dump(struct ctl_data *ctl_data, FILE *fh)
{
	uint32_t *config;
	int n;
	int i;
	int s = 0;

	config = &ctl_data->buffer[BUFFER_ABI_OFFSET];
	n = ctl_data->buffer[BUFFER_SIZE_OFFSET] / sizeof(uint32_t);

	if (ctl_data->no_abi)
		s = sizeof(struct sof_abi_hdr) / sizeof(uint32_t);

	/* Print out in CSV txt formal */
	for (i = s; i < n; i++) {
		if (i == n - 1)
			fprintf(fh, "%u\n", config[i]);
		else
			fprintf(fh, "%u,", config[i]);
	}
}

/*
 * Print the read kcontrol configuration data with either
 * 16bit Hex binary format or ASCII CSV format.
 */
static void data_dump(struct ctl_data *ctl_data)
{
	if (ctl_data->binary)
		hex_data_dump(ctl_data);
	else
		csv_data_dump(ctl_data, stdout);
}

static int get_file_size(int fd)
{
	struct stat st;
	int ret;

	if (fstat(fd, &st) == -1)
		ret = -EINVAL;
	else
		ret = st.st_size;

	return ret;
}

static int buffer_alloc(struct ctl_data *ctl_data)
{
	int buffer_size;

	/*
	 * Allocate buffer for tlv write/read. The buffer needs a two
	 * words header with tag (SOF_CTRL_CMD_BINARY) and size in bytes.
	 */
	buffer_size = ctl_data->ctrl_size + 2 * sizeof(unsigned int);
	ctl_data->buffer = calloc(1, buffer_size);
	if (!ctl_data->buffer) {
		fprintf(stderr,
			"Error: Failed to allocate buffer for user data.\n");
		return -EINVAL;
	}

	ctl_data->buffer[BUFFER_TAG_OFFSET] = SOF_CTRL_CMD_BINARY;

	ctl_data->buffer_size = buffer_size;

	return 0;
}

static void buffer_free(struct ctl_data *ctl_data)
{
	free(ctl_data->buffer);
	ctl_data->buffer_size = 0;
}

static int ctl_setup(struct ctl_data *ctl_data)
{
	int mode = SND_CTL_NONBLOCK;
	int ctrl_size;
	int buffer_size;
	int read;
	int write;
	int type;
	char opt;
	int ret;

	/* Open the device, mixer control and get read/write/type properties.
	 */
	ret = snd_ctl_open(&ctl_data->ctl, ctl_data->dev, mode);
	if (ret) {
		fprintf(stderr, "Error: Could not open device %s.\n",
			ctl_data->dev);
		return ret;
	}

	/* Allocate buffers for pointers info, id, and value. */
	ret = snd_ctl_elem_info_malloc(&ctl_data->info);
	if (ret) {
		fprintf(stderr, "Error: Could not malloc elem_info!\n");
		goto ctl_close;
	}
	ret = snd_ctl_elem_id_malloc(&ctl_data->id);
	if (ret) {
		fprintf(stderr, "Error: Could not malloc elem_id!\n");
		goto info_free;
	}
	ret = snd_ctl_elem_value_malloc(&ctl_data->value);
	if (ret) {
		fprintf(stderr, "Error: Could not malloc elem_value!\n");
		goto id_free;
	}

	/* Get handle id for the ascii control name. */
	ret = snd_ctl_ascii_elem_id_parse(ctl_data->id, ctl_data->cname);
	if (ret) {
		fprintf(stderr, "Error: Can't find %s.\n", ctl_data->cname);
		goto value_free;
	}

	/* Get handle info from id. */
	snd_ctl_elem_info_set_id(ctl_data->info, ctl_data->id);
	ret = snd_ctl_elem_info(ctl_data->ctl, ctl_data->info);
	if (ret) {
		fprintf(stderr, "Error: Could not get elem info.\n");
		goto value_free;
	}

	if (ctl_data->binary && ctl_data->set) {
		/* set ctrl_size to file size */
		ctrl_size = get_file_size(ctl_data->in_fd);
		if (ctrl_size <= 0) {
			fprintf(stderr, "Error: Input file unavailable.\n");
			goto value_free;
		}

		/* need more space for raw data file(no header in the file) */
		if (ctl_data->no_abi)
			ctrl_size += sizeof(struct sof_abi_hdr);
	} else {
		/* Get control attributes from info. */
		ctrl_size = snd_ctl_elem_info_get_count(ctl_data->info);
	}

	fprintf(stderr, "Control size is %d.\n", ctrl_size);
	read = snd_ctl_elem_info_is_tlv_readable(ctl_data->info);
	write = snd_ctl_elem_info_is_tlv_writable(ctl_data->info);
	type = snd_ctl_elem_info_get_type(ctl_data->info);

	if (!read && !write) {
		fprintf(stderr, "Error: Not a read/write control\n");
		goto value_free;
	}
	if (type != SND_CTL_ELEM_TYPE_BYTES) {
		fprintf(stderr, "Error: control type has no bytes support.\n");
		goto value_free;
	}

	ctl_data->ctrl_size = ctrl_size;

	/* allocate buffer for tlv data */
	ret = buffer_alloc(ctl_data);
	if (ret < 0) {
		fprintf(stderr, "Error: Could not allocate buffer, ret:%d\n",
			ret);
		goto value_free;
	}

	return ret;
buff_free:
	buffer_free(ctl_data);

value_free:
	snd_ctl_elem_value_free(ctl_data->value);
id_free:
	snd_ctl_elem_id_free(ctl_data->id);
info_free:
	snd_ctl_elem_info_free(ctl_data->info);
ctl_close:
	ret |= snd_ctl_close(ctl_data->ctl);
	return ret;
}

static int ctl_free(struct ctl_data *ctl_data)
{
	int ret;

	buffer_free(ctl_data);

	snd_ctl_elem_value_free(ctl_data->value);
	snd_ctl_elem_id_free(ctl_data->id);
	snd_ctl_elem_info_free(ctl_data->info);

	ret = snd_ctl_close(ctl_data->ctl);

	return ret;
}

static void ctl_dump(struct ctl_data *ctl_data)
{
	FILE *fh;
	int offset = 0;
	size_t n;/* in bytes */

	if (ctl_data->out_fd > 0) {
		if (ctl_data->binary) {
			/* output ctl_data(exclude the header)to file */
			/* open input file */
			fh = fdopen(ctl_data->out_fd, "wb");
			if (!fh) {
				fprintf(stderr, "error: %s\n", strerror(errno));
				return;
			}

			offset = BUFFER_ABI_OFFSET;
			n = ctl_data->buffer[BUFFER_SIZE_OFFSET];

			if (ctl_data->no_abi) {
				offset += sizeof(struct sof_abi_hdr) /
					  sizeof(int);
				n -= sizeof(struct sof_abi_hdr);
			}
			n = fwrite(&ctl_data->buffer[offset],
				   1, n, fh);
		} else {
			fh = fdopen(ctl_data->out_fd, "w");
			if (!fh) {
				fprintf(stderr, "error: %s\n", strerror(errno));
				return;
			}
			csv_data_dump(ctl_data, fh);
		}

		fprintf(stdout, "%ld bytes written to file.\n", n);
		fclose(fh);
	} else {
		/* dump to stdout */
		header_dump(ctl_data);
		data_dump(ctl_data);
	}
}

static int ctl_set_get(struct ctl_data *ctl_data)
{
	int ret;
	size_t n;

	if (!ctl_data->buffer) {
		fprintf(stderr, "Error: No buffer for set/get!\n");
		return -EINVAL;
	}

	if (ctl_data->set) {
		fprintf(stdout, "Applying configuration \"%s\" ",
			ctl_data->input_file);
		fprintf(stdout, "into device %s control %s.\n",
			ctl_data->dev, ctl_data->cname);
		n = read_setup(ctl_data);
		if (n < 1) {
			fprintf(stderr, "Error: failed data read from %s.\n",
				ctl_data->input_file);
			return -EINVAL;
		}

		ctl_data->buffer[BUFFER_SIZE_OFFSET] = n * sizeof(unsigned int);
		ret = snd_ctl_elem_tlv_write(ctl_data->ctl, ctl_data->id,
					     ctl_data->buffer);
		if (ret < 0) {
			fprintf(stderr, "Error: failed TLV write (%d)\n", ret);
			return ret;
		}
		fprintf(stdout, "Success.\n");

	} else {
		fprintf(stdout, "Retrieving configuration for ");
		fprintf(stdout, "device %s control %s.\n",
			ctl_data->dev, ctl_data->cname);
		ctl_data->buffer[BUFFER_SIZE_OFFSET] = ctl_data->ctrl_size;
		ret = snd_ctl_elem_tlv_read(ctl_data->ctl, ctl_data->id,
					    ctl_data->buffer,
					    ctl_data->buffer_size);
		if (ret < 0) {
			fprintf(stderr, "Error: failed TLV read.\n");
			return ret;
		}
		fprintf(stdout, "Success.\n");
	}
}

int main(int argc, char *argv[])
{
	char *input_file = NULL;
	char *output_file = NULL;
	struct ctl_data *ctl_data;
	char nname[256];
	int ret = 0;
	int n = 0;
	int read;
	int write;
	int type;
	char opt;
	struct sof_abi_hdr *hdr;

	ctl_data = calloc(1, sizeof(struct ctl_data));
	if (!ctl_data) {
		fprintf(stderr,
			"Error: Failed to allocate buffer for ctl_data\n");
		return -ENOMEM;
	}

	ctl_data->dev = "hw:0";

	while ((opt = getopt(argc, argv, "hD:c:s:n:o:t:g:br")) != -1) {
		switch (opt) {
		case 'D':
			ctl_data->dev = optarg;
			break;
		case 'c':
			ctl_data->cname = optarg;
			break;
		case 'n':
			sprintf(nname, "numid=%d", atoi(optarg));
			ctl_data->cname = nname;
			break;
		case 's':
			input_file = optarg;
			ctl_data->input_file = input_file;
			ctl_data->set = true;
			break;
		case 'o':
			output_file = optarg;
			break;
		case 'b':
			ctl_data->binary = true;
			break;
		case 'r':
			ctl_data->no_abi = true;
			break;
		case 't':
			ctl_data->type = atoi(optarg);
			break;
		case 'g':
			ctl_data->print_abi_header = true;
			ctl_data->print_abi_size = atoi(optarg);
			break;
		case 'h':
		/* pass through */
		default:
			usage(argv[0]);
			goto struct_free;
		}
	}

	/* open output file */
	if (output_file) {
		ctl_data->out_fd = open(output_file, O_CREAT | O_TRUNC | O_RDWR,
					0600);
		if (ctl_data->out_fd <= 0) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			goto struct_free;
		}
	}

	/* Just print the ABI header if requested */
	if (ctl_data->print_abi_header) {
		ctl_data->ctrl_size = sizeof(struct sof_abi_hdr);
		buffer_alloc(ctl_data);
		header_init(ctl_data);
		hdr = (struct sof_abi_hdr *)
			&ctl_data->buffer[BUFFER_ABI_OFFSET];
		hdr->size = ctl_data->print_abi_size;
		ctl_data->buffer[BUFFER_SIZE_OFFSET] = ctl_data->ctrl_size;
		ctl_dump(ctl_data);
		buffer_free(ctl_data);
		goto out_fd_close;
	}

	/* The control need to be defined. */
	if (!ctl_data->cname) {
		fprintf(stderr, "Error: No control was requested.\n");
		usage(argv[0]);
		goto out_fd_close;

	}

	/* open input file */
	if (input_file) {
		ctl_data->in_fd = open(input_file, O_RDONLY);
		if (ctl_data->in_fd <= 0) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			goto out_fd_close;
		}
	}

	/* set up ctl_elem, allocate buffers */
	ret = ctl_setup(ctl_data);
	if (ret < 0) {
		fprintf(stderr, "Error: ctl_data setup failed, ret:%d", ret);
		goto in_fd_close;
	}

	/* set/get the tlv bytes kcontrol */
	ret = ctl_set_get(ctl_data);
	if (ret < 0) {
		fprintf(stderr, "Error: Could not %s control, ret:%d\n",
			ctl_data->set ? "set" : "get", ret);
		goto data_free;
	}

	/* dump the tlv buffer to a file or stdout */
	ctl_dump(ctl_data);

data_free:
	ret = ctl_free(ctl_data);

in_fd_close:
	if (ctl_data->out_fd)
		close(ctl_data->out_fd);

out_fd_close:
	if (ctl_data->in_fd)
		close(ctl_data->in_fd);

struct_free:
	free(ctl_data);

	return ret;
}
