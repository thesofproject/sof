// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Jaska Uimonen <jaska.uimonen@linux.intel.com>

#include <stdio.h>
#include "ssp.h"

int print_ssp_blob_decode(uint8_t *blob, uint32_t len)
{
	const uint32_t *p = (uint32_t *)blob;

	printf("ssp blob parameters:\n");
	printf("gateway_attributes 0x%08x\n", *p);
	p++;
	printf("ts_group 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
	       *p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));
	p += 8;
	printf("ssc0 0x%08x\n", *p);
	p++;
	printf("ssc1 0x%08x\n", *p);
	p++;
	printf("sscto 0x%08x\n", *p);
	p++;
	printf("sspsp 0x%08x\n", *p);
	p++;
	printf("sstsa 0x%08x\n", *p);
	p++;
	printf("ssrsa 0x%08x\n", *p);
	p++;
	printf("ssc2 0x%08x\n", *p);
	p++;
	printf("sspsp2 0x%08x\n", *p);
	p++;
	printf("ssc3 0x%08x\n", *p);
	p++;
	printf("ssioc 0x%08x\n", *p);
	p++;
	printf("mdivc 0x%08x\n", *p);
	p++;
	printf("mdivr  0x%08x\n\n", *p);

	return 0;
}
