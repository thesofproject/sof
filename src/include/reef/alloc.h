/* 
 * BSD 3 Clause - See LICENCE file for details.
 *
 * Copyright (c) 2015, Intel Corporation
 * All rights reserved.
 *
 */

#ifndef __INCLUDE_ALLOC__
#define __INCLUDE_ALLOC__

#include <string.h>
#include <stdint.h>
#include <reef/dma.h>

/* Heap Memory Zones */
/* device zone - never freed, always suceeds */
#define RZONE_DEV	0
/* module zone - freed on module removal, can fail */
#define RZONE_MODULE	1

/* Modules */
#define RMOD_SYS	0	/* system module */
#define RMOD(m)		(m + 16)	/* all other modules */

struct mm_info {
	uint32_t used;
	uint32_t free;
};

void *rmalloc(int zone, int module, size_t bytes);

/* allocates buffer on 1k boundary */
void *rballoc(int zone, int module, size_t bytes);

void rfree(int zone, int module, void *ptr);

void rbfree(int zone, int module, void *ptr);

void init_heap(void);

void bzero(void *s, size_t n);

void *memset(void *s, int c, size_t n);

struct mm_info *mm_pm_context_info(void);
int mm_pm_context_save(struct dma_sg_config *sg);
int mm_pm_context_restore(struct dma_sg_config *sg);

#endif
