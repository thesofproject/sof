/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 * Author: Karol Trzcinski	<karolx.trzcinski@linux.intel.com>
 */

#include <ipc/trace.h>
#include <smex/ldc.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <user/trace.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "convert.h"
#include "filter.h"
#include "misc.h"

#define COMPONENTS_SEPARATOR ','
#define COMPONENT_NAME_SCAN_STRING_LENGTH 32

extern struct convert_config *global_config;

/** map between log level given by user and enum value */
static const struct {
	const char name[16];
	int32_t log_level;
} log_level_dict[] = {
	{"verbose",	LOG_LEVEL_VERBOSE},
	{"debug",	LOG_LEVEL_DEBUG},
	{"info",	LOG_LEVEL_INFO},
	{"warning",	LOG_LEVEL_WARNING},
	{"error",	LOG_LEVEL_ERROR},
	{"critical",	LOG_LEVEL_CRITICAL},
	{"v",		LOG_LEVEL_VERBOSE},
	{"d",		LOG_LEVEL_DEBUG},
	{"i",		LOG_LEVEL_INFO},
	{"w",		LOG_LEVEL_WARNING},
	{"e",		LOG_LEVEL_ERROR},
	{"c",		LOG_LEVEL_CRITICAL},
};

struct filter_element {
	struct list_item list;
	int32_t uuid_id;	/**< type id, or -1 when not important */
	int32_t comp_id;	/**< component id or -1 when not important */
	int32_t pipe_id;	/**< pipeline id or -1 when not important */
	int32_t log_level;	/**< new log level value */
};

/**
 * Search for uuid entry with given component name in given uids dictionary
 * @param name of uuid entry
 * @return pointer to sof_uuid_entry with given name
 */
static struct sof_uuid_entry *get_uuid_by_name(const char *name)
{
	const struct snd_sof_uids_header *uids_dict = global_config->uids_dict;
	uintptr_t beg = (uintptr_t)uids_dict + uids_dict->data_offset;
	uintptr_t end = beg + uids_dict->data_length;
	struct sof_uuid_entry *ptr = (struct sof_uuid_entry *)beg;

	while ((uintptr_t)ptr < end) {
		if (strcmp(name, ptr->name) == 0)
			return ptr;
		++ptr;
	}
	return NULL;
}

/**
 * Parse log level name (from log_level_dict) to enum value.
 * Take care about possible whitespace at the begin.
 * @param value_start pointer to the begin of range to search
 * @return enum value for given log level, or -1 for invalid value
 */
static int filter_parse_log_level(const char *value_start)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(log_level_dict); ++i) {
		if (!strcmp(log_level_dict[i].name, value_start))
			return log_level_dict[i].log_level;
	}
	return -1;
}

static char *filter_parse_component_name(char *input_str, struct filter_element *out)
{
	static char scan_format_string[COMPONENT_NAME_SCAN_STRING_LENGTH] = "";
	char comp_name[UUID_NAME_MAX_LEN];
	struct sof_uuid_entry *uuid_entry;
	int ret;

	/* if component name is not specified, stay with default out->uuid_id value */
	if (input_str[0] == '*')
		return &input_str[1];

	/*
	 * Take care about buffer overflows when dealing with input from
	 * user, so scan no more than UUID_NAME_MAX_LEN bytes to
	 * `comp_name` variable. Only once initialise scan_format_string.
	 */
	if (strlen(scan_format_string) == 0) {
		ret = snprintf(scan_format_string, sizeof(scan_format_string),
				"%%%d[^0-9* ]s", UUID_NAME_MAX_LEN);
		if (ret <= 0)
			return NULL;
	}
	ret = sscanf(input_str, scan_format_string, comp_name);
	if (ret <= 0)
		return NULL;

	/* find component uuid key */
	uuid_entry = get_uuid_by_name(comp_name);
	if (!uuid_entry) {
		log_err("unknown component name `%s`\n", comp_name);
		return NULL;
	}
	out->uuid_id = get_uuid_key(uuid_entry);
	return strstr(input_str, comp_name) + strlen(comp_name);
}

/**
 * Parse component definition from input_str.
 *
 * Possible input_str formats:
 *   `name pipe_id.comp_id`
 *   `name pipe_id.*`
 *   `name *`
 *   `name`
 *   `* pipe_id.comp_id`
 *   `* pipe_id.*`
 *   `*`
 * Whitespace is possible at the begin, end and after `name`.
 * `name` must refer to values from given UUID dictionary,
 *        (so name comes from DECLARE_SOF_UUID macro usage)

 * @param input_str formatted component definition
 * @param out element where component definition should be saved
 */
static int filter_parse_component(char *input_str, struct filter_element *out)
{
	char *instance_info;
	int ret;

	/* trim whitespaces, to easily check first and last char */
	input_str = trim(input_str);

	/* assign default values */
	out->uuid_id = 0;
	out->pipe_id = -1;
	out->comp_id = -1;

	/* parse component name and store pointer after component name, pointer to instance info */
	instance_info = filter_parse_component_name(input_str, out);
	if (!instance_info) {
		log_err("component name parsing `%s`\n", input_str);
		return -EINVAL;
	}

	/* if instance is not specified then stop parsing */
	instance_info = ltrim(instance_info);
	if (instance_info[0] == '\0' ||
	    (instance_info[0] == '*' && instance_info[1] == '\0')) {
		return 0;
	}

	/* now parse last part: `number.x` where x is a number or `*` */
	ret = sscanf(instance_info, "%d.%d", &out->pipe_id, &out->comp_id);
	if (ret == 2)
		return 0;
	else if (ret != 1)
		return -EINVAL;

	/* pipeline id parsed but component id is not a number */
	if (instance_info[strlen(instance_info) - 1] == '*')
		return 0;
	log_err("Use * to specify each component on particular pipeline\n");
	return -EINVAL;
}

/**
 * Convert argument from -F flag to sof_ipc_dma_trace_filter_elem struct values.
 *
 * Possible log_level - see filter_parse_log_level() documentation
 * Possible component - list of components separated by `COMPONENTS_SEPARATOR`,
 *                      for single component definition description look at
 *                      filter_parse_component() documentation
 *
 * Examples:
 *   `debug="pipe1"` - set debug log level for components from pipeline1
 *   `d="pipe1, dai2.3"` - as above, but also for dai2.3
 *   `error="FIR*"` - for each FIR component set log level to error
 *
 * @param input_str log level settings in format `log_level=component`
 * @param out_list output list with filter_element elements
 */
static int filter_parse_entry(char *input_str, struct list_item *out_list)
{
	struct filter_element *filter;
	char *comp_fmt_end;
	int32_t log_level;
	char *comp_fmt;
	int ret;

	/*
	 * split string on '=' char, left part describes log level,
	 * the right one is for component description.
	 */
	comp_fmt = strchr(input_str, '=');
	if (!comp_fmt) {
		log_err("unable to find `=` in `%s`\n", input_str);
		return -EINVAL;
	}
	*comp_fmt = 0;
	++comp_fmt;

	/* find correct log level in given conf string - string before `=` */
	log_level = filter_parse_log_level(input_str);
	if (log_level < 0) {
		log_err("unable to parse log level from `%s`\n", input_str);
		return log_level;
	}

	/*
	 * now parse list of components name and optional instance identifier,
	 * split string on `COMPONENTS_SEPARATOR`
	 */
	while (comp_fmt) {
		filter = malloc(sizeof(struct filter_element));
		if (!filter) {
			log_err("unable to malloc memory\n");
			return -ENOMEM;
		}

		comp_fmt_end = strchr(comp_fmt, COMPONENTS_SEPARATOR);
		if (comp_fmt_end)
			*comp_fmt_end = '\0';
		ret = filter_parse_component(comp_fmt, filter);
		if (ret < 0) {
			log_err("unable to parse component from `%s`\n", comp_fmt);
			free(filter);
			return ret;
		}
		filter->log_level = log_level;

		list_item_append(&filter->list, out_list);
		comp_fmt = comp_fmt_end ? comp_fmt_end + 1 : 0;
	}

	return 0;
}

/**
 * Parse `input_str` content and send it to FW via debugFS.
 *
 * `input_str` contain single filter definition element per line.
 * Each line is parsed by `filter_parse_entry`, and saved in list.
 * List of `sof_ipc_dma_trace_filter_elem` is writend to debugFS,
 * and then send as IPC to FW (this action is implemented in driver).
 * Each line in debugFS represents single IPC message.
 */
int filter_update_firmware(void)
{
	char *input_str = global_config->filter_config;
	struct filter_element *filter;
	struct list_item filter_list;
	struct list_item *list_elem;
	struct list_item *list_temp;
	char *line_end;
	FILE *out_fd = NULL;
	int ret = 0;

	list_init(&filter_list);

	/* parse `input_str` line by line */
	line_end = strchr(input_str, '\n');
	while (line_end) {
		line_end[0] = '\0';
		ret = filter_parse_entry(input_str, &filter_list);
		if (ret < 0)
			goto err;
		input_str = line_end + 1;
		line_end = strchr(input_str, '\n');
	}

	/* write output to debugFS */
	out_fd = fopen(FILTER_KERNEL_PATH, "w");
	if (!out_fd) {
		log_err("Unable to open out file '%s'\n", FILTER_KERNEL_PATH);
		ret = -errno;
		goto err;
	}

	list_for_item(list_elem, &filter_list) {
		filter = container_of(list_elem, struct filter_element, list);
		fprintf(out_fd, "%d %X %d %d;", filter->log_level,
			filter->uuid_id, filter->pipe_id, filter->comp_id);
	}
	fprintf(out_fd, "\n");

err:
	if (out_fd)
		fclose(out_fd);

	/* free each component from parsed element list */
	list_for_item_safe(list_elem, list_temp, &filter_list) {
		filter = container_of(list_elem, struct filter_element, list);
		free(filter);
	}

	return ret;
}
