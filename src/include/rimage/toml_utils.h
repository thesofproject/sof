// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>
 *         Marc Herbert <marc.herbert@intel.com>
 */

#ifndef __TOML_UTILS_H__
#define __TOML_UTILS_H__

#include "toml.h"

#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>

/** parser counter, used to assert nothing left unparsed in toml data */
struct parse_ctx {
	int key_cnt;		/**< number of parsed key */
	int table_cnt;		/**< number of parsed tables */
	int array_cnt;		/**< number of parsed arrays */
};

/* macros used to dump values after parsing */
#define DUMP_KEY_FMT "   %20s: "
#define DUMP(fmt, ...) fprintf(stdout, fmt "\n", ##__VA_ARGS__)
#define DUMP_KEY(key, fmt, ...) DUMP(DUMP_KEY_FMT fmt, key, ##__VA_ARGS__)

void print_bytes(FILE *out, const uint8_t *arr, size_t len);

#define DUMP_PRINTABLE_BYTES(name, var) _dump_printable_bytes(name, var, sizeof(var))

void _dump_printable_bytes(const char *name, const uint8_t *arr, size_t len);

/** private parser error trace function */
void vlog_err(const char *msg, va_list vl);

/** parser error trace function, error code is returned to shorten client code */
int log_err(int err_code, const char *msg, ...);

/** log malloc error message for given key */
int err_malloc(const char *key);

/** log key not found error */
int err_key_not_found(const char *key);

/** error during parsing key value, possible detailed message */
int err_key_parse(const char *key, const char *extra_msg, ...);

/** initialize parser context before parsing */
void parse_ctx_init(struct parse_ctx *ctx);

/** check nothing left unparsed in given parsing context */
int assert_everything_parsed(const toml_table_t *table, struct parse_ctx *ctx);

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
			      const char *key, int64_t def, int *error);

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
			  int64_t def, int *error);

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
			 uint8_t *dst, int capacity, int *error);

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
		   char *dst, int capacity, int *error);

void parse_uuid(char *buf, uint8_t *uuid);

/** version is stored as toml array with integer number, something like:
 *   "version = [1, 8]"
 */
int parse_version(toml_table_t *toml, int64_t version[2]);

#endif /* __TOML_UTILS_H__ */
