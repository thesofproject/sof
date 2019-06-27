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
	fprintf(stdout, "\t %s -h\n", name);
	fprintf(stdout, "\nWhere:\n");
	fprintf(stdout, " -D device name (default is hw:0)\n");
	fprintf(stdout, " -c control name e.g.");
	fprintf(stdout, " numid=22,name=\\\"EQIIR1.0 EQIIR\\\"\"\n");
	fprintf(stdout, " -n control id e.g. 22\n");
	fprintf(stdout, " -s set data using ASCII CSV input file\n");
	fprintf(stdout, " -b input file is in binary mode\n");
}

static int read_setup(struct ctl_data *ctl_data)
{
	FILE *fh;
	unsigned int x;
	int n = 0;
	int n_max = ctl_data->ctrl_size / sizeof(unsigned int);
	char *mode = ctl_data->binary ? "rb" : "r";
	int separator;

	/* open input file */
	fh = fdopen(ctl_data->in_fd, mode);
	if (!fh) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		return -errno;
	}

	if (ctl_data->binary) {
		n = fread(&ctl_data->buffer[2], sizeof(int), n_max, fh);
		goto read_done;
	}

	while (fscanf(fh, "%u", &x) != EOF) {
		if (n < n_max)
			ctl_data->buffer[2 + n] = x;

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
		(struct sof_abi_hdr *)&ctl_data->buffer[2];

	fprintf(stdout, "hdr: magic 0x%8.8x\n", hdr->magic);
	fprintf(stdout, "hdr: type %d\n", hdr->type);
	fprintf(stdout, "hdr: size %d bytes\n", hdr->size);
	fprintf(stdout, "hdr: abi %d:%d:%d\n",
		SOF_ABI_VERSION_MAJOR(hdr->abi),
		SOF_ABI_VERSION_MINOR(hdr->abi),
		SOF_ABI_VERSION_PATCH(hdr->abi));
}

/* dump binary data out with CSV txt format */
static void csv_data_dump(struct ctl_data *ctl_data)
{
	uint32_t *config;
	int n;
	int i;

	config = &ctl_data->buffer[2];
	n = ctl_data->buffer[1] / sizeof(uint32_t);

	/* Print out in CSV txt formal */
	for (i = 0; i < n; i++) {
		if (i == n - 1)
			fprintf(stdout, "%u\n", config[i]);
		else
			fprintf(stdout, "%u,", config[i]);
	}
}

/*
 * Print the read kcontrol configuration data with either
 * 16bit Hex binary format or ASCII CSV format.
 */
static void data_dump(struct ctl_data *ctl_data)
{
	csv_data_dump(ctl_data);
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

int main(int argc, char *argv[])
{
	char nname[256];
	int ret;
	int read;
	int write;
	int type;
	char opt;
	char *input_file = NULL;
	struct ctl_data *ctl_data;
	int n;

	ctl_data = calloc(1, sizeof(struct ctl_data));
	if (!ctl_data) {
		fprintf(stderr,
			"Error: Failed to allocate buffer for ctl_data\n");
		return -ENOMEM;
	}

	ctl_data->dev = "hw:0";

	while ((opt = getopt(argc, argv, "hD:c:s:n:b")) != -1) {
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
		case 'b':
			ctl_data->binary = true;
			break;
		case 'h':
			usage(argv[0]);
			exit(0);
			break;
		default:
			usage(argv[0]);
			exit(EXIT_FAILURE);
		}
	}

	/* The control need to be defined. */
	if (!ctl_data->cname) {
		fprintf(stderr, "Error: No control was requested.\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);

	}

	/* open input file */
	if (input_file) {
		ctl_data->in_fd = open(input_file, O_RDONLY);
		if (ctl_data->in_fd <= 0) {
			fprintf(stderr, "error: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* Open the mixer control and get read/write/type properties. */
	ret = snd_ctl_open(&ctl_data->ctl, ctl_data->dev, SND_CTL_NONBLOCK);
	if (ret) {
		fprintf(stderr, "Error: Could not open device %s.\n",
			ctl_data->dev);
		exit(ret);
	}

	/* Allocate buffers for pointers info, id, and value. */
	snd_ctl_elem_info_alloca(&ctl_data->info);
	snd_ctl_elem_id_alloca(&ctl_data->id);
	snd_ctl_elem_value_alloca(&ctl_data->value);

	/* Get handle id for the ascii control name. */
	ret = snd_ctl_ascii_elem_id_parse(ctl_data->id, ctl_data->cname);
	if (ret) {
		fprintf(stderr, "Error: Can't find %s.\n", ctl_data->cname);
		exit(ret);
	}

	/* Get handle info from id. */
	snd_ctl_elem_info_set_id(ctl_data->info, ctl_data->id);
	ret = snd_ctl_elem_info(ctl_data->ctl, ctl_data->info);
	if (ret) {
		fprintf(stderr, "Error: Could not get elem info.\n");
		exit(ret);
	}

	if (ctl_data->binary && ctl_data->set) {
		/* set ctrl_size to file size */
		ctl_data->ctrl_size = get_file_size(ctl_data->in_fd);
		if (ctl_data->ctrl_size <= 0) {
			fprintf(stderr, "Error: Input file unavailable.\n");
			exit(EXIT_FAILURE);
		}
	} else {
		/* Get control attributes from info. */
		ctl_data->ctrl_size =
			snd_ctl_elem_info_get_count(ctl_data->info);
	}

	fprintf(stderr, "Control size is %d.\n", ctl_data->ctrl_size);
	read = snd_ctl_elem_info_is_tlv_readable(ctl_data->info);
	write = snd_ctl_elem_info_is_tlv_writable(ctl_data->info);
	type = snd_ctl_elem_info_get_type(ctl_data->info);
	if (!read) {
		fprintf(stderr, "Error: No read capability.\n");
		exit(EXIT_FAILURE);
	}
	if (!write) {
		fprintf(stderr, "Error: No write capability.\n");
		exit(EXIT_FAILURE);
	}
	if (type != SND_CTL_ELEM_TYPE_BYTES) {
		fprintf(stderr, "Error: control type has no bytes support.\n");
		exit(EXIT_FAILURE);
	}

	/* Next allocate buffer for tlv write/read. The buffer needs a two
	 * words header with tag (SOF_CTRL_CMD_BINARY) and size in bytes.
	 */
	ctl_data->buffer_size = ctl_data->ctrl_size +
				2 * sizeof(unsigned int);
	ctl_data->buffer = calloc(1, ctl_data->buffer_size);
	if (!ctl_data->buffer) {
		fprintf(stderr,
			"Error: Failed to allocate buffer for user data.\n");
		exit(EXIT_FAILURE);
	}

	ctl_data->buffer[0] = SOF_CTRL_CMD_BINARY;
	if (ctl_data->set) {
		fprintf(stdout, "Applying configuration \"%s\" ",
			ctl_data->input_file);
		fprintf(stdout, "into device %s control %s.\n",
			ctl_data->dev, ctl_data->cname);
		n = read_setup(ctl_data);
		if (n < 1) {
			fprintf(stderr, "Error: failed data read from %s.\n",
				ctl_data->input_file);
			free(ctl_data->buffer);
			exit(EXIT_FAILURE);
		}

		header_dump(ctl_data);

		ctl_data->buffer[1] = n * sizeof(unsigned int);
		ret = snd_ctl_elem_tlv_write(ctl_data->ctl, ctl_data->id,
					     ctl_data->buffer);
		if (ret) {
			fprintf(stderr, "Error: failed TLV write (%d).\n", ret);
			free(ctl_data->buffer);
			exit(ret);
		}
		fprintf(stdout, "Success.\n");

	} else {
		fprintf(stdout, "Retrieving configuration for ");
		fprintf(stdout, "device %s control %s.\n",
			ctl_data->dev, ctl_data->cname);
		ctl_data->buffer[1] = ctl_data->ctrl_size;
		ret = snd_ctl_elem_tlv_read(ctl_data->ctl, ctl_data->id,
					    ctl_data->buffer,
					    ctl_data->buffer_size);
		if (ret) {
			fprintf(stderr, "Error: failed TLV read.\n");
			free(ctl_data->buffer);
			exit(ret);
		}
		fprintf(stdout, "Success.\n");

		header_dump(ctl_data);

		data_dump(ctl_data);
	}
	free(ctl_data->buffer);
	return 0;
}
