/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 */

#include "rimage/sof/user/manifest.h"
#include "rimage/sof/user/manifest.h"
#include "rimage/adsp_config.h"
#include "rimage/ext_manifest_gen.h"
#include "rimage/plat_auth.h"
#include "rimage/manifest.h"
#include "rimage/rimage.h"
#include "rimage/cse.h"
#include "rimage/css.h"
#include "rimage/toml_utils.h"
#include "rimage/file_utils.h"
#include "toml.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

/* map memory zone string name to enum value */
static enum snd_sof_fw_blk_type zone_name_to_idx(const char *name)
{
	static const struct {
		const char name[32];
		enum snd_sof_fw_blk_type type;
	} mem_zone_name_dict[] = {
		{"START", SOF_FW_BLK_TYPE_START},
		{"IRAM", SOF_FW_BLK_TYPE_IRAM},
		{"DRAM", SOF_FW_BLK_TYPE_DRAM},
		{"SRAM", SOF_FW_BLK_TYPE_SRAM},
		{"ROM", SOF_FW_BLK_TYPE_ROM},
		{"IMR", SOF_FW_BLK_TYPE_IMR},
		{"RSRVD0", SOF_FW_BLK_TYPE_RSRVD0},
		{"HP-SRAM", SOF_FW_BLK_TYPE_HPSRAM},
		{"LP-SRAM", SOF_FW_BLK_TYPE_LPSRAM},
		{"RSRVD8", SOF_FW_BLK_TYPE_RSRVD8},
		{"RSRVD9", SOF_FW_BLK_TYPE_RSRVD9},
		{"RSRVD10", SOF_FW_BLK_TYPE_RSRVD10},
		{"RSRVD11", SOF_FW_BLK_TYPE_RSRVD11},
		{"RSRVD12", SOF_FW_BLK_TYPE_RSRVD12},
		{"RSRVD13", SOF_FW_BLK_TYPE_RSRVD13},
		{"RSRVD14", SOF_FW_BLK_TYPE_RSRVD14},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(mem_zone_name_dict); ++i) {
		if (!strcmp(name, mem_zone_name_dict[i].name))
			return mem_zone_name_dict[i].type;
	}
	return SOF_FW_BLK_TYPE_INVALID;
}

static void dump_adsp(const struct adsp *adsp)
{
	int i;

	DUMP("\nadsp");
	DUMP_KEY("name", "'%s'", adsp->name);
	DUMP_KEY("image_size", "0x%x", adsp->image_size);
	DUMP_KEY("exec_boot_ldr", "%d", adsp->exec_boot_ldr);
	for (i = 0; i < ARRAY_SIZE(adsp->mem.zones); ++i) {
		DUMP_KEY("mem_zone.idx", "%d", i);
		DUMP_KEY("mem_zone.size", "0x%x", adsp->mem.zones[i].size);
		DUMP_KEY("mem_zone.base", "0x%x", adsp->mem.zones[i].base);
		DUMP_KEY("mem_zone.host_offset", "0x%x", adsp->mem.zones[i].host_offset);
	}
}

static int parse_adsp(const toml_table_t *toml, struct parse_ctx *pctx, struct adsp *out,
		      bool verbose)
{
	toml_array_t *mem_zone_array, *alias_array;
	struct memory_zone *zone;
	struct parse_ctx ctx;
	toml_table_t *adsp;
	toml_raw_t raw;
	int zone_idx;
	char a_kind;
	int a_size;
	bool alias_found;
	int ret;
	int i;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	adsp = toml_table_in(toml, "adsp");
	if (!adsp)
		return err_key_not_found("adsp");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */

	/* configurable fields */
	raw = toml_raw_in(adsp, "name");
	if (!raw)
		return err_key_not_found("name");
	++ctx.key_cnt;

	/* free(out->name) is called in adsp_free() */
	ret = toml_rtos(raw, (char **)&out->name);
	if (ret < 0)
		return err_key_parse("name", NULL);

	out->image_size = parse_uint32_hex_key(adsp, &ctx, "image_size", 0, &ret);
	if (ret < 0)
		return ret;

	out->exec_boot_ldr = parse_uint32_key(adsp, &ctx, "exec_boot_ldr", 0, &ret);
	if (ret < 0)
		return ret;

	memset(&out->mem, 0, sizeof(out->mem));
	out->mem.alias.mask = parse_uint32_hex_key(adsp, &ctx, "alias_mask", -ENODATA, &ret);
	alias_found = !ret;

	/* check everything parsed, 1 or 2 tables should be present */
	ctx.array_cnt += 1 + alias_found;
	ret = assert_everything_parsed(adsp, &ctx);
	if (ret < 0)
		return ret;

	if (alias_found) {
		alias_array = toml_array_in(adsp, "mem_alias");
		if (!alias_array)
			return err_key_not_found("mem_alias");
		a_kind = toml_array_kind(alias_array);
		a_size = toml_array_nelem(alias_array);
		if (a_kind != 't' || a_size != 2)
			return err_key_parse("mem_alias", "wrong array type %c or length %d",
					     a_kind, a_size);

		/* retrieve "cached" and "uncached" alias base addresses */
		for (i = 0; i < a_size; ++i) {
			toml_table_t *alias = toml_table_at(alias_array, i);
			char alias_name[16];
			uint32_t base;

			if (!alias)
				return err_key_parse("mem_alias", NULL);

			parse_str_key(alias, &ctx, "type", alias_name, sizeof(alias_name), &ret);
			if (ret < 0)
				return err_key_parse("mem_alias", NULL);

			base = parse_uint32_hex_key(alias, &ctx, "base", -1, &ret);

			if (!strncmp("cached", alias_name, sizeof("cached")))
				out->mem.alias.cached = base & out->mem.alias.mask;
			else if (!strncmp("uncached", alias_name, sizeof("uncached")))
				out->mem.alias.uncached = base & out->mem.alias.mask;
		}
	} else {
		/* Make uncache_to_cache() an identity transform */
		out->mem.alias.uncached = 0;
		out->mem.alias.cached = 0;
		out->mem.alias.mask = 0;
	}

	/* look for entry array */
	mem_zone_array = toml_array_in(adsp, "mem_zone");
	if (!mem_zone_array)
		return err_key_not_found("mem_zone");
	a_kind = toml_array_kind(mem_zone_array);
	a_size = toml_array_nelem(mem_zone_array);
	if (a_kind != 't' || a_size > SOF_FW_BLK_TYPE_NUM)
		return err_key_parse("mem_zone", "wrong array type %c or length %d",
				     a_kind, a_size);

	/* parse entry array elements */
	for (i = 0; i < a_size; ++i) {
		toml_table_t *mem_zone = toml_table_at(mem_zone_array, i);
		char zone_name[32];

		if (!mem_zone)
			return err_key_parse("mem_zone", NULL);

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non-configurable fields */

		/* configurable fields */
		parse_str_key(mem_zone, &ctx, "type", zone_name, sizeof(zone_name), &ret);
		if (ret < 0)
			return err_key_parse("mem_zone", NULL);

		zone_idx = zone_name_to_idx(zone_name);
		if (zone_idx < 0)
			return err_key_parse("mem_zone.name", "unknown zone '%s'", zone_name);

		zone = &out->mem.zones[zone_idx];
		zone->base = parse_uint32_hex_key(mem_zone, &ctx, "base", -1, &ret);
		if (ret < 0)
			return err_key_parse("mem_zone", NULL);

		zone->host_offset = parse_uint32_hex_key(mem_zone, &ctx, "host_offset", 0,
							 &ret);
		if (ret < 0)
			return err_key_parse("mem_zone", NULL);

		zone->size = parse_uint32_hex_key(mem_zone, &ctx, "size", -1, &ret);
		if (ret < 0)
			return err_key_parse("mem_zone", NULL);

		/* check everything parsed */
		ret = assert_everything_parsed(mem_zone, &ctx);
		if (ret < 0)
			return ret;
	}

	if (verbose)
		dump_adsp(out);

	/*
	 * values set in other places in code:
	 * - write_firmware
	 * - write_firmware_meu
	 * - man_vX_Y
	 */

	return 0;
}

static void dump_cse(const struct CsePartitionDirHeader *cse_header,
		     const struct CsePartitionDirEntry *cse_entry)
{
	int i;

	DUMP("\ncse");
	DUMP_PRINTABLE_BYTES("partition_name", cse_header->partition_name);
	DUMP_KEY("header_version", "%d", cse_header->header_version);
	DUMP_KEY("entry_version", "%d", cse_header->entry_version);
	DUMP_KEY("nb_entries", "%d", cse_header->nb_entries);
	for (i = 0; i < cse_header->nb_entries; ++i) {
		DUMP_PRINTABLE_BYTES("entry.name", cse_entry[i].entry_name);
		DUMP_KEY("entry.offset", "0x%x", cse_entry[i].offset);
		DUMP_KEY("entry.length", "0x%x", cse_entry[i].length);
	}
}

static int parse_cse(const toml_table_t *toml, struct parse_ctx *pctx,
		     struct CsePartitionDirHeader *hdr, struct CsePartitionDirEntry *out,
		     int entry_capacity, bool verbose)
{
	toml_array_t *cse_entry_array;
	toml_table_t *cse_entry;
	struct parse_ctx ctx;
	toml_table_t *cse;
	int ret;
	int i;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	cse = toml_table_in(toml, "cse");
	if (!cse)
		return err_key_not_found("cse");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */
	hdr->header_marker = CSE_HEADER_MAKER;
	hdr->header_length = sizeof(struct CsePartitionDirHeader);

	/* configurable fields */
	hdr->header_version = parse_uint32_key(cse, &ctx, "header_version", 1, &ret);
	if (ret < 0)
		return ret;

	hdr->entry_version = parse_uint32_key(cse, &ctx, "entry_version", 1, &ret);
	if (ret < 0)
		return ret;

	parse_printable_key(cse, &ctx, "partition_name", hdr->partition_name,
			    sizeof(hdr->partition_name), &ret);
	if (ret < 0)
		return ret;

	/* check everything parsed, expect 1 table */
	ctx.array_cnt += 1;
	ret = assert_everything_parsed(cse, &ctx);
	if (ret < 0)
		return ret;

	/* entry array */
	cse_entry_array = toml_array_in(cse, "entry");
	if (!cse_entry_array)
		return err_key_not_found("entry");
	if (toml_array_kind(cse_entry_array) != 't' ||
	    toml_array_nelem(cse_entry_array) != entry_capacity)
		return err_key_parse("entry", "wrong array type or length != %d", entry_capacity);

	/* parse entry array elements */
	for (i = 0; i < toml_array_nelem(cse_entry_array); ++i) {
		cse_entry = toml_table_at(cse_entry_array, i);
		if (!cse_entry)
			return err_key_parse("entry", NULL);

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non-configurable fields */

		/* configurable fields */
		parse_printable_key(cse_entry, &ctx, "name", out[i].entry_name,
				    sizeof(out[i].entry_name), &ret);
		if (ret < 0)
			return err_key_parse("entry", NULL);

		out[i].offset = parse_uint32_hex_key(cse_entry, &ctx, "offset", -1, &ret);
		if (ret < 0)
			return err_key_parse("entry", NULL);

		out[i].length = parse_uint32_hex_key(cse_entry, &ctx, "length", -1, &ret);
		if (ret < 0)
			return err_key_parse("entry", NULL);

		/* check everything parsed */
		ret = assert_everything_parsed(cse_entry, &ctx);
		if (ret < 0)
			return ret;
	}

	hdr->nb_entries = toml_array_nelem(cse_entry_array);

	if (verbose)
		dump_cse(hdr, out);

	/*
	 * values set in other places in code:
	 * - checksum
	 */

	return 0;
}

static void dump_cse_v2_5(const struct CsePartitionDirHeader_v2_5 *cse_header,
		     const struct CsePartitionDirEntry *cse_entry)
{
	int i;

	DUMP("\ncse");
	DUMP_PRINTABLE_BYTES("partition_name", cse_header->partition_name);
	DUMP_KEY("header_version", "%d", cse_header->header_version);
	DUMP_KEY("entry_version", "%d", cse_header->entry_version);
	DUMP_KEY("nb_entries", "%d", cse_header->nb_entries);
	for (i = 0; i < cse_header->nb_entries; ++i) {
		DUMP_PRINTABLE_BYTES("entry.name", cse_entry[i].entry_name);
		DUMP_KEY("entry.offset", "0x%x", cse_entry[i].offset);
		DUMP_KEY("entry.length", "0x%x", cse_entry[i].length);
	}
}

/* TODO: fix up constants in headers for v2.5 */
static int parse_cse_v2_5(const toml_table_t *toml, struct parse_ctx *pctx,
		     struct CsePartitionDirHeader_v2_5 *hdr, struct CsePartitionDirEntry *out,
		     int entry_capacity, bool verbose)
{
	toml_array_t *cse_entry_array;
	toml_table_t *cse_entry;
	struct parse_ctx ctx;
	toml_table_t *cse;
	int ret;
	int i;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	cse = toml_table_in(toml, "cse");
	if (!cse)
		return err_key_not_found("cse");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */
	hdr->header_marker = CSE_HEADER_MAKER;
	hdr->header_length = sizeof(struct CsePartitionDirHeader_v2_5);

	/* configurable fields */
	hdr->header_version = parse_uint32_key(cse, &ctx, "header_version", 2, &ret);
	if (ret < 0)
		return ret;

	hdr->entry_version = parse_uint32_key(cse, &ctx, "entry_version", 1, &ret);
	if (ret < 0)
		return ret;

	parse_printable_key(cse, &ctx, "partition_name", hdr->partition_name,
			    sizeof(hdr->partition_name), &ret);
	if (ret < 0)
		return ret;

	/* check everything parsed, expect 1 table */
	ctx.array_cnt += 1;
	ret = assert_everything_parsed(cse, &ctx);
	if (ret < 0)
		return ret;

	/* entry array */
	cse_entry_array = toml_array_in(cse, "entry");
	if (!cse_entry_array)
		return err_key_not_found("entry");
	if (toml_array_kind(cse_entry_array) != 't' ||
	    toml_array_nelem(cse_entry_array) != entry_capacity)
		return err_key_parse("entry", "wrong array type or length != %d", entry_capacity);

	/* parse entry array elements */
	for (i = 0; i < toml_array_nelem(cse_entry_array); ++i) {
		cse_entry = toml_table_at(cse_entry_array, i);
		if (!cse_entry)
			return err_key_parse("entry", NULL);

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non-configurable fields */

		/* configurable fields */
		parse_printable_key(cse_entry, &ctx, "name", out[i].entry_name,
				    sizeof(out[i].entry_name), &ret);
		if (ret < 0)
			return err_key_parse("entry", NULL);

		out[i].offset = parse_uint32_hex_key(cse_entry, &ctx, "offset", -1, &ret);
		if (ret < 0)
			return err_key_parse("offset", NULL);

		out[i].length = parse_uint32_hex_key(cse_entry, &ctx, "length", -1, &ret);
		if (ret < 0)
			return err_key_parse("length", NULL);

		/* check everything parsed */
		ret = assert_everything_parsed(cse_entry, &ctx);
		if (ret < 0)
			return ret;
	}

	hdr->nb_entries = toml_array_nelem(cse_entry_array);

	if (verbose)
		dump_cse_v2_5(hdr, out);

	/*
	 * values set in other places in code:
	 * - checksum
	 */

	return 0;
}

static void dump_css_v1_5(const struct css_header_v1_5 *css)
{
	DUMP("\ncss 1.5");
	DUMP_KEY("module_type", "%d", css->module_type);
	DUMP_KEY("header_len", "%d", css->header_len);
	DUMP_KEY("header_version", "0x%x", css->header_version);
	DUMP_KEY("module_vendor", "0x%x", css->module_vendor);
	DUMP_KEY("size", "%d", css->size);
	DUMP_KEY("key_size", "%d", css->key_size);
	DUMP_KEY("modulus_size", "%d", css->modulus_size);
	DUMP_KEY("exponent_size", "%d", css->exponent_size);
}

static int parse_css_v1_5(const toml_table_t *toml, struct parse_ctx *pctx,
			  struct css_header_v1_5 *out, bool verbose)
{
	struct parse_ctx ctx;
	toml_table_t *css;
	int ret;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	css = toml_table_in(toml, "css");
	if (!css)
		return err_key_not_found("css");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */

	/* configurable fields */
	out->module_type = parse_uint32_key(css, &ctx, "module_type", MAN_CSS_LT_MODULE_TYPE, &ret);
	if (ret < 0)
		return ret;

	out->header_len = parse_uint32_key(css, &ctx, "header_len", MAN_CSS_HDR_SIZE, &ret);
	if (ret < 0)
		return ret;

	out->header_version = parse_uint32_hex_key(css, &ctx, "header_version", MAN_CSS_HDR_VERSION,
						   &ret);
	if (ret < 0)
		return ret;
	out->module_vendor = parse_uint32_hex_key(css, &ctx, "module_vendor", MAN_CSS_MOD_VENDOR,
						  &ret);
	if (ret < 0)
		return ret;

	out->size = parse_uint32_key(css, &ctx, "size", 0x800, &ret);
	if (ret < 0)
		return ret;

	out->key_size = parse_uint32_key(css, &ctx, "key_size", MAN_CSS_KEY_SIZE, &ret);
	if (ret < 0)
		return ret;

	out->modulus_size = parse_uint32_key(css, &ctx, "modulus_size", MAN_CSS_MOD_SIZE, &ret);
	if (ret < 0)
		return ret;

	out->exponent_size = parse_uint32_key(css, &ctx, "exponent_size", MAN_CSS_EXP_SIZE, &ret);
	if (ret < 0)
		return ret;

	/* check everything parsed */
	ret = assert_everything_parsed(css, &ctx);
	if (ret < 0)
		return ret;

	if (verbose)
		dump_css_v1_5(out);

	/*
	 * values set in other places in code:
	 * - date
	 * - version
	 * - modulus
	 * - exponent
	 * - signature
	 */

	return 0;
}

static void dump_css_v1_8(const struct css_header_v1_8 *css)
{
	DUMP("\ncss 1.8");
	DUMP_KEY("header_type", "%d", css->header_type);
	DUMP_KEY("header_len", "%d", css->header_len);
	DUMP_KEY("header_version", "0x%x", css->header_version);
	DUMP_KEY("module_vendor", "0x%x", css->module_vendor);
	DUMP_KEY("size", "%d", css->size);
	DUMP_KEY("svn", "%d", css->svn);
	DUMP_KEY("modulus_size", "%d", css->modulus_size);
	DUMP_KEY("exponent_size", "%d", css->exponent_size);
}

static int parse_css_v1_8(const toml_table_t *toml, struct parse_ctx *pctx,
			  struct css_header_v1_8 *out, bool verbose)
{
	static const uint8_t hdr_id[4] = MAN_CSS_HDR_ID;
	struct parse_ctx ctx;
	toml_table_t *css;
	int ret;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	css = toml_table_in(toml, "css");
	if (!css)
		return err_key_not_found("css");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */
	memcpy(out->header_id, hdr_id, sizeof(out->header_id));

	/* configurable fields */
	out->header_type = parse_uint32_key(css, &ctx, "header_type", MAN_CSS_MOD_TYPE, &ret);
	if (ret < 0)
		return ret;

	out->header_len = parse_uint32_key(css, &ctx, "header_len", MAN_CSS_HDR_SIZE, &ret);
	if (ret < 0)
		return ret;

	out->header_version = parse_uint32_hex_key(css, &ctx, "header_version", MAN_CSS_HDR_VERSION,
						   &ret);
	if (ret < 0)
		return ret;
	out->module_vendor = parse_uint32_hex_key(css, &ctx, "module_vendor", MAN_CSS_MOD_VENDOR,
						  &ret);
	if (ret < 0)
		return ret;

	out->size = parse_uint32_key(css, &ctx, "size", 222, &ret);
	if (ret < 0)
		return ret;

	out->svn = parse_uint32_key(css, &ctx, "svn", 0, &ret);
	if (ret < 0)
		return ret;

	out->modulus_size = parse_uint32_key(css, &ctx, "modulus_size", MAN_CSS_MOD_SIZE, &ret);
	if (ret < 0)
		return ret;

	out->exponent_size = parse_uint32_key(css, &ctx, "exponent_size", MAN_CSS_EXP_SIZE, &ret);
	if (ret < 0)
		return ret;

	/* check everything parsed */
	ret = assert_everything_parsed(css, &ctx);
	if (ret < 0)
		return ret;

	if (verbose)
		dump_css_v1_8(out);

	/*
	 * values set in other places in code:
	 * - date
	 * - version
	 * - modulus
	 * - exponent
	 * - signature
	 */

	return 0;
}

static void dump_css_v2_5(const struct css_header_v2_5 *css)
{
	DUMP("\ncss 2.5");
	DUMP_KEY("header_type", "%d", css->header_type);
	DUMP_KEY("header_len", "%d", css->header_len);
	DUMP_KEY("header_version", "0x%x", css->header_version);
	DUMP_KEY("module_vendor", "0x%x", css->module_vendor);
	DUMP_KEY("size", "%d", css->size);
	DUMP_KEY("svn", "%d", css->svn);
	DUMP_KEY("modulus_size", "%d", css->modulus_size);
	DUMP_KEY("exponent_size", "%d", css->exponent_size);
}

static int parse_css_v2_5(const toml_table_t *toml, struct parse_ctx *pctx,
			  struct css_header_v2_5 *out, bool verbose)
{
	static const uint8_t hdr_id[4] = MAN_CSS_HDR_ID;
	struct parse_ctx ctx;
	toml_table_t *css;
	int ret;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	css = toml_table_in(toml, "css");
	if (!css)
		return err_key_not_found("css");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */
	memcpy(out->header_id, hdr_id, sizeof(out->header_id));

	/* configurable fields */
	out->header_type = parse_uint32_key(css, &ctx, "header_type", MAN_CSS_MOD_TYPE, &ret);
	if (ret < 0)
		return ret;

	out->header_len = parse_uint32_key(css, &ctx, "header_len", MAN_CSS_HDR_SIZE_2_5, &ret);
	if (ret < 0)
		return ret;

	out->header_version = parse_uint32_hex_key(css, &ctx, "header_version", MAN_CSS_HDR_VERSION_2_5,
						   &ret);
	if (ret < 0)
		return ret;
	out->module_vendor = parse_uint32_hex_key(css, &ctx, "module_vendor", MAN_CSS_MOD_VENDOR,
						  &ret);
	if (ret < 0)
		return ret;

	out->size = parse_uint32_key(css, &ctx, "size", 281, &ret);
	if (ret < 0)
		return ret;

	out->svn = parse_uint32_key(css, &ctx, "svn", 0, &ret);
	if (ret < 0)
		return ret;

	out->modulus_size = parse_uint32_key(css, &ctx, "modulus_size", MAN_CSS_MOD_SIZE_2_5, &ret);
	if (ret < 0)
		return ret;

	out->exponent_size = parse_uint32_key(css, &ctx, "exponent_size", MAN_CSS_EXP_SIZE, &ret);
	if (ret < 0)
		return ret;

	/* hardcoded to align with meu */
	out->reserved1[0] = 0xf;
	out->reserved1[1] = 0x048e0000; // TODO: what is this ?

	/* check everything parsed */
	ret = assert_everything_parsed(css, &ctx);
	if (ret < 0)
		return ret;

	if (verbose)
		dump_css_v2_5(out);

	/*
	 * values set in other places in code:
	 * - date
	 * - version
	 * - modulus
	 * - exponent
	 * - signature
	 */

	return 0;
}

static void dump_signed_pkg(const struct signed_pkg_info_ext *signed_pkg)
{
	int i;

	DUMP("\nsigned_pkg");
	DUMP_PRINTABLE_BYTES("name", signed_pkg->name);
	DUMP_KEY("vcn", "%d", signed_pkg->vcn);
	DUMP_KEY("svn", "%d", signed_pkg->svn);
	DUMP_KEY("fw_type", "%d", signed_pkg->fw_type);
	DUMP_KEY("fw_sub_type", "%d", signed_pkg->fw_sub_type);
	for (i = 0; i < ARRAY_SIZE(signed_pkg->bitmap); ++i)
		DUMP_KEY("bitmap", "%d", signed_pkg->bitmap[i]);
	for (i = 0; i < ARRAY_SIZE(signed_pkg->module); ++i) {
		DUMP_PRINTABLE_BYTES("meta.name", signed_pkg->module[i].name);
		DUMP_KEY("meta.type", "0x%x", signed_pkg->module[i].type);
		DUMP_KEY("meta.hash_algo", "0x%x", signed_pkg->module[i].hash_algo);
		DUMP_KEY("meta.hash_size", "0x%x", signed_pkg->module[i].hash_size);
		DUMP_KEY("meta.meta_size", "%d", signed_pkg->module[i].meta_size);
	}
}

static int parse_signed_pkg(const toml_table_t *toml, struct parse_ctx *pctx,
			    struct image *image, bool verbose)
{
	struct adsp *adsp = image->adsp;
	struct signed_pkg_info_ext *out = &adsp->man_v1_8->signed_pkg;
	struct signed_pkg_info_module *mod;
	toml_array_t *bitmap_array;
	toml_array_t *module_array;
	toml_table_t *signed_pkg;
	struct parse_ctx ctx;
	toml_table_t *module;
	toml_raw_t raw;
	int64_t temp_i;
	int ret;
	int i;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	signed_pkg = toml_table_in(toml, "signed_pkg");
	if (!signed_pkg)
		return err_key_not_found("signed_pkg");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */
	out->ext_type = SIGN_PKG_EXT_TYPE;
	out->ext_len = sizeof(struct signed_pkg_info_ext);

	/* configurable fields */
	parse_printable_key(signed_pkg, &ctx, "name", out->name, sizeof(out->name), &ret);
	if (ret < 0)
		return ret;

	out->vcn = parse_uint32_key(signed_pkg, &ctx, "vcn", 0, &ret);
	if (ret < 0)
		return ret;

	out->svn = parse_uint32_key(signed_pkg, &ctx, "svn", 0, &ret);
	if (ret < 0)
		return ret;

	out->fw_type = parse_uint32_hex_key(signed_pkg, &ctx, "fw_type", 0, &ret);
	if (ret < 0)
		return ret;

	out->fw_sub_type = parse_uint32_hex_key(signed_pkg, &ctx, "fw_sub_type", 0, &ret);
	if (ret < 0)
		return ret;

	/* bitmap array */
	bitmap_array = toml_array_in(signed_pkg, "bitmap");
	if (!bitmap_array) {
		/* default value, depending on the IMR type */
		out->bitmap[4] = image->imr_type == 4 ? 0x10 : 0x8;
	} else {
		++ctx.array_cnt;
		if (toml_array_kind(bitmap_array) != 'v' || toml_array_type(bitmap_array) != 'i' ||
		    toml_array_nelem(bitmap_array) > ARRAY_SIZE(out->bitmap))
			return err_key_parse("bitmap", "wrong array type or length > %d",
					     ARRAY_SIZE(out->bitmap));

		for (i = 0; i < toml_array_nelem(bitmap_array); ++i) {
			raw = toml_raw_at(bitmap_array, i);
			if (!raw)
				return err_key_parse("bitmap", NULL);

			ret = toml_rtoi(raw, &temp_i);
			if (ret < 0 || temp_i < 0)
				return err_key_parse("bitmap", "values can't be negative");
			out->bitmap[i] = temp_i;
		}
	}

	/* check everything parsed, expect 1 more array */
	ctx.array_cnt += 1;
	ret = assert_everything_parsed(signed_pkg, &ctx);
	if (ret < 0)
		return ret;

	/* modules array */
	module_array = toml_array_in(signed_pkg, "module");
	if (!module_array)
		return err_key_not_found("module");
	if (toml_array_kind(module_array) != 't' ||
	    toml_array_nelem(module_array) != ARRAY_SIZE(out->module))
		return err_key_parse("module", "wrong array type or length != %d",
				     ARRAY_SIZE(out->module));

	/* parse modules array elements */
	for (i = 0; i < toml_array_nelem(module_array); ++i) {
		module = toml_table_at(module_array, i);
		if (!module)
			return err_key_parse("module", NULL);
		mod = &out->module[i];

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non-configurable fields */

		/* configurable fields */
		parse_printable_key(module, &ctx, "name", mod->name, sizeof(mod->name), &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->type = parse_uint32_hex_key(module, &ctx, "type", 0x03, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->hash_algo = parse_uint32_hex_key(module, &ctx, "hash_algo", 0x02, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->hash_size = parse_uint32_hex_key(module, &ctx, "hash_size", 0x20, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->meta_size = parse_uint32_key(module, &ctx, "meta_size", 96, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		/* check everything parsed */
		ret = assert_everything_parsed(module, &ctx);
		if (ret < 0)
			return ret;
	}

	if (verbose)
		dump_signed_pkg(out);

	/*
	 * values set in other places in code:
	 * - module.hash
	 */

	return 0;
}

static void dump_signed_pkg_v2_5(const struct signed_pkg_info_ext_v2_5 *signed_pkg)
{
	int i;

	DUMP("\nsigned_pkg");
	DUMP_PRINTABLE_BYTES("name", signed_pkg->name);
	DUMP_KEY("vcn", "%d", signed_pkg->vcn);
	DUMP_KEY("svn", "%d", signed_pkg->svn);
	DUMP_KEY("fw_type", "%d", signed_pkg->fw_type);
	DUMP_KEY("fw_sub_type", "%d", signed_pkg->fw_sub_type);
	for (i = 0; i < ARRAY_SIZE(signed_pkg->bitmap); ++i)
		DUMP_KEY("bitmap", "%d", signed_pkg->bitmap[i]);
	for (i = 0; i < ARRAY_SIZE(signed_pkg->module); ++i) {
		DUMP_PRINTABLE_BYTES("meta.name", signed_pkg->module[i].name);
		DUMP_KEY("meta.type", "0x%x", signed_pkg->module[i].type);
		DUMP_KEY("meta.hash_algo", "0x%x", signed_pkg->module[i].hash_algo);
		DUMP_KEY("meta.hash_size", "0x%x", signed_pkg->module[i].hash_size);
		DUMP_KEY("meta.meta_size", "%d", signed_pkg->module[i].meta_size);
	}
}

static int parse_signed_pkg_v2_5(const toml_table_t *toml, struct parse_ctx *pctx,
				 struct image *image, bool verbose)
{
	struct adsp *adsp = image->adsp;
	struct signed_pkg_info_ext_v2_5 *out = &adsp->man_v2_5->signed_pkg;
	struct signed_pkg_info_module_v2_5 *mod;
	toml_array_t *bitmap_array;
	toml_array_t *module_array;
	toml_table_t *signed_pkg;
	struct parse_ctx ctx;
	toml_table_t *module;
	toml_raw_t raw;
	int64_t temp_i;
	int ret;
	int i;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	signed_pkg = toml_table_in(toml, "signed_pkg");
	if (!signed_pkg)
		return err_key_not_found("signed_pkg");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */
	out->ext_type = SIGN_PKG_EXT_TYPE;
	out->ext_len = sizeof(struct signed_pkg_info_ext_v2_5);

	/* configurable fields */
	parse_printable_key(signed_pkg, &ctx, "name", out->name, sizeof(out->name), &ret);
	if (ret < 0)
		return ret;

	out->vcn = parse_uint32_key(signed_pkg, &ctx, "vcn", 0, &ret);
	if (ret < 0)
		return ret;

	out->svn = parse_uint32_key(signed_pkg, &ctx, "svn", 0, &ret);
	if (ret < 0)
		return ret;

	out->fw_type = parse_uint32_hex_key(signed_pkg, &ctx, "fw_type", 0, &ret);
	if (ret < 0)
		return ret;

	out->fw_sub_type = parse_uint32_hex_key(signed_pkg, &ctx, "fw_sub_type", 0, &ret);
	if (ret < 0)
		return ret;

	/* bitmap array */
	bitmap_array = toml_array_in(signed_pkg, "bitmap");
	if (!bitmap_array) {
		/* default value, depending on the IMR type */
		out->bitmap[4] = image->imr_type == 4 ? 0x10 : 0x8;
	} else {
		++ctx.array_cnt;
		if (toml_array_kind(bitmap_array) != 'v' || toml_array_type(bitmap_array) != 'i' ||
		    toml_array_nelem(bitmap_array) > ARRAY_SIZE(out->bitmap))
			return err_key_parse("bitmap", "wrong array type or length > %d",
					     ARRAY_SIZE(out->bitmap));

		for (i = 0; i < toml_array_nelem(bitmap_array); ++i) {
			raw = toml_raw_at(bitmap_array, i);
			if (!raw)
				return err_key_parse("bitmap", NULL);

			ret = toml_rtoi(raw, &temp_i);
			if (ret < 0 || temp_i < 0)
				return err_key_parse("bitmap", "values can't be negative");
			out->bitmap[i] = temp_i;
		}
	}

	/* check everything parsed, expect 1 more array */
	ctx.array_cnt += 1;
	ret = assert_everything_parsed(signed_pkg, &ctx);
	if (ret < 0)
		return ret;

	/* modules array */
	module_array = toml_array_in(signed_pkg, "module");
	if (!module_array)
		return err_key_not_found("module");
	if (toml_array_kind(module_array) != 't' ||
	    toml_array_nelem(module_array) != ARRAY_SIZE(out->module))
		return err_key_parse("module", "wrong array type or length != %d",
				     ARRAY_SIZE(out->module));

	/* parse modules array elements */
	for (i = 0; i < toml_array_nelem(module_array); ++i) {
		module = toml_table_at(module_array, i);
		if (!module)
			return err_key_parse("module", NULL);
		mod = &out->module[i];

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non-configurable fields */

		/* configurable fields */
		parse_printable_key(module, &ctx, "name", mod->name, sizeof(mod->name), &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->type = parse_uint32_hex_key(module, &ctx, "type", 0x03, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->hash_algo = parse_uint32_hex_key(module, &ctx, "hash_algo", 0x00, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->hash_size = parse_uint32_hex_key(module, &ctx, "hash_size", 0x30, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->meta_size = parse_uint32_key(module, &ctx, "meta_size", 112, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		/* check everything parsed */
		ret = assert_everything_parsed(module, &ctx);
		if (ret < 0)
			return ret;
	}

	if (verbose)
		dump_signed_pkg_v2_5(out);

	/*
	 * values set in other places in code:
	 * - module.hash
	 */

	return 0;
}

static void dump_signed_pkg_ace_v1_5(const struct signed_pkg_info_ext_ace_v1_5 *signed_pkg)
{
	int i;

	DUMP("\nsigned_pkg");
	DUMP_KEY("name", "'%s'", signed_pkg->name);
	DUMP_KEY("vcn", "%d", signed_pkg->vcn);
	DUMP_KEY("svn", "%d", signed_pkg->svn);
	DUMP_KEY("fw_type", "%d", signed_pkg->fw_type);
	DUMP_KEY("fw_sub_type", "%d", signed_pkg->fw_sub_type);
	for (i = 0; i < ARRAY_SIZE(signed_pkg->module); ++i) {
		DUMP_KEY("meta.name", "'%s'", signed_pkg->module[i].name);
		DUMP_KEY("meta.type", "0x%x", signed_pkg->module[i].type);
	}
}

static int parse_signed_pkg_ace_v1_5(const toml_table_t *toml, struct parse_ctx *pctx,
				     struct image *image, bool verbose)
{
	struct adsp *adsp = image->adsp;
	struct signed_pkg_info_ext_ace_v1_5 *out = &adsp->man_ace_v1_5->signed_pkg;
	struct signed_pkg_info_module_ace_v1_5 *mod;
	toml_array_t *module_array;
	toml_table_t *signed_pkg;
	struct parse_ctx ctx;
	toml_table_t *module;
	int ret;
	int i;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	signed_pkg = toml_table_in(toml, "signed_pkg");
	if (!signed_pkg)
		return err_key_not_found("signed_pkg");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */
	out->ext_type = SIGN_PKG_EXT_TYPE_ACE_V1_5;
	out->ext_len = sizeof(struct signed_pkg_info_ext_ace_v1_5);

	/* configurable fields */
	parse_printable_key(signed_pkg, &ctx, "name", out->name, sizeof(out->name), &ret);
	if (ret < 0)
		return ret;

	out->vcn = parse_uint32_key(signed_pkg, &ctx, "vcn", 0, &ret);
	if (ret < 0)
		return ret;

	out->svn = parse_uint32_key(signed_pkg, &ctx, "svn", 0, &ret);
	if (ret < 0)
		return ret;

	out->fw_type = parse_uint32_hex_key(signed_pkg, &ctx, "fw_type", 0, &ret);
	if (ret < 0)
		return ret;

	out->fw_sub_type = parse_uint32_hex_key(signed_pkg, &ctx, "fw_sub_type", 0, &ret);
	if (ret < 0)
		return ret;

	out->partition_usage = parse_uint32_hex_key(signed_pkg, &ctx, "partition_usage", 0, &ret);
	if (ret < 0)
		return ret;

	/* check everything parsed, expect 1 more array */
	ctx.array_cnt += 1;
	ret = assert_everything_parsed(signed_pkg, &ctx);
	if (ret < 0)
		return ret;

	/* modules array */
	module_array = toml_array_in(signed_pkg, "module");
	if (!module_array)
		return err_key_not_found("module");
	if (toml_array_kind(module_array) != 't' ||
	    toml_array_nelem(module_array) != ARRAY_SIZE(out->module))
		return err_key_parse("module", "wrong array type or length != %d",
				     ARRAY_SIZE(out->module));

	/* parse modules array elements */
	for (i = 0; i < toml_array_nelem(module_array); ++i) {
		module = toml_table_at(module_array, i);
		if (!module)
			return err_key_parse("module", NULL);
		mod = &out->module[i];

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non-configurable fields */

		/* configurable fields */
		parse_printable_key(module, &ctx, "name", mod->name, sizeof(mod->name), &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->type = parse_uint32_hex_key(module, &ctx, "type", 0x03, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->hash_algo = parse_uint32_hex_key(module, &ctx, "hash_algo", 0x00, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->meta_size = parse_uint32_key(module, &ctx, "meta_size", 112, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		/* check everything parsed */
		ret = assert_everything_parsed(module, &ctx);
		if (ret < 0)
			return ret;
	}

	if (verbose)
		dump_signed_pkg_ace_v1_5(out);

	/*
	 * values set in other places in code:
	 * - module.hash
	 */

	return 0;
}

static void dump_partition_info_ext(const struct partition_info_ext *part_info)
{
	int i;

	DUMP("\npartition_info");
	DUMP_PRINTABLE_BYTES("name", part_info->name);
	DUMP_KEY("part_version", "0x%x", part_info->part_version);
	DUMP_KEY("instance_id", "%d", part_info->instance_id);
	for (i = 0; i < ARRAY_SIZE(part_info->module); ++i) {
		DUMP_PRINTABLE_BYTES("module.name", part_info->module[i].name);
		DUMP_KEY("module.meta_size", "0x%x", part_info->module[i].meta_size);
		DUMP_KEY("module.type", "0x%x", part_info->module[i].type);
	}
}

static int parse_partition_info_ext(const toml_table_t *toml, struct parse_ctx *pctx,
				    struct partition_info_ext *out, bool verbose)
{
	static const uint8_t module_reserved[3] = {0x00, 0xff, 0xff};
	struct partition_info_module *mod;
	toml_table_t *partition_info;
	toml_array_t *module_array;
	toml_table_t *module;
	struct parse_ctx ctx;
	int ret;
	int i;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	partition_info = toml_table_in(toml, "partition_info");
	if (!partition_info)
		return err_key_not_found("partition_info");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non-configurable fields */
	out->ext_type = PART_INFO_EXT_TYPE;
	out->ext_len = sizeof(struct partition_info_ext);
	memset(out->reserved, 0xff, sizeof(out->reserved));

	/* configurable fields */
	parse_printable_key(partition_info, &ctx, "name", out->name, sizeof(out->name), &ret);
	if (ret < 0)
		return ret;

	out->vcn = parse_uint32_key(partition_info, &ctx, "vcn", 0, &ret);
	if (ret < 0)
		return ret;

	out->part_version =
		parse_uint32_hex_key(partition_info, &ctx, "part_version", 0x10000000, &ret);
	if (ret < 0)
		return ret;

	out->vcn = parse_uint32_hex_key(partition_info, &ctx, "part_version", 0x10000000, &ret);
	if (ret < 0)
		return ret;

	out->vcn = parse_uint32_hex_key(partition_info, &ctx, "fmt_version", 0, &ret);
	if (ret < 0)
		return ret;

	out->instance_id = parse_uint32_key(partition_info, &ctx, "instance_id", 1, &ret);
	if (ret < 0)
		return ret;

	out->part_flags = parse_uint32_key(partition_info, &ctx, "part_flags", 0, &ret);
	if (ret < 0)
		return ret;

	/* check everything parsed, expect 1 array */
	ctx.array_cnt += 1;
	ret = assert_everything_parsed(partition_info, &ctx);
	if (ret < 0)
		return ret;

	/* look for module array */
	module_array = toml_array_in(partition_info, "module");
	if (!module_array)
		return err_key_not_found("module");
	if (toml_array_kind(module_array) != 't' ||
	    toml_array_nelem(module_array) > ARRAY_SIZE(out->module))
		return err_key_parse("module", "wrong array type or length > %d",
				     ARRAY_SIZE(out->module));

	/* parse module array elements */
	for (i = 0; i < toml_array_nelem(module_array); ++i) {
		module = toml_table_at(module_array, i);
		if (!module)
			return err_key_parse("module", NULL);
		mod = &out->module[i];

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non-configurable fields */
		memcpy(mod->reserved, module_reserved, sizeof(mod->reserved));

		/* configurable fields */
		parse_printable_key(module, &ctx, "name", mod->name, sizeof(mod->name), &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->meta_size = parse_uint32_key(module, &ctx, "meta_size", 96, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		mod->type = parse_uint32_hex_key(module, &ctx, "type", 0x03, &ret);
		if (ret < 0)
			return err_key_parse("module", NULL);

		/* check everything parsed */
		ret = assert_everything_parsed(module, &ctx);
		if (ret < 0)
			return ret;
	}

	if (verbose)
		dump_partition_info_ext(out);

	/*
	 * values set in other places in code:
	 * - length
	 * - hash
	 * - module.hash
	 */

	return 0;
}

static int parse_info_ext_0x16(const toml_table_t *toml, struct parse_ctx *pctx,
				    struct info_ext_0x16 *out, bool verbose)
{
	/* known */
	out->ext_type = 0x16;
	out->ext_len = sizeof(*out);
	out->name[0] = 'A';
	out->name[1] = 'D';
	out->name[2] = 'S';
	out->name[3] = 'P';

	/* copied from meu - unknown */
	out->data[0] = 0x10000000;
	out->data[2] = 0x1;
	out->data[3] = 0x0;
	out->data[4] = 0x3003;

	return 0;
}

static void dump_adsp_file_ext_v1_8(const struct sof_man_adsp_meta_file_ext_v1_8 *adsp_file)
{
	int i;
	int j;

	DUMP("\nadsp_file_ext 1.8");
	DUMP_KEY("imr_type", "0x%x", adsp_file->imr_type);
	for (i = 0; i < ARRAY_SIZE(adsp_file->comp_desc); ++i) {
		DUMP_KEY("comp.version", "0x%x", adsp_file->comp_desc[i].version);
		DUMP_KEY("comp.base_offset", "0x%x", adsp_file->comp_desc[i].base_offset);
		for (j = 0; j < ARRAY_SIZE(adsp_file->comp_desc->attributes); ++j)
			DUMP_KEY("comp.atributes[]", "%d", adsp_file->comp_desc[i].attributes[j]);
	}
}

static int parse_adsp_file_ext_v1_8(const toml_table_t *toml, struct parse_ctx *pctx,
				    struct sof_man_adsp_meta_file_ext_v1_8 *out, bool verbose)
{
	struct sof_man_component_desc_v1_8 *desc;
	toml_array_t *attributes_array;
	toml_table_t *adsp_file_ext;
	toml_array_t *comp_array;
	struct parse_ctx ctx;
	toml_raw_t attribute;
	toml_table_t *comp;
	int64_t temp_i;
	int ret;
	int i;
	int j;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	adsp_file_ext = toml_table_in(toml, "adsp_file");
	if (!adsp_file_ext)
		return err_key_not_found("adsp_file");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non configurable flieds */
	out->ext_type = 17; /* always 17 for ADSP extension */
	out->ext_len = sizeof(struct sof_man_adsp_meta_file_ext_v1_8);

	/* configurable fields */
	out->imr_type = parse_uint32_hex_key(adsp_file_ext, &ctx, "imr_type",
					     MAN_DEFAULT_IMR_TYPE, &ret);
	if (ret < 0)
		return ret;

	/* check everything parsed, expect 1 array */
	ctx.array_cnt += 1;
	ret = assert_everything_parsed(adsp_file_ext, &ctx);
	if (ret < 0)
		return ret;

	/* parse comp array */
	comp_array = toml_array_in(adsp_file_ext, "comp");
	if (!comp_array)
		return err_key_not_found("comp");
	if (toml_array_nelem(comp_array) != 1 || toml_array_kind(comp_array) != 't')
		return err_key_parse("comp", "wrong array type or length != 1");

	/* parse comp array elements */
	for (i = 0; i < toml_array_nelem(comp_array); ++i) {
		comp = toml_table_at(comp_array, i);
		if (!comp)
			return err_key_parse("comp", NULL);
		desc = &out->comp_desc[i];

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non-configurable fields */

		/* configurable fields */
		desc->version = parse_uint32_key(comp, &ctx, "version", 0, &ret);
		if (ret < 0)
			return err_key_parse("comp", NULL);

		desc->base_offset = parse_uint32_hex_key(comp, &ctx, "base_offset",
							 MAN_DESC_OFFSET_V1_8, &ret);
		if (ret < 0)
			return err_key_parse("comp", NULL);

		/* parse attributes array */
		attributes_array = toml_array_in(comp, "attributes");
		if (attributes_array) {
			++ctx.array_cnt;
			if (toml_array_nelem(attributes_array) > ARRAY_SIZE(desc->attributes) ||
			    toml_array_kind(attributes_array) != 'v' ||
			    toml_array_type(attributes_array) != 'i')
				return err_key_parse("comp.attributes",
						     "wrong array type or length > %d",
						     ARRAY_SIZE(desc->attributes));
			for (j = 0; j < toml_array_nelem(attributes_array); ++j) {
				attribute = toml_raw_at(attributes_array, j);
				if (!attribute)
					err_key_parse("comp.attributes", NULL);
				ret = toml_rtoi(attribute, &temp_i);
				if (ret < 0 || temp_i < 0 || temp_i > UINT32_MAX)
					err_key_parse("comp.attributes", NULL);
				desc->attributes[j] = (uint32_t)temp_i;
			}
		}

		/* check everything parsed */
		ret = assert_everything_parsed(comp, &ctx);
		if (ret < 0)
			return ret;
	}

	if (verbose)
		dump_adsp_file_ext_v1_8(out);

	/*
	 * values set in other places in code:
	 * - imr_type
	 * - comp.limit_offset
	 */

	return 0;
}

static void dump_adsp_file_ext_v2_5(const struct sof_man_adsp_meta_file_ext_v2_5 *adsp_file)
{
	int i;
	int j;

	DUMP("\nadsp_file 2.5");
	DUMP_KEY("imr_type", "0x%x", adsp_file->imr_type);
	for (i = 0; i < ARRAY_SIZE(adsp_file->comp_desc); ++i) {
		DUMP_KEY("comp.version", "0x%x", adsp_file->comp_desc[i].version);
		DUMP_KEY("comp.base_offset", "0x%x", adsp_file->comp_desc[i].base_offset);
		for (j = 0; j < ARRAY_SIZE(adsp_file->comp_desc->attributes); ++j)
			DUMP_KEY("comp.atributes[]", "%d", adsp_file->comp_desc[i].attributes[j]);
	}
}

static int parse_adsp_file_ext_v2_5(const toml_table_t *toml, struct parse_ctx *pctx,
				    struct sof_man_adsp_meta_file_ext_v2_5 *out, bool verbose)
{
	struct sof_man_component_desc_v2_5 *desc;
	toml_array_t *attributes_array;
	toml_table_t *adsp_file_ext;
	toml_array_t *comp_array;
	struct parse_ctx ctx;
	toml_raw_t attribute;
	toml_table_t *comp;
	int64_t temp_i;
	int ret;
	int i;
	int j;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	adsp_file_ext = toml_table_in(toml, "adsp_file");
	if (!adsp_file_ext)
		return err_key_not_found("adsp_file");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	/* non configurable flieds */
	out->ext_type = 17; /* always 17 for ADSP extension */
	out->ext_len = sizeof(struct sof_man_adsp_meta_file_ext_v2_5);

	/* configurable fields */
	out->imr_type = parse_uint32_hex_key(adsp_file_ext, &ctx, "imr_type",
					     MAN_DEFAULT_IMR_TYPE, &ret);
	if (ret < 0)
		return ret;

	/* check everything parsed, expect 1 array */
	ctx.array_cnt += 1;
	ret = assert_everything_parsed(adsp_file_ext, &ctx);
	if (ret < 0)
		return ret;

	/* parse comp array */
	comp_array = toml_array_in(adsp_file_ext, "comp");
	if (!comp_array)
		return err_key_not_found("comp");
	if (toml_array_nelem(comp_array) != 1 || toml_array_kind(comp_array) != 't')
		return err_key_parse("comp", "wrong array type or length != 1");

	/* parse comp array elements */
	for (i = 0; i < toml_array_nelem(comp_array); ++i) {
		comp = toml_table_at(comp_array, i);
		if (!comp)
			return err_key_parse("comp", NULL);
		desc = &out->comp_desc[i];

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx);

		/* non configurable flieds */

		/* configurable fields */
		desc->version = parse_uint32_key(comp, &ctx, "version", 0, &ret);
		if (ret < 0)
			return err_key_parse("comp", NULL);

		desc->base_offset = parse_uint32_hex_key(comp, &ctx, "base_offset", 0x2000, &ret);
		if (ret < 0)
			return err_key_parse("comp", NULL);

		/* parse attributes array */
		attributes_array = toml_array_in(comp, "attributes");
		if (attributes_array) {
			++ctx.array_cnt;
			if (toml_array_nelem(attributes_array) > ARRAY_SIZE(desc->attributes) ||
			    toml_array_kind(attributes_array) != 'v' ||
			    toml_array_type(attributes_array) != 'i')
				return err_key_parse("comp.attributes",
						     "wrong array type or length > %d",
						     ARRAY_SIZE(desc->attributes));
			for (j = 0; j < toml_array_nelem(attributes_array); ++j) {
				attribute = toml_raw_at(attributes_array, j);
				if (!attribute)
					err_key_parse("comp.attributes", NULL);
				ret = toml_rtoi(attribute, &temp_i);
				if (ret < 0 || temp_i < 0 || temp_i > UINT32_MAX)
					err_key_parse("comp.attributes", NULL);
				desc->attributes[j] = (uint32_t)temp_i;
			}
		}

		/* check everything parsed */
		ret = assert_everything_parsed(comp, &ctx);
		if (ret < 0)
			return ret;
	}

	if (verbose)
		dump_adsp_file_ext_v2_5(out);

	/*
	 * values set in other places in code:
	 * - imr_type
	 * - comp.limit_offset
	 */

	return 0;
}

static void dump_fw_desc(const struct sof_man_fw_desc *fw_desc)
{
	DUMP("\nfw_desc.header");
	DUMP_KEY("header_id", "'%c%c%c%c'", fw_desc->header.header_id[0],
		 fw_desc->header.header_id[1], fw_desc->header.header_id[2],
		 fw_desc->header.header_id[3]);
	DUMP_PRINTABLE_BYTES("name", fw_desc->header.name);
	DUMP_KEY("preload_page_count", "%d", fw_desc->header.preload_page_count);
	DUMP_KEY("fw_image_flags", "0x%x", fw_desc->header.fw_image_flags);
	DUMP_KEY("feature_mask", "0x%x", fw_desc->header.feature_mask);
	DUMP_KEY("hw_buf_base_addr", "0x%x", fw_desc->header.fw_compat);
	DUMP_KEY("hw_buf_length", "0x%x", fw_desc->header.hw_buf_length);
	DUMP_KEY("load_offset", "0x%x", fw_desc->header.load_offset);
}

static int parse_fw_desc(const toml_table_t *toml, struct parse_ctx *pctx,
			 struct sof_man_fw_desc *out, bool verbose)
{
	static const uint8_t header_id[4] = SOF_MAN_FW_HDR_ID;
	struct parse_ctx ctx;
	toml_table_t *header;
	toml_table_t *desc;
	int ret;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	desc = toml_table_in(toml, "fw_desc");
	if (!desc)
		return err_key_not_found("fw_desc");
	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	header = toml_table_in(desc, "header");
	if (!header)
		return err_key_not_found("header");
	++ctx.table_cnt;

	/* check everything parsed */
	ret = assert_everything_parsed(desc, &ctx);
	if (ret < 0)
		return ret;

	/* initialize parser context for header subtable */
	parse_ctx_init(&ctx);

	/* non configurable flieds */
	memcpy(&out->header.header_id, header_id, sizeof(header_id));
	out->header.header_len = sizeof(struct sof_man_fw_header);

	/* configurable fields */
	parse_printable_key(header, &ctx, "name", out->header.name, sizeof(out->header.name),
			    &ret);
	if (ret < 0)
		return err_key_parse("header", NULL);

	out->header.preload_page_count =
		parse_uint32_key(header, &ctx, "preload_page_count", 0, &ret);
	if (ret < 0)
		return err_key_parse("header", NULL);

	out->header.fw_image_flags =
		parse_uint32_hex_key(header, &ctx, "fw_image_flags", SOF_MAN_FW_HDR_FLAGS, &ret);
	if (ret < 0)
		return err_key_parse("header", NULL);

	out->header.feature_mask =
		parse_uint32_hex_key(header, &ctx, "feature_mask", SOF_MAN_FW_HDR_FEATURES, &ret);
	if (ret < 0)
		return err_key_parse("header", NULL);

	out->header.fw_compat =
		parse_uint32_hex_key(header, &ctx, "hw_buf_base_addr", 0, &ret);
	if (ret < 0)
		return err_key_parse("header", NULL);

	out->header.hw_buf_length = parse_uint32_hex_key(header, &ctx, "hw_buf_length", 0, &ret);
	if (ret < 0)
		return err_key_parse("header", NULL);

	out->header.load_offset = parse_uint32_hex_key(header, &ctx, "load_offset", -1, &ret);
	if (ret < 0)
		return err_key_parse("header", NULL);

	/* check everything parsed */
	ret = assert_everything_parsed(header, &ctx);
	if (ret < 0)
		return ret;

	if (verbose)
		dump_fw_desc(out);

	/*
	 * values set in other places in code:
	 * - major_version
	 * - minor_version
	 * - build_version
	 * - num_module_entries
	 */

	return 0;
}

static int parse_scheduling(const toml_table_t *mod_entry, struct parse_ctx *ctx,
			    struct fw_image_ext_mod_config *ext_mod_config, int *ext_length)
{
	toml_array_t *arr;
	toml_raw_t raw;
	int64_t val;
	int ret;

	/* check "sched_caps" key */
	arr = toml_array_in(mod_entry, "sched_caps");
	if (!arr) {
		ext_mod_config->header.num_scheduling_capabilities = 0;
		*ext_length = 0;
		return 0;
	}

	if (toml_array_type(arr) != 'i'  || toml_array_nelem(arr) != 2 ||
	    toml_array_kind(arr) != 'v')
		return err_key_parse("sched_caps", "wrong array type or length != 2");

	ctx->array_cnt++;

	raw = toml_raw_at(arr, 0);
	if (raw == 0)
		return err_key_parse("frame_length", NULL);
	ret = toml_rtoi(raw, &val);
	if (ret < 0)
		return err_key_parse("frame_length", "can't convert element to integer");
	ext_mod_config->sched_caps.frame_length = val;

	raw = toml_raw_at(arr, 1);
	if (raw == 0)
		return err_key_parse("multiples_supported", NULL);
	ret = toml_rtoi(raw, &val);
	if (ret < 0)
		return err_key_parse("multiples_supported", "can't convert element to integer");
	ext_mod_config->sched_caps.multiples_supported.ul = val;

	ext_mod_config->header.num_scheduling_capabilities = 1;
	*ext_length = sizeof(const struct mod_scheduling_caps);

	return 0;
}

static int parse_pin(const toml_table_t *mod_entry, struct parse_ctx *ctx,
		     struct fw_image_ext_mod_config *ext_mod_config, int *ext_length)
{
	toml_array_t *arr;
	toml_raw_t raw;
	int64_t val;
	int ret;
	int i, j;

	/* check "pin" key */
	arr = toml_array_in(mod_entry, "pin");
	if (!arr) {
		ext_mod_config->header.num_pin_entries = 0;
		*ext_length = 0;
		return 0;
	}

	if (toml_array_type(arr) != 'i'  || toml_array_kind(arr) != 'v')
		return err_key_parse("pin", "wrong array type");

	ctx->array_cnt++;

	ext_mod_config->header.num_pin_entries = toml_array_nelem(arr) / 6;
	ext_mod_config->pin_desc = calloc(sizeof(const struct fw_pin_description),
					  toml_array_nelem(arr) / 6);

	if(!ext_mod_config->pin_desc)
		return err_malloc("pin");

	j = 0;
	for (i = 0; ; i += 6, j++) {
		raw = toml_raw_at(arr, i);
		if (raw == 0)
			break;

		ret = toml_rtoi(raw, &val);
		if (ret < 0)
			return err_key_parse("pin", "can't convert element to integer");
		ext_mod_config->pin_desc[j].caps.ul = (uint16_t)val;

		raw = toml_raw_at(arr, i + 1);
		ret = toml_rtoi(raw, &val);
		if (ret < 0)
			return err_key_parse("pin", "can't convert element to integer");
		ext_mod_config->pin_desc[j].format_type = (enum mod_stream_type)val;

		raw = toml_raw_at(arr, i + 2);
		ret = toml_rtoi(raw, &val);
		if (ret < 0)
			return err_key_parse("pin", "can't convert element to integer");
		ext_mod_config->pin_desc[j].sample_rate.ul = (uint32_t)val;

		raw = toml_raw_at(arr, i + 3);
		ret = toml_rtoi(raw, &val);
		if (ret < 0)
			return err_key_parse("pin", "can't convert element to integer");
		ext_mod_config->pin_desc[j].sample_size.ul = (uint16_t)val;

		raw = toml_raw_at(arr, i + 4);
		ret = toml_rtoi(raw, &val);
		if (ret < 0)
			return err_key_parse("pin", "can't convert element to integer");
		ext_mod_config->pin_desc[j].sample_container.ul = (uint32_t)val;

		raw = toml_raw_at(arr, i + 5);
		ret = toml_rtoi(raw, &val);
		if (ret < 0)
			return err_key_parse("pin", "can't convert element to integer");
		ext_mod_config->pin_desc[j].ch_cfg.ul = (uint32_t)val;
	}

	*ext_length = ext_mod_config->header.num_pin_entries *
			sizeof(const struct fw_pin_description);

	return 0;
}

static int parse_mod_config(const toml_table_t *mod_entry, struct parse_ctx *ctx,
			    struct fw_image_manifest_module *modules,
			    struct sof_man_module *mod_man)
{
	toml_array_t *arr;
	toml_raw_t raw;
	int *pin_data;
	int64_t val;
	int ret;
	int i;

	/* check "pin" key */
	arr = toml_array_in(mod_entry, "mod_cfg");
	if (!arr) {
		mod_man->cfg_count = 0;
		return 0;
	}

	if (toml_array_type(arr) != 'i' || toml_array_kind(arr) != 'v')
		return err_key_parse("mod_cfg", "wrong array type");

	ctx->array_cnt++;

	pin_data = (int *)(modules->mod_cfg + modules->mod_cfg_count);
	mod_man->cfg_offset = modules->mod_cfg_count;
	modules->mod_cfg_count += toml_array_nelem(arr) / 11;
	mod_man->cfg_count = toml_array_nelem(arr) / 11;

	/* parse "pin" array elements */
	for (i = 0; ; ++i) {
		raw = toml_raw_at(arr, i);
		if (raw == 0)
			break;

		ret = toml_rtoi(raw, &val);
		if (ret < 0)
			return err_key_parse("mod_cfg", "can't convert element to integer");
		pin_data[i] = val;
	}

	return 0;
}

static void dump_module(struct fw_image_manifest_module *man_cavs)
{
	int i;

	DUMP("\nmodule");
	DUMP_KEY("moudle count", "%d", man_cavs->mod_man_count);
	DUMP_KEY("module config count", "%d", man_cavs->mod_cfg_count);

	for (i = 0; i < man_cavs->mod_man_count; i++) {
		DUMP_PRINTABLE_BYTES("module name", man_cavs->mod_man[i].name);
		DUMP_KEY("load type", "%d", man_cavs->mod_man[i].type.load_type);
		DUMP_KEY("init config", "%d", man_cavs->mod_man[i].type.init_config);
		DUMP_KEY("domain ll", "%d", man_cavs->mod_man[i].type.domain_ll);
		DUMP_KEY("domain dp", "%d", man_cavs->mod_man[i].type.domain_dp);
		DUMP_KEY("config count", "%d", man_cavs->mod_man[i].cfg_count);
		DUMP_KEY("config offset", "%d", man_cavs->mod_man[i].cfg_offset);
	}
}

static int parse_module(const toml_table_t *toml, struct parse_ctx *pctx,
			struct adsp *out, bool verbose)
{
	struct fw_image_manifest_module *modules;
	toml_array_t *mod_entry_array;
	toml_table_t *module;
	toml_table_t *mod_entry;
	struct parse_ctx ctx;
	int entry_count;
	int type, ext_length;
	int tmp_cfg_count;
	int ret, i;

	/* look for subtable in toml, increment pctx parsed table cnt and initialize local ctx */
	module = toml_table_in(toml, "module");
	if (!module)
		return 0;

	out->write_firmware_ext_man = ext_man_write_cavs_25;

	modules = calloc(sizeof(struct fw_image_manifest_module), 1);
	if (!modules)
		return err_malloc("man_cavs");
	out->modules = modules;

	++pctx->table_cnt;
	parse_ctx_init(&ctx);

	entry_count = parse_uint32_key(module, &ctx, "count", 2, &ret);
	if (ret < 0)
		return ret;

	ctx.array_cnt += 1;

	mod_entry_array = toml_array_in(module, "entry");
	if (!mod_entry_array)
		return err_key_not_found("entry");
	if (toml_array_kind(mod_entry_array) != 't' ||
	    toml_array_nelem(mod_entry_array) != entry_count)
		return err_key_parse("entry", "wrong array type or length != %d", entry_count);

	modules->mod_ext.mod_conf_count = entry_count;
	modules->mod_man = calloc(sizeof(const struct sof_man_module), entry_count);
	if (!modules->mod_man)
		return -ENOMEM;

	modules->mod_man_count = toml_array_nelem(mod_entry_array);

	tmp_cfg_count = entry_count * 32;
	modules->mod_cfg = calloc(sizeof(const struct sof_man_mod_config), tmp_cfg_count);

	/* parse entry array elements */
	for (i = 0; i < toml_array_nelem(mod_entry_array); ++i) {
		struct fw_ext_mod_config_header *header;
		struct sof_man_module *mod_man;
		struct parse_ctx ctx_entry;
		char buf[48];

		mod_entry = toml_table_at(mod_entry_array, i);
		if (!mod_entry)
			return err_key_parse("entry", NULL);

		/* initialize parse context for each array element */
		parse_ctx_init(&ctx_entry);

		mod_man = &modules->mod_man[i];

		memcpy(mod_man->struct_id, "$AME", 4);

		/* configurable fields */
		parse_printable_key(mod_entry, &ctx_entry, "name", mod_man->name,
				    sizeof(mod_man->name), &ret);
		if (ret < 0)
			return err_key_parse("name", NULL);

		parse_str_key(mod_entry, &ctx_entry, "uuid", buf, sizeof(buf),
			      &ret);
		if (ret < 0)
			return err_key_parse("uuid", NULL);

		parse_uuid(buf, mod_man->uuid);

		mod_man->affinity_mask = parse_uint32_hex_key(mod_entry, &ctx_entry,
							      "affinity_mask", 1, &ret);
		if (ret < 0)
			return err_key_parse("affinity_mask", NULL);

		mod_man->instance_max_count = parse_uint32_hex_key(mod_entry, &ctx_entry,
								   "instance_count", 1, &ret);
		if (ret < 0)
			return err_key_parse("instance_count", NULL);

		type = parse_uint32_hex_key(mod_entry, &ctx_entry, "domain_types", 0, &ret);
		if (ret < 0)
			return err_key_parse("domain_types", NULL);
		if (!type)
			mod_man->type.domain_ll = 1;
		else
			mod_man->type.domain_dp = 1;

		mod_man->type.load_type = parse_uint32_hex_key(mod_entry, &ctx_entry,
							       "load_type", 1, &ret);
		if (ret < 0)
			return err_key_parse("load_type", NULL);

		mod_man->type.init_config = parse_uint32_hex_key(mod_entry, &ctx_entry,
								"init_config", 0, &ret);
		if (ret < 0)
			return err_key_parse("init_config", NULL);

		mod_man->type.auto_start = parse_uint32_hex_key(mod_entry, &ctx_entry,
								"auto_start", 1, &ret);
		if (ret < 0)
			return err_key_parse("auto_start", NULL);

		header = &modules->mod_ext.ext_mod_config_array[i].header;
		header->version_major = 2;
		header->version_minor = 5;
		header->ext_module_config_length = sizeof(struct fw_ext_mod_config_header);
		memcpy(header->guid, mod_man->uuid, sizeof(mod_man->uuid));

		type = parse_uint32_hex_key(mod_entry, &ctx_entry, "module_type", 1, &ret);
		if (ret < 0)
			return err_key_parse("module_type", NULL);

		header->module_type = type;

		ret = parse_scheduling(mod_entry, &ctx_entry,
				       modules->mod_ext.ext_mod_config_array + i, &ext_length);
		if (ret < 0)
			return err_key_parse("schd_caps", NULL);
		header->ext_module_config_length += ext_length;

		ret = parse_pin(mod_entry, &ctx_entry, modules->mod_ext.ext_mod_config_array + i,
				&ext_length);
		if (ret < 0)
			return err_key_parse("pin", NULL);
		header->ext_module_config_length += ext_length;

		ret = parse_mod_config(mod_entry, &ctx_entry, modules, mod_man);
		if (ret < 0)
			return err_key_parse("mod_cfg", NULL);

		if (modules->mod_cfg_count > tmp_cfg_count)
			return -ENOMEM;

		/* check everything parsed */
		ret = assert_everything_parsed(mod_entry, &ctx_entry);
		if (ret < 0)
			return ret;
	}

	/* check everything parsed */
	ret = assert_everything_parsed(module, &ctx);
	if (ret < 0)
		return ret;

	if (verbose)
		dump_module(modules);

	return 0;
}

static int parse_adsp_config_v1_0(const toml_table_t *toml, struct image *image)
{
	struct adsp *out = image->adsp;
	bool verbose = image->verbose;
	struct parse_ctx ctx;
	int ret;

	/* version array has already been parsed, so increment ctx.array_cnt */
	parse_ctx_init(&ctx);
	++ctx.array_cnt;

	/* parse each adsp subtable, sue platform has different manifest definition */
	ret = parse_adsp(toml, &ctx, out, verbose);
	if (ret < 0)
		return err_key_parse("adsp", NULL);

	/* assign correct write functions */
	out->write_firmware = simple_write_firmware;
	out->write_firmware_meu = NULL;

	/* check everything parsed */
	ret = assert_everything_parsed(toml, &ctx);
	if (ret < 0)
		return ret;

	return 0;
}

static int parse_adsp_config_v1_5(const toml_table_t *toml, struct image *image)
{
	struct adsp *out = image->adsp;
	bool verbose = image->verbose;
	struct parse_ctx ctx;
	int ret;

	/* version array has already been parsed, so increment ctx.array_cnt */
	parse_ctx_init(&ctx);
	++ctx.array_cnt;

	/* parse each adsp subtable, sue platform has different manifest definition */
	ret = parse_adsp(toml, &ctx, out, verbose);
	if (ret < 0)
		return err_key_parse("adsp", NULL);

	/* suecreek has dedicated manifest file */
	if (!strcmp(out->name, "sue")) {
		/* out free is done in client code */
		out->man_v1_5_sue = malloc(sizeof(struct fw_image_manifest_v1_5_sue));
		if (!out->man_v1_5_sue)
			return err_malloc("man_v1_5_sue");

		/* clear memory */
		memset(out->man_v1_5_sue, 0, sizeof(*out->man_v1_5_sue));

		/* assign correct write functions */
		out->write_firmware = man_write_fw_v1_5_sue;
		out->write_firmware_meu = man_write_fw_meu_v1_5;
		out->verify_firmware = ri_manifest_verify_v1_5;

		/* parse others sibtables */
		ret = parse_fw_desc(toml, &ctx, &out->man_v1_5_sue->desc, verbose);
		if (ret < 0)
			return err_key_parse("fw_desc", NULL);
	} else {
		/* out free is done in client code */
		out->man_v1_5 = malloc(sizeof(struct fw_image_manifest_v1_5));
		if (!out->man_v1_5)
			return err_malloc("man_v1_5");

		/* clear memory */
		memset(out->man_v1_5, 0, sizeof(*out->man_v1_5));

		/* assign correct write functions */
		out->write_firmware = man_write_fw_v1_5;
		out->write_firmware_meu = man_write_fw_meu_v1_5;
		out->verify_firmware = ri_manifest_verify_v1_5;

		/* parse others sibtables */
		ret = parse_css_v1_5(toml, &ctx, &out->man_v1_5->css_header, verbose);
		if (ret < 0)
			return err_key_parse("css", NULL);

		ret = parse_fw_desc(toml, &ctx, &out->man_v1_5->desc, verbose);
		if (ret < 0)
			return err_key_parse("fw_desc", NULL);
	}

	/* check everything parsed */
	ret = assert_everything_parsed(toml, &ctx);
	if (ret < 0)
		return ret;

	return 0;
}

static int parse_adsp_config_v1_8(const toml_table_t *toml, struct image *image)
{
	struct adsp *out = image->adsp;
	bool verbose = image->verbose;
	struct parse_ctx ctx;
	int ret;

	/* out free is done in client code */
	out->man_v1_8 = malloc(sizeof(struct fw_image_manifest_v1_8));
	if (!out->man_v1_8)
		return err_malloc("man_v1_8");

	/* clear memory */
	memset(out->man_v1_8, 0, sizeof(*out->man_v1_8));

	/* assign correct write functions */
	out->write_firmware = man_write_fw_v1_8;
	out->write_firmware_meu = man_write_fw_meu_v1_8;
	out->verify_firmware = ri_manifest_verify_v1_8;

	/* version array has already been parsed, so increment ctx.array_cnt */
	parse_ctx_init(&ctx);
	++ctx.array_cnt;

	/* parse each toml subtable */
	ret = parse_adsp(toml, &ctx, out, verbose);
	if (ret < 0)
		return err_key_parse("adsp", NULL);

	ret = parse_cse(toml, &ctx, &out->man_v1_8->cse_partition_dir_header,
			out->man_v1_8->cse_partition_dir_entry, MAN_CSE_PARTS, verbose);
	if (ret < 0)
		return err_key_parse("cse", NULL);

	ret = parse_css_v1_8(toml, &ctx, &out->man_v1_8->css, verbose);
	if (ret < 0)
		return err_key_parse("css", NULL);

	ret = parse_signed_pkg(toml, &ctx, image, verbose);
	if (ret < 0)
		return err_key_parse("signed_pkg", NULL);

	ret = parse_partition_info_ext(toml, &ctx, &out->man_v1_8->partition_info, verbose);
	if (ret < 0)
		return err_key_parse("partition_info", NULL);

	ret = parse_adsp_file_ext_v1_8(toml, &ctx, &out->man_v1_8->adsp_file_ext, verbose);
	if (ret < 0)
		return err_key_parse("adsp_file", NULL);

	ret = parse_fw_desc(toml, &ctx, &out->man_v1_8->desc, verbose);
	if (ret < 0)
		return err_key_parse("fw_desc", NULL);

	/* check everything parsed */
	ret = assert_everything_parsed(toml, &ctx);
	if (ret < 0)
		return ret;

	return 0;
}

static int parse_adsp_config_v2_5(const toml_table_t *toml, struct image *image)
{
	struct adsp *out = image->adsp;
	bool verbose = image->verbose;
	struct parse_ctx ctx;
	int ret;

	/* out free is done in client code */
	out->man_v2_5 = malloc(sizeof(struct fw_image_manifest_v2_5));
	if (!out->man_v2_5)
		return err_malloc("man_v2_5");

	/* clear memory */
	memset(out->man_v2_5, 0, sizeof(*out->man_v2_5));

	/* assign correct write functions */
	out->write_firmware = man_write_fw_v2_5;
	out->write_firmware_meu = man_write_fw_meu_v2_5;
	out->verify_firmware = ri_manifest_verify_v2_5;

	/* version array has already been parsed, so increment ctx.array_cnt */
	parse_ctx_init(&ctx);
	++ctx.array_cnt;

	/* parse each toml subtable */
	ret = parse_adsp(toml, &ctx, out, verbose);
	if (ret < 0)
		return err_key_parse("adsp", NULL);

	ret = parse_cse_v2_5(toml, &ctx, &out->man_v2_5->cse_partition_dir_header,
			out->man_v2_5->cse_partition_dir_entry, MAN_CSE_PARTS, verbose);
	if (ret < 0)
		return err_key_parse("cse", NULL);

	ret = parse_css_v2_5(toml, &ctx, &out->man_v2_5->css, verbose);
	if (ret < 0)
		return err_key_parse("css", NULL);

	ret = parse_signed_pkg_v2_5(toml, &ctx, image, verbose);
	if (ret < 0)
		return err_key_parse("signed_pkg", NULL);

	ret = parse_info_ext_0x16(toml, &ctx, &out->man_v2_5->info_0x16, verbose);
	if (ret < 0)
		return err_key_parse("partition_info", NULL);

	ret = parse_adsp_file_ext_v2_5(toml, &ctx, &out->man_v2_5->adsp_file_ext, verbose);
	if (ret < 0)
		return err_key_parse("adsp_file", NULL);

	ret = parse_fw_desc(toml, &ctx, &out->man_v2_5->desc, verbose);
	if (ret < 0)
		return err_key_parse("fw_desc", NULL);

	ret = parse_module(toml, &ctx, out, verbose);
	if (ret < 0)
		return err_key_parse("module", NULL);

	/* check everything parsed */
	ret = assert_everything_parsed(toml, &ctx);
	if (ret < 0)
		return ret;

	return 0;
}

static int parse_adsp_config_ace_v1_5(const toml_table_t *toml, struct image *image)
{
	struct adsp *out = image->adsp;
	bool verbose = image->verbose;
	struct parse_ctx ctx;
	int ret;

	out->man_ace_v1_5 = malloc(sizeof(struct fw_image_manifest_ace_v1_5));
	if (!out->man_ace_v1_5)
		return err_malloc("man_ace_v1_5");

	/* clear memory */
	memset(out->man_ace_v1_5, 0, sizeof(*out->man_ace_v1_5));

	/* assign correct write functions */
	out->write_firmware = man_write_fw_ace_v1_5;
	out->write_firmware_meu = man_write_fw_meu_v2_5;
	out->verify_firmware = ri_manifest_verify_v2_5;

	/* version array has already been parsed, so increment ctx.array_cnt */
	parse_ctx_init(&ctx);
	++ctx.array_cnt;

	/* parse each toml subtable */
	ret = parse_adsp(toml, &ctx, out, verbose);
	if (ret < 0)
		return err_key_parse("adsp", NULL);

	ret = parse_cse_v2_5(toml, &ctx, &out->man_ace_v1_5->cse_partition_dir_header,
			out->man_ace_v1_5->cse_partition_dir_entry, 3, verbose);
	if (ret < 0)
		return err_key_parse("cse", NULL);

	ret = parse_css_v2_5(toml, &ctx, &out->man_ace_v1_5->css, verbose);
	if (ret < 0)
		return err_key_parse("css", NULL);

	ret = parse_signed_pkg_ace_v1_5(toml, &ctx, image, verbose);
	if (ret < 0)
		return err_key_parse("signed_pkg", NULL);

	ret = parse_info_ext_0x16(toml, &ctx, &out->man_ace_v1_5->info_0x16, verbose);
	if (ret < 0)
		return err_key_parse("partition_info", NULL);

	ret = parse_adsp_file_ext_v2_5(toml, &ctx, &out->man_ace_v1_5->adsp_file_ext, verbose);
	if (ret < 0)
		return err_key_parse("adsp_file", NULL);

	ret = parse_fw_desc(toml, &ctx, &out->man_ace_v1_5->desc, verbose);
	if (ret < 0)
		return err_key_parse("fw_desc", NULL);

	ret = parse_module(toml, &ctx, out, verbose);
	if (ret < 0)
		return err_key_parse("module", NULL);

	/* check everything parsed */
	ret = assert_everything_parsed(toml, &ctx);
	if (ret < 0)
		return ret;

	return 0;
}

struct config_parser {
	int major;
	int minor;
	int (*parse)(const toml_table_t *toml, struct image *image);
};

static const struct config_parser *find_config_parser(int64_t version[2])
{
	/* list of supported configuration version with handler to parser */
	static const struct config_parser parsers[] = {
		{1, 0, parse_adsp_config_v1_0},
		{1, 5, parse_adsp_config_v1_5},
		{1, 8, parse_adsp_config_v1_8},
		{2, 5, parse_adsp_config_v2_5},
		{3, 0, parse_adsp_config_ace_v1_5},
	};
	int i;

	for (i = 0; i < ARRAY_SIZE(parsers); ++i) {
		if (parsers[i].major == version[0] &&
		    parsers[i].minor == version[1]) {
			return &parsers[i];
		}
	}
	return NULL;
}

static int adsp_parse_config_fd(FILE *fd, struct image *image)
{
	const struct config_parser *parser;
	int64_t manifest_version[2];
	toml_table_t *toml;
	char errbuf[256];
	int ret;

	/* whole toml file is parsed to global toml table at once */
	toml = toml_parse_file(fd, errbuf, ARRAY_SIZE(errbuf));
	if (!toml)
		return log_err(-EINVAL, "error: toml file parsing, %s\n", errbuf);

	/* manifest version is in toml root */
	ret = parse_version(toml, manifest_version);
	if (ret < 0)
		goto error;

	/* find parser compatible with manifest version */
	parser = find_config_parser(manifest_version);
	if (!parser) {
		ret = log_err(-EINVAL, "error: Unsupported config version %d.%d\n",
			      manifest_version[0], manifest_version[1]);
		goto error;
	}

	/* run dedicated toml configuration parser */
	ret = parser->parse(toml, image);
error:
	toml_free(toml);
	return ret;
}

/* public function, fully handle parsing process */
int adsp_parse_config(const char *file, struct image *image)
{
	FILE *fd;
	int ret;

	fd = fopen(file, "r");
	if (!fd)
		return file_error("unable to open file for reading", file);

	ret = adsp_parse_config_fd(fd, image);
	fclose(fd);
	return ret;
}

/* free given pointer and internally allocated memory */
void adsp_free(struct adsp *adsp)
{
	if (!adsp)
		return;

	if (adsp->man_v1_5)
		free(adsp->man_v1_5);

	if (adsp->man_v1_5_sue)
		free(adsp->man_v1_5_sue);

	if (adsp->man_v1_8)
		free(adsp->man_v1_8);

	if (adsp->man_v2_5)
		free(adsp->man_v2_5);

	if (adsp->modules)
		free(adsp->modules);

	if (adsp->name)
		free((char *)adsp->name);

	free(adsp);
}
