/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <alsa/asoundlib.h>

#define SOF_CTRL_CMD_BINARY 3 /* TODO: From uapi ipc */

static void usage(char *name)
{
	fprintf(stdout, "Usage %s <option(s)>\n", name);
	fprintf(stdout, "Set example %s -Dhw:0 ", name);
	fprintf(stdout, "-c \"numid=22,name=\\\"EQIIR1.0 EQIIR\\\"\" -s ");
	fprintf(stdout, "iir.txt\n");
	fprintf(stdout, "Set example %s -Dhw:0 -n 22 -s iir.txt\n", name);
	fprintf(stdout, "Get example %s -Dhw:0 -n 22\n", name);
	fprintf(stdout, "%s:\t \t\tControl SOF equalizers\n", name);
	fprintf(stdout, "%s:\t -D <dev>\tUse device <dev>, defaults to hw:0\n",
		name);
	fprintf(stdout, "%s:\t -c <name>\tGet configuration for EQ <name>\n",
		name);
	fprintf(stdout, "%s:\t -n <number>\tGet configuration for ", name);
	fprintf(stdout, "given numid\n");
	fprintf(stdout, "%s:\t -s <file>\tSetup equalizer with data", name);
	fprintf(stdout, "in <file>.\n");
	fprintf(stdout, "\t\t\t\tThe ASCII text file must contain comma\n");
	fprintf(stdout, "\t\t\t\tseparated unsigned integers.\n");
	exit(0);
}

static int read_setup(unsigned int *data, char setup[], size_t smax)
{
	FILE *fh;
	int n = 0;
	int n_max = smax / sizeof(unsigned int);
	int separator;

	/* open input file */
	fh = fopen(setup, "r");
	if (!fh) {
		fprintf(stderr, "error: %s\n", strerror(errno));
		return -errno;
	}

	while (fscanf(fh, "%u", &data[n]) != EOF && n < n_max) {
		if (n > 0)
			fprintf(stdout, ",");
		fprintf(stdout, "%u", data[n]);
		separator = fgetc(fh);
		while (separator != ',' && separator != EOF)
			separator = fgetc(fh);

		n++;
	}
	fclose(fh);
	fprintf(stdout, "\n");
	return n;
}

int main(int argc, char *argv[])
{
	snd_ctl_t *ctl;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *value;
	uint32_t *config;
	unsigned int *user_data;
	char nname[256];
	int ret;
	int ctrl_size;
	int read;
	int write;
	int type;
	int i;
	char opt;
	int n;
	int buffer_size;
	int mode = SND_CTL_NONBLOCK;
	char *dev = "hw:0";
	char *cname = NULL;
	char *setup = NULL;
	int set = 0;

	while ((opt = getopt(argc, argv, "hD:c:s:n:")) != -1) {
		switch (opt) {
		case 'D':
			dev = optarg;
			break;
		case 'c':
			cname = optarg;
			break;
		case 'n':
			sprintf(nname, "numid=%d", atoi(optarg));
			cname = nname;
			break;
		case 's':
			setup = optarg;
			set = 1;
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
	if (!cname) {
		fprintf(stderr, "Error: No control was requested.\n");
		usage(argv[0]);
		exit(EXIT_FAILURE);

	}

	/* Open the device, mixer control and get read/write/type properties.
	 */
	ret = snd_ctl_open(&ctl, dev, mode);
	if (ret) {
		fprintf(stderr, "Error: Could not open device %s.\n", dev);
		exit(ret);
	}

	/* Allocate buffers for pointers info, id, and value. */
	snd_ctl_elem_info_alloca(&info);
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_alloca(&value);

	/* Get handle id for the ascii control name. */
	ret = snd_ctl_ascii_elem_id_parse(id, cname);
	if (ret) {
		fprintf(stderr, "Error: Can't find %s.\n", cname);
		exit(ret);
	}

	/* Get handle info from id. */
	snd_ctl_elem_info_set_id(info, id);
	ret = snd_ctl_elem_info(ctl, info);
	if (ret) {
		fprintf(stderr, "Error: Could not get elem info.\n");
		exit(ret);
	}

	/* Get control attributes from info. */
	ctrl_size = snd_ctl_elem_info_get_count(info);
	read = snd_ctl_elem_info_is_tlv_readable(info);
	write = snd_ctl_elem_info_is_tlv_writable(info);
	type = snd_ctl_elem_info_get_type(info);
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
	buffer_size = ctrl_size + 2 * sizeof(unsigned int);
	user_data = calloc(1, buffer_size);
	user_data[0] = SOF_CTRL_CMD_BINARY;
	if (set) {
		fprintf(stdout, "Applying configuration \"%s\" ", setup);
		fprintf(stdout, "into device %s control %s.\n", dev, cname);
		n = read_setup(&user_data[2], setup, ctrl_size);
		if (n < 1) {
			fprintf(stderr, "Error: failed data read from %s.\n",
				setup);
			free(user_data);
			exit(EXIT_FAILURE);
		}
		user_data[1] = n * sizeof(unsigned int);
		ret = snd_ctl_elem_tlv_write(ctl, id, user_data);
		if (ret) {
			fprintf(stderr, "Error: failed TLV write.\n");
			free(user_data);
			exit(ret);
		}
		fprintf(stdout, "Success.\n");

	} else {
		fprintf(stdout, "Retrieving configuration for ");
		fprintf(stdout, "device %s control %s.\n", dev, cname);
		user_data[1] = ctrl_size;
		ret = snd_ctl_elem_tlv_read(ctl, id,
			user_data, buffer_size);
		if (ret) {
			fprintf(stderr, "Error: failed TLV read.\n");
			free(user_data);
			exit(ret);
		}
		fprintf(stdout, "Success.\n");

		/* Print the read EQ configuration data with similar syntax
		 * as the input file format.
		 */
		config = (uint32_t *) (user_data + 2);
		n = config[0] / sizeof(uint32_t);
		for (i = 0; i < n; i++) {
			if (i == n - 1)
				fprintf(stdout, "%u\n", config[i]);
			else
				fprintf(stdout, "%u,", config[i]);
		}
	}
	free(user_data);
}
