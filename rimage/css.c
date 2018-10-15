/*
 * Copyright (c) 2017, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 *  Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *          Keyon Jie <yang.jie@linux.intel.com>
 */

#include <stdio.h>
#include <sys/time.h>
#include "rimage.h"
#include "css.h"
#include "manifest.h"

void ri_css_hdr_create(struct image *image)
{
	struct css_header_v1_8 *css = image->fw_image + MAN_CSS_HDR_OFFSET;
	struct tm *date;
	struct timeval tv;
	int val;

	fprintf(stdout, " cse: completing CSS manifest\n");

	/* get local time and date */
	gettimeofday(&tv, NULL);
	date = localtime(&tv.tv_sec);

	if (!date) {
		fprintf(stderr, "error: cant get localtime %d\n", -errno);
		return;
	}

	date->tm_year += 1900;
	fprintf(stdout, " css: set build date to %d:%2.2d:%2.2d\n",
		date->tm_year, date->tm_mon, date->tm_mday);

	/* year yYyy */
	val = date->tm_year / 1000;
	css->date |= val  << 28;
	date->tm_year -= val * 1000;
	/* year yyYy */
	val = date->tm_year / 100;
	css->date |= val << 24;
	date->tm_year -= val * 100;
	/* year yyyY */
	val = date->tm_year / 10;
	css->date |= val << 20;
	date->tm_year -= val * 10;
	/* year Yyyy */
	val = date->tm_year;
	css->date |= val << 16;

	/* month Mm - for some reason month starts at 0 */
	val = ++date->tm_mon / 10;
	css->date |= val << 12;
	date->tm_mon -= (val * 10);
	/* month mM */
	val = date->tm_mon;
	css->date |= val << 8;

	/* Day Dd */
	val = date->tm_mday / 10;
	css->date |= val << 4;
	date->tm_mday -= (val * 10);
	/* Day dD */
	val = date->tm_mday;
	css->date |= val << 0;
}
