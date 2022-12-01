// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include "nhlt.h"
#include "dmic.h"
#include "ssp.h"

#define ITEMS_PER_LINE 8
#define MAX_NHLT_LEN 20000

static void print_blob_as_bytes(uint8_t *blob, uint32_t len)
{
	int lines = len / ITEMS_PER_LINE;
	int remind = len % ITEMS_PER_LINE;
	int i, j;

	printf("blob as bytes:\n");

	for (i = 0; i < lines; i++) {
		for (j = 0; j < ITEMS_PER_LINE; j++) {
			printf("0x%02x,", *blob);
			blob++;
		}
		printf("\n");
	}

	for (i = 0; i < remind; i++) {
		printf("0x%02x,", *blob);
		blob++;
	}
	printf("\n\n");
}

static void print_blob_as_integers(uint32_t *blob, uint32_t len_t)
{
	uint32_t len = len_t / 4;
	int lines = len / ITEMS_PER_LINE;
	int remind = len % ITEMS_PER_LINE;
	int i, j;

	printf("blob as integers:\n");

	for (i = 0; i < lines; i++) {
		for (j = 0; j < ITEMS_PER_LINE; j++) {
			printf("0x%08x,", *blob);
			blob++;
		}
		printf("\n");
	}

	for (i = 0; i < remind; i++) {
		printf("0x%08x,", *blob);
		blob++;
	}
	printf("\n\n");
}

static void print_format_header(struct nhlt_fmt_cfg *fmt_cfg)
{
	printf("fmt_tag %u\n", fmt_cfg->fmt_ext.fmt.fmt_tag);
	printf("channels %u\n", fmt_cfg->fmt_ext.fmt.channels);
	printf("samples_per_sec %u\n", fmt_cfg->fmt_ext.fmt.samples_per_sec);
	printf("avg_bytes_per_sec %u\n", fmt_cfg->fmt_ext.fmt.avg_bytes_per_sec);
	printf("block_align %u\n", fmt_cfg->fmt_ext.fmt.block_align);
	printf("bits_per_sample %u\n", fmt_cfg->fmt_ext.fmt.bits_per_sample);
	printf("cb_size %u\n", fmt_cfg->fmt_ext.fmt.cb_size);
	printf("valid_bits_per_sample %u\n", fmt_cfg->fmt_ext.sample.valid_bits_per_sample);
	printf("channel_mask %u\n\n", fmt_cfg->fmt_ext.channel_mask);
}

static int get_blobs_from_nhlt(uint8_t *src, int dmic_hw_ver)
{
	struct nhlt_acpi_table *top = (struct nhlt_acpi_table *)src;
	uint8_t *byte_p = src + sizeof(struct nhlt_acpi_table);
	struct nhlt_endpoint *ep;
	struct nhlt_specific_cfg cfg;
	struct nhlt_fmt *formats_config;
	struct nhlt_fmt_cfg *fmt_cfg;
	uint8_t *dst;
	uint32_t len;
	int i, j;

	printf("get_blobs_from_nhlt endpoint_count %u\n\n", top->endpoint_count);

	for (i = 0; i < top->endpoint_count; i++) {
		ep = (struct nhlt_endpoint *)byte_p;
		switch (ep->linktype) {
		case NHLT_LINK_DMIC:
			cfg = ep->config;
			/*jump over possible dmic array description */
			byte_p += sizeof(struct nhlt_endpoint) + cfg.size;
			formats_config = (struct nhlt_fmt *)byte_p;
			byte_p += sizeof(struct nhlt_fmt);

			printf("***********************************\n");
			printf("dmic endpoint found with %u formats\n\n",
			       formats_config->fmt_count);
			for (j = 0; j < formats_config->fmt_count; j++) {
				fmt_cfg = (struct nhlt_fmt_cfg *)byte_p;
				print_format_header(fmt_cfg);
				/* finally we should be at dmic vendor blob */
				cfg = fmt_cfg->config;
				byte_p += sizeof(struct nhlt_fmt_cfg);
				dst = byte_p;
				len = cfg.size;
				byte_p += cfg.size;
				printf("found dmic blob length %u\n\n", len);
				print_blob_as_bytes(dst, len);
				print_blob_as_integers((uint32_t *)dst, len);
				print_dmic_blob_decode(dst, len, dmic_hw_ver);
			}
			break;
		case NHLT_LINK_SSP:
			cfg = ep->config;
			byte_p += sizeof(struct nhlt_endpoint) + cfg.size;
			formats_config = (struct nhlt_fmt *)byte_p;
			byte_p += sizeof(struct nhlt_fmt);

			printf("**********************************\n");
			printf("ssp endpoint found with %u formats\n\n", formats_config->fmt_count);
			for (j = 0; j < formats_config->fmt_count; j++) {
				fmt_cfg = (struct nhlt_fmt_cfg *)byte_p;

				print_format_header(fmt_cfg);

				/* finally we should be at ssp vendor blob */
				cfg = fmt_cfg->config;
				byte_p += sizeof(struct nhlt_fmt_cfg);
				dst = byte_p;
				len = cfg.size;
				byte_p += cfg.size;
				printf("found ssp blob length %u\n\n", len);
				print_blob_as_bytes(dst, len);
				print_blob_as_integers((uint32_t *)dst, len);
				print_ssp_blob_decode(dst, len);
			}
			break;
		default:
			printf("found unknown blob linktype %u length %u\n\n", ep->linktype, len);
			byte_p += ep->length;
			break;
		}
	}

	return 0;
}

static void usage(char *name)
{
	fprintf(stdout, "Usage:\t %s [-i <input nhlt binary>] [-d <dmic hw version>]\n", name);
}

int main(int argc, char *argv[])
{
	uint8_t nhlt[MAX_NHLT_LEN];
	char *input_file = NULL;
	int dmic_hw_ver = 1;
	uint8_t *nhlt_p;
	FILE *fp;
	int opt;
	int len;
	int i;

	while ((opt = getopt(argc, argv, "hi:d:")) != -1) {
		switch (opt) {
		case 'i':
			input_file = optarg;
			break;
		case 'd':
			dmic_hw_ver = atoi(optarg);
			break;
		case 'h':
		/* pass through */
		default:
			usage(argv[0]);
			return -1;
		}
	}

	if (!input_file) {
		usage(argv[0]);
		return -1;
	}

	fp = fopen(input_file, "rb");
	if (!fp) {
		fprintf(stderr, "Can't open input file %s\n", input_file);
		return -1;
	}

	for (i = 0; i < MAX_NHLT_LEN; i++) {
		if (!fread(&nhlt[i], 1, 1, fp))
			break;
	}

	len = i;
	nhlt_p = &nhlt[0];

	printf("read %d bytes from blob, dmic hw ver %d\n\n", len, dmic_hw_ver);

	get_blobs_from_nhlt(nhlt_p, dmic_hw_ver);

	return 0;
}
