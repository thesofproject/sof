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
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#include <stdint.h>
#include <stdlib.h>
#include <sof/sof.h>
#include <sof/alloc.h>
#include <sof/trace.h>
#include <sof/ipc.h>
#include <sof/module.h>
#include <sof/list.h>

#define trace_module(__e)	trace_event_atomic(TRACE_CLASS_MODULE, __e)
#define trace_module_value(__e)	trace_value_atomic(__e)
#define trace_module_error(__e)	trace_error(TRACE_CLASS_MODULE, __e)

#define PLATFORM_HOST_DMAC 0

/* size in 4k pages */
#define MODULE_PAGE_TABLE_SIZE	32

static int copy_module(struct sof_module *smod, struct ipc_module_new *ipc_mod)
{
	struct dma_copy dc;
	uint8_t *page_table;
	struct dma_sg_config sg_config;
#ifdef CONFIG_HOST_PTABLE
	struct list_item elem_list;
#endif
	struct dma_sg_elem elem;
	int ret;

	/* validate */
	if (ipc_mod->buffer.pages > MODULE_PAGE_TABLE_SIZE) {
		trace_module_error("me0");
		return NULL;
	}

	/* allocate page table */
	page_table = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
			     MODULE_PAGE_TABLE_SIZE);
	if (!page_table) {
		trace_module_error("mc1");
		return NULL;
	}

	/* allocate ELF data - freed as not needed after relocations */
	smod->elf = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM,
			      ipc_mod->buffer.size);
	if (!smod) {
		trace_module_error("mc2");
		goto mod_buf_err;
	}

	/* get DMAC for copy */
	ret = dma_copy_new(&dc, PLATFORM_HOST_DMAC);
	if (ret < 0) {
		trace_module_error("mc3");
		goto dma_cp_err;
	}

#ifdef CONFIG_HOST_PTABLE

	list_init(&elem_list);

	/* use DMA to read in compressed page table ringbuffer from host */
	ret = ipc_get_page_descriptors(dc.dmac, page_table,
				       &ipc_mod->buffer);
	if (ret < 0) {
		trace_ipc_error("mc4");
		goto dma_cp_err;
	}

	ret = ipc_parse_page_descriptors(page_table, &ipc_mod->buffer,
		&elem_list, SOF_IPC_STREAM_PLAYBACK);
	if (ret) {
		trace_ipc_error("mc5");
		goto dma_cp_err;
	}
#else

	ret = dma_copy_set_stream_tag(&dc, ipc_mod->stream_tag);
	if (ret < 0) {
		trace_ipc_error("mc6");
		goto dma_cp_err;
	}

#endif

	/* set up DMA configuration */
	list_item_prepend(&elem.list, &sg_config.elem_list);

	/* copy module from host */
	ret = dma_copy_from_host(&dc, &sg_config, 0, smod->elf,
				 ipc_mod->buffer.size);
	if (ret < 0) {
		trace_ipc_error("mc7");
		goto dma_cp_err;
	}

	/* write data back to memory */
	dcache_writeback_region(smod->elf, ipc_mod->buffer.size);
	rfree(page_table);
	return 0;

dma_cp_err:
	rfree(smod->elf);
mod_buf_err:
	rfree(page_table);
	return ret;
}

static int relocate_module(struct sof_module *smod,
	struct ipc_module_new *ipc_mod)
{
	int ret;

	/* parse module sections and get text, data, bss sizes */
	ret = arch_elf_parse_sections(smod);
	if (ret < 0) {
		trace_ipc_error("mr0");
		return ret;
	}

	/* allocate memory for text section */
	smod->text = rmalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM |
			     SOF_MEM_CAPS_CACHE | SOF_MEM_CAPS_EXEC,
			     smod->text_bytes);
	if (!smod->text) {
		trace_ipc_error("mr1");
		return -ENOMEM;
	}

	/* allocate memory for data section */
	smod->data = rmalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM |
			     SOF_MEM_CAPS_CACHE,
			     smod->data_bytes);
	if (!smod->text) {
		trace_ipc_error("mr2");
		rfree(smod->text);
		return -ENOMEM;
	}

	/* allocate memory for bss section */
	smod->bss = rmalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM |
			     SOF_MEM_CAPS_CACHE,
			     smod->bss_bytes);
	if (!smod->text) {
		trace_ipc_error("mr3");
		rfree(smod->text);
		rfree(smod->data);
		return -ENOMEM;
	}

	/* relocate module */
	ret = arch_elf_reloc_sections(smod);
	if (ret < 0) {
		trace_ipc_error("mr4");
		rfree(smod->text);
		rfree(smod->data);
		rfree(smod->bss);
		return ret;
	}

	return 0;
}

struct sof_module *module_init(struct sof *sof, struct ipc_module_new *ipc_mod)
{
	struct sof_module *smod;
	int err;

	/* allocate module */
	smod = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(*smod));
	if (!smod) {
		trace_module_error("mi0");
		return NULL;
	}

	/* copy module from host */
	err = copy_module(smod, ipc_mod);
	if (!smod) {
		trace_module_error("mi1");
		goto copy_err;
	}

	/* relocate module */
	err = relocate_module(smod, ipc_mod);
	if (!smod) {
		trace_module_error("mi2");
		goto reloc_err;
	}

	/* probe the module */
	err = smod->drv->init(smod);
	if (err < 0) {
		trace_module_error("mi3");
		goto reloc_err;
	}

	/* compelte init */
	list_item_append(&smod->list, &sof->module_list);
	rfree(smod->elf);

	return smod;

reloc_err:
	rfree(smod->text);
	rfree(smod->data);
	rfree(smod->bss);
copy_err:
	rfree(smod->elf);
	rfree(smod);
	return NULL;
}

int module_remove(struct sof *sof, struct sof_module *module)
{
	int ret;

	ret = module->drv->exit(module);
	list_item_del(&module->list);
	rfree(module);

	return ret;
}
