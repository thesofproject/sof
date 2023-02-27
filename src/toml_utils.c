/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 *         Marc Herbert <marc.herbert@intel.com>
 */

#include "toml.h"
#include <rimage/toml_utils.h>
#include <rimage/cavs/cavs_ext_manifest.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

void print_bytes(FILE *out, const uint8_t *arr, size_t len)
{
	for (const uint8_t *pos = arr; pos < arr + len; pos++) {
		char c = *pos;

		if (isprint(c))
			fputc(c, out);
		else
			fprintf(out, "\\x%.2x", c);
	}
}

#define DUMP_PRINTABLE_BYTES(name, var) _dump_printable_bytes(name, var, sizeof(var))

void _dump_printable_bytes(const char *name, const uint8_t *arr, size_t len)
{
	printf(DUMP_KEY_FMT, name);
	print_bytes(stdout, arr, len);
	printf("\n");
}

/** private parser error trace function */
void vlog_err(const char *msg, va_list vl)
{
	vfprintf(stderr, msg, vl);
}

/** parser error trace function, error code is returned to shorten client code */
int log_err(int err_code, const char *msg, ...)
{
	va_list vl;

	va_start(vl, msg);
	vlog_err(msg, vl);
	va_end(vl);
	return err_code;
}

/** log malloc error message for given key */
int err_malloc(const char *key)
{
	return log_err(-ENOMEM, "error: malloc failed during parsing key '%s'\n", key);
}

/** log key not found error */
int err_key_not_found(const char *key)
{
	return log_err(-EINVAL, "error: '%s' not found\n", key);
}

/** error during parsing key value, possible detailed message */
int err_key_parse(const char *key, const char *extra_msg, ...)
{
	int ret = -EINVAL;
	va_list vl;

	if (extra_msg) {
		log_err(ret, "error: key '%s' parsing error, ", key);
		va_start(vl, extra_msg);
		vlog_err(extra_msg, vl);
		va_end(vl);
		return log_err(ret, "\n");
	} else {
		return log_err(ret, "error: key '%s' parsing error\n", key);
	}
}

/** initialize parser context before parsing */
void parse_ctx_init(struct parse_ctx *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
}

/** check nothing left unparsed in given parsing context */
int assert_everything_parsed(const toml_table_t *table, struct parse_ctx *ctx)
{
	const char *key = toml_table_key(table);
	int ret = 0;

	/* toml_table_key returns NULL for global context */
	if (!key)
		key = "toml";

	/* from number of parsed fields subtract fields count in given table */
	ctx->key_cnt = toml_table_nkval(table) - ctx->key_cnt;
	ctx->array_cnt = toml_table_narr(table) - ctx->array_cnt;
	ctx->table_cnt = toml_table_ntab(table) - ctx->table_cnt;

	/* when any field left unparsed, then raise error */
	if (ctx->key_cnt != 0)
		ret = log_err(-EINVAL, "error: %d unparsed keys left in '%s'\n", ctx->key_cnt, key);
	if (ctx->array_cnt != 0)
		ret = log_err(-EINVAL, "error: %d unparsed arrays left in '%s'\n", ctx->array_cnt,
			      key);
	if (ctx->table_cnt != 0)
		ret = log_err(-EINVAL, "error: %d unparsed tables left in '%s'\n", ctx->table_cnt,
			      key);
	return ret;
}

/**
 * Parse hex value from key in given toml table
 * @param table toml table where key is specified
 * @param ctx parsing context, key counter will be incremented after successful key parse
 * @param key field name
 * @param def is default value or -1 when value don't have default value
 * @param error code, 0 when success
 * @return default, parsed, or UINT32_MAX value for error cases
 */
uint32_t parse_uint32_hex_key(const toml_table_t *table, struct parse_ctx *ctx,
			      const char *key, int64_t def, int *error)
{
	toml_raw_t raw;
	char *temp_s;
	uint32_t val;
	int ret;

	/* look for key in given table, assign def value when key not found */
	raw = toml_raw_in(table, key);
	if (!raw) {
		if (def < 0 || def > UINT32_MAX) {
			*error = err_key_not_found(key);
			return UINT32_MAX;
		} else {
			*error = 0;
			return (uint32_t)def;
		}
	}
	/* there is not build-in support for hex numbers in toml, so read then as string */
	ret = toml_rtos(raw, &temp_s);
	if (ret < 0) {
		*error = err_key_parse(key, NULL);
		return UINT32_MAX;
	}
	val = strtoul(temp_s, 0, 0);

	free(temp_s);
	/* assert parsing success and value is within uint32_t range */
	if (errno < 0) {
		*error = err_key_parse(key, "can't convert hex value");
		return UINT32_MAX;
	}

	/* set success error code and increment parsed key counter */
	*error = 0;
	++ctx->key_cnt;
	return (uint32_t)val;
}

/**
 * Parse integer value from key in given toml table
 * @param table toml table where key is specified
 * @param ctx parsing context, key counter will be incremented after successful key parse
 * @param key field name
 * @param def is default value or -1 when value don't have default value
 * @param error code, 0 when success
 * @return default, parsed, or UINT32_MAX value for error cases
 */
uint32_t parse_uint32_key(const toml_table_t *table, struct parse_ctx *ctx, const char *key,
			  int64_t def, int *error)
{
	toml_raw_t raw;
	int64_t val;
	int ret;

	/* look for key in given table, assign def value when key not found */
	raw = toml_raw_in(table, key);
	if (!raw) {
		if (def < 0 || def > UINT32_MAX) {
			*error = err_key_not_found(key);
			return UINT32_MAX;
		} else {
			*error = 0;
			return (uint32_t)def;
		}
	}
	/* there is build-in support for integer numbers in toml, so use lib function */
	ret = toml_rtoi(raw, &val);
	if (ret < 0) {
		*error = err_key_parse(key, "can't convert to integer value");
		return UINT32_MAX;
	}
	/* assert value is within uint32_t range */
	if (val < 0 || val > UINT32_MAX) {
		*error = log_err(-ERANGE, "key %s out of uint32_t range", key);
		return UINT32_MAX;
	}
	/* set success error code and increment parsed key counter */
	*error = 0;
	++ctx->key_cnt;
	return (uint32_t)val;
}

/**
 * Parse string value from key in given toml table to uint8_t array. The
 * destination is NOT a string because it is padded with zeros if and
 * only if there is some capacity left. For string destinations use
 * parse_str_key().
 *
 * @param table toml table where key is specified
 * @param ctx parsing context, key counter will be incremented after successful key parse
 * @param key field name
 * @param dst uint8_t[] destination
 * @param capacity dst array size
 * @param error code, 0 when success
 */
void parse_printable_key(const toml_table_t *table, struct parse_ctx *ctx, const char *key,
			 uint8_t *dst, int capacity, int *error)
{
	toml_raw_t raw;
	char *temp_s;
	int len;
	int ret;

	/* look for key in given table  */
	raw = toml_raw_in(table, key);
	if (!raw) {
		*error = err_key_not_found(key);
		return;
	}
	/* read string from toml, theres malloc inside toml_rtos() */
	ret = toml_rtos(raw, &temp_s);
	if (ret < 0) {
		*error = err_key_parse(key, NULL);
		return;
	}

	len = strlen(temp_s);
	if (len > capacity) {
		if (len > 20) {
			static const char ellipsis[] = "...";
			const size_t el_len = sizeof(ellipsis);

			strncpy(temp_s + 20 - el_len, ellipsis, el_len);
		}

		*error = log_err(-EINVAL, "Too long input '%s' for key '%s' (%d > %d) characters\n",
				 temp_s, key, len, capacity);
		free(temp_s);
		return;
	}

	/* copy string to dst, pad with zeros the space left if any */
	strncpy((char *)dst, temp_s, capacity);
	free(temp_s);
	/* update parsing context */
	++ctx->key_cnt;
	*error = 0;
}

/**
 * Parse string value from key in given toml table to given
 * char[]. Destination is padded with zeros. As the only difference with
 * parse_printable_key(), dst is guaranteed to be null-terminated when
 * there is no error because the last destination byte is reserved for
 * that.
 *
 * @param table toml table where key is specified
 * @param ctx parsing context, key counter will be incremented after successful key parse
 * @param key field name
 * @param dst char[] destination
 * @param capacity dst array size including null termination.
 * @param error code, 0 when success
 */
void parse_str_key(const toml_table_t *table, struct parse_ctx *ctx, const char *key,
		   char *dst, int capacity, int *error)
{
	parse_printable_key(table, ctx, key, (uint8_t *)dst, capacity - 1, error);
	if (*error) /* return immediately to help forensics */
		return;
	dst[capacity - 1] = 0;
}

void parse_uuid(char *buf, uint8_t *uuid)
{
	struct uuid_t id;
	uint32_t d[10];

	sscanf(buf, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", &id.d0, &d[0],
	       &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9]);
	id.d1 = (uint16_t)d[0];
	id.d2 = (uint16_t)d[1];
	id.d3 = (uint8_t)d[2];
	id.d4 = (uint8_t)d[3];
	id.d5 = (uint8_t)d[4];
	id.d6 = (uint8_t)d[5];
	id.d7 = (uint8_t)d[6];
	id.d8 = (uint8_t)d[7];
	id.d9 = (uint8_t)d[8];
	id.d10 = (uint8_t)d[9];

	memcpy(uuid, &id, sizeof(id));
}

/** version is stored as toml array with integer number, something like:
 *   "version = [1, 8]"
 */
int parse_version(toml_table_t *toml, int64_t version[2])
{
	toml_array_t *arr;
	toml_raw_t raw;
	int ret;
	int i;

	/* check "version" key */
	arr = toml_array_in(toml, "version");
	if (!arr)
		return err_key_not_found("version");
	if (toml_array_type(arr) != 'i'  || toml_array_nelem(arr) != 2 ||
	    toml_array_kind(arr) != 'v')
		return err_key_parse("version", "wrong array type or length != 2");

	/* parse "version" array elements */
	for (i = 0; i < 2; ++i) {
		raw = toml_raw_at(arr, i);
		if (raw == 0)
			return err_key_parse("version", NULL);
		ret = toml_rtoi(raw, &version[i]);
		if (ret < 0)
			return err_key_parse("version", "can't convert element to integer");
	}
	return 0;
}
