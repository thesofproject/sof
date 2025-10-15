// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>
//         Jyri Sarha <jyri.sarha@intel.com> (restructured and moved to this file)

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ipc/probe_dma_frame.h>

#include "wave.h"

#define APP_NAME "sof-probes"

#define PACKET_MAX_SIZE	4096	/**< Size limit for probe data packet */
#define DATA_READ_LIMIT 4096	/**< Data limit for file read */
#define FILES_LIMIT	32	/**< Maximum num of probe output files */
#define FILE_PATH_LIMIT 128	/**< Path limit for probe output files */

struct wave_files {
	FILE *fd;
	uint32_t buffer_id;
	uint32_t fmt;
	uint32_t size;
	struct wave header;
};

enum p_state {
	READY = 0,		/**< At this stage app is looking for a SYNC word */
	SYNC,			/**< SYNC received, copying data */
	CHECK			/**< Check crc and save packet if valid */
};

struct dma_frame_parser {
	bool log_to_stdout;
	enum p_state state;
	struct probe_data_packet *packet;
	size_t packet_size;
	uint8_t *w_ptr;				/* Write pointer to copy data to */
	uint32_t total_data_to_copy;		/* Total bytes left to copy */
	int start;				/* Start of unfilled data */
	int len;				/* Data buffer fill level */
	uint8_t data[DATA_READ_LIMIT];
	struct wave_files files[FILES_LIMIT];
};

static uint32_t sample_rate[] = {
	8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100,
	48000, 64000, 88200, 96000, 128000, 176400, 192000
};

int get_buffer_file(struct wave_files *files, uint32_t buffer_id)
{
	int i;

	for (i = 0; i < FILES_LIMIT; i++) {
		if (files[i].fd != NULL && files[i].buffer_id == buffer_id)
			return i;
	}
	return -1;
}

int get_buffer_file_free(struct wave_files *files)
{
	int i;

	for (i = 0; i < FILES_LIMIT; i++) {
		if (files[i].fd == NULL)
			return i;
	}
	return -1;
}

bool is_audio_format(uint32_t format)
{
	return (format & PROBE_MASK_FMT_TYPE) != 0 && (format & PROBE_MASK_AUDIO_FMT) == 0;
}

int init_wave(struct dma_frame_parser *p, uint32_t buffer_id, uint32_t format)
{
	bool audio = is_audio_format(format);
	char path[FILE_PATH_LIMIT];
	int i;

	i = get_buffer_file_free(p->files);
	if (i == -1) {
		fprintf(stderr, "error: too many buffers\n");
		exit(0);
	}

	sprintf(path, "buffer_%d.%s", buffer_id, audio ? "wav" : "bin");

	fprintf(stderr, "%s:\t Creating file %s\n", APP_NAME, path);

	if (!audio && p->log_to_stdout) {
		p->files[i].fd = stdout;
	} else {
		p->files[i].fd = fopen(path, "wb");
		if (!p->files[i].fd) {
			fprintf(stderr, "error: unable to create file %s, error %d\n",
				path, errno);
			exit(0);
		}
	}

	p->files[i].buffer_id = buffer_id;
	p->files[i].fmt = format;

	if (!audio)
		return i;

	p->files[i].header.riff.chunk_id = HEADER_RIFF;
	p->files[i].header.riff.format = HEADER_WAVE;
	p->files[i].header.fmt.subchunk_id = HEADER_FMT;
	p->files[i].header.fmt.subchunk_size = 16;
	p->files[i].header.fmt.audio_format = 1;
	p->files[i].header.fmt.num_channels = ((format & PROBE_MASK_NB_CHANNELS) >> PROBE_SHIFT_NB_CHANNELS) + 1;
	p->files[i].header.fmt.sample_rate = sample_rate[(format & PROBE_MASK_SAMPLE_RATE) >> PROBE_SHIFT_SAMPLE_RATE];
	p->files[i].header.fmt.bits_per_sample = (((format & PROBE_MASK_CONTAINER_SIZE) >> PROBE_SHIFT_CONTAINER_SIZE) + 1) * 8;
	p->files[i].header.fmt.byte_rate = p->files[i].header.fmt.sample_rate *
					p->files[i].header.fmt.num_channels *
					p->files[i].header.fmt.bits_per_sample / 8;
	p->files[i].header.fmt.block_align = p->files[i].header.fmt.num_channels *
					  p->files[i].header.fmt.bits_per_sample / 8;
	p->files[i].header.data.subchunk_id = HEADER_DATA;

	fwrite(&p->files[i].header, sizeof(struct wave), 1, p->files[i].fd);

	return i;
}

void finalize_wave_files(struct dma_frame_parser *p)
{
	struct wave_files *files = p->files;
	uint32_t i, chunk_size;

	/* fill the header at the beginning of each file */
	/* and close all opened files */
	/* check wave struct to understand the offsets */
	for (i = 0; i < FILES_LIMIT; i++) {
		if (!is_audio_format(files[i].fmt))
			continue;

		if (files[i].fd) {
			chunk_size = files[i].size + sizeof(struct wave) -
				     offsetof(struct riff_chunk, format);

			fseek(files[i].fd, sizeof(uint32_t), SEEK_SET);
			fwrite(&chunk_size, sizeof(uint32_t), 1, files[i].fd);
			fseek(files[i].fd, sizeof(struct wave) -
			      offsetof(struct data_subchunk, subchunk_size),
			      SEEK_SET);
			fwrite(&files[i].size, sizeof(uint32_t), 1, files[i].fd);

			fclose(files[i].fd);
		}
	}
}

int validate_data_packet(struct probe_data_packet *packet)
{
	uint64_t *checksump;
	uint64_t sum;

	sum = (uint32_t) (packet->sync_word +
			  packet->buffer_id  +
			  packet->format +
			  packet->timestamp_high +
			  packet->timestamp_low +
			  packet->data_size_bytes);

	checksump = (uint64_t *) (packet->data + packet->data_size_bytes);

	if (sum != *checksump) {
		fprintf(stderr, "Checksum error 0x%016" PRIx64 " != 0x%016" PRIx64 "\n",
			sum, *checksump);
		return -EINVAL;
	}

	return 0;
}

int process_sync(struct dma_frame_parser *p)
{
	struct probe_data_packet *temp_packet;

	/* request to copy data_size from probe packet and 64-bit checksum */
	p->total_data_to_copy = p->packet->data_size_bytes + sizeof(uint64_t);

	if (sizeof(struct probe_data_packet) + p->total_data_to_copy >
	    p->packet_size) {
		p->packet_size = sizeof(struct probe_data_packet) +
			p->total_data_to_copy;

		temp_packet = realloc(p->packet, p->packet_size);

		if (!temp_packet)
			return -ENOMEM;

		p->packet = temp_packet;
	}

	p->w_ptr = (uint8_t *)p->packet->data;

	return 0;
}

struct dma_frame_parser *parser_init(void)
{
	struct dma_frame_parser *p = malloc(sizeof(*p));
	if (!p) {
		fprintf(stderr, "error: allocation failed, err %d\n",
			errno);
		return NULL;
	}
	memset(p, 0, sizeof(*p));
	p->packet = malloc(PACKET_MAX_SIZE);
	if (!p) {
		fprintf(stderr, "error: allocation failed, err %d\n",
			errno);
		free(p);
		return NULL;
	}
	memset(p->packet, 0, PACKET_MAX_SIZE);
	p->packet_size = PACKET_MAX_SIZE;
	return p;
}

void parser_free(struct dma_frame_parser *p)
{
	free(p->packet);
	free(p);
}

void parser_log_to_stdout(struct dma_frame_parser *p)
{
	p->log_to_stdout = true;
}

void parser_fetch_free_buffer(struct dma_frame_parser *p, uint8_t **d, size_t *len)
{
	*d = &p->data[p->start];
	*len = sizeof(p->data) - p->start;
}

int parser_parse_data(struct dma_frame_parser *p, size_t d_len)
{
	uint i = 0;

	p->len = p->start + d_len;
	/* processing all loaded bytes */
	while (i < p->len) {
		if (p->total_data_to_copy == 0) {
			switch (p->state) {
			case READY:
				/* check for SYNC */
				if (p->len - i < sizeof(p->packet->sync_word)) {
					p->start = p->len - i;
					memmove(&p->data[0], &p->data[i], p->start);
					i += p->start;
				} else if (*((uint32_t *)&p->data[i]) ==
					   PROBE_EXTRACT_SYNC_WORD) {
					memset(p->packet, 0, p->packet_size);
					/* request to copy full data packet */
					p->total_data_to_copy =
						sizeof(struct probe_data_packet);
					p->w_ptr = (uint8_t *)p->packet;
					p->state = SYNC;
					p->start = 0;
				} else {
					i++;
				}
				break;
			case SYNC:
				/* SYNC -> CHECK */
				if (process_sync(p) < 0) {
					fprintf(stderr, "OOM, quitting\n");
					return -ENOMEM;
				}
				p->state = CHECK;
				break;
			case CHECK:
				/* CHECK -> READY */
				/* find corresponding file and save data if valid */
				if (validate_data_packet(p->packet) == 0) {
					int file = get_buffer_file(p->files,
								   p->packet->buffer_id);

					if (file < 0)
						file = init_wave(p, p->packet->buffer_id,
								 p->packet->format);

					if (file < 0) {
						fprintf(stderr,
							"unable to open file for %u\n",
							p->packet->buffer_id);
						return -EIO;
					}

					fwrite(p->packet->data, 1,
					       p->packet->data_size_bytes,
					       p->files[file].fd);
					p->files[file].size += p->packet->data_size_bytes;
					}
				p->state = READY;
				break;
			}
		}
		/* data copying section */
		if (p->total_data_to_copy > 0) {
			uint data_to_copy;

			/* check if there is enough bytes loaded */
			/* or copy partially if not */
			if (i + p->total_data_to_copy > p->len) {
				data_to_copy = p->len - i;
				p->total_data_to_copy -= data_to_copy;
			} else {
				data_to_copy = p->total_data_to_copy;
				p->total_data_to_copy = 0;
			}
			memcpy(p->w_ptr, &p->data[i], data_to_copy);
			p->w_ptr += data_to_copy;
			i += data_to_copy;
		}
	}
	return 0;
}
