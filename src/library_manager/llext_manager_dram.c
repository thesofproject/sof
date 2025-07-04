// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

#include <rtos/alloc.h>

#include <sof/lib_manager.h>
#include <ipc/topology.h>

#include <zephyr/llext/buf_loader.h>
#include <zephyr/llext/llext.h>
#include <zephyr/llext/llext_internal.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_DECLARE(lib_manager, CONFIG_SOF_LOG_LEVEL);

struct lib_manager_dram_storage {
	struct ext_library ext_lib;
	struct lib_manager_mod_ctx *ctx;
	struct lib_manager_module *mod;
	struct llext *llext;
	struct llext_buf_loader *bldr;
	struct llext_elf_sect_map *sect;
	struct llext_symbol *sym;
	unsigned int n_llext;
};

/*
 * This holds the complete LLEXT manager context in DRAM over DSP shut down to
 * be restored during the next boot
 */
__imrdata static struct lib_manager_dram_storage lib_manager_dram;

/* Store LLEXT manager context in DRAM to be restored during the next boot. */
int llext_manager_store_to_dram(void)
{
	struct ext_library *_ext_lib = ext_lib_get();
	unsigned int i, j, k, l, n_lib, n_mod, n_llext, n_sect, n_sym;
	size_t buf_size;

	if (lib_manager_dram.n_llext) {
		tr_err(&lib_manager_tr, "context already saved");
		return 0;
	}

	/*
	 * Count libraries, modules, instantiated extensions, sections and exported
	 * symbols in them. Allocate a buffer of required size.
	 */
	lib_manager_dram.ext_lib = *_ext_lib;
	for (i = 0, n_lib = 0, n_mod = 0, n_llext = 0, n_sect = 0, n_sym = 0;
	     i < ARRAY_SIZE(_ext_lib->desc); i++)
		if (_ext_lib->desc[i]) {
			n_lib++;
			n_mod += _ext_lib->desc[i]->n_mod;
			for (k = 0; k < _ext_lib->desc[i]->n_mod; k++)
				if (_ext_lib->desc[i]->mod[k].ebl) {
					n_llext++;
					n_sect += _ext_lib->desc[i]->mod[k].llext->sect_cnt;
					n_sym += _ext_lib->desc[i]->mod[k].llext->exp_tab.sym_cnt;
					tr_dbg(&lib_manager_tr, "add %u exported syms",
					       _ext_lib->desc[i]->mod[k].llext->exp_tab.sym_cnt);
				}
		}

	buf_size = sizeof(lib_manager_dram.ctx[0]) * n_lib +
		sizeof(lib_manager_dram.mod[0]) * n_mod +
		sizeof(lib_manager_dram.sect[0]) * n_sect +
		sizeof(lib_manager_dram.sym[0]) * n_sym +
		(sizeof(lib_manager_dram.llext[0]) + sizeof(lib_manager_dram.bldr[0])) * n_llext;

	lib_manager_dram.ctx = rmalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_L3,
				       buf_size);
	if (!lib_manager_dram.ctx)
		return -ENOMEM;

	/* Save pointers to buffer parts, holding parts of the context */
	lib_manager_dram.mod = (struct lib_manager_module *)(lib_manager_dram.ctx + n_lib);
	lib_manager_dram.sect = (struct llext_elf_sect_map *)(lib_manager_dram.mod + n_mod);
	lib_manager_dram.llext = (struct llext *)(lib_manager_dram.sect + n_sect);
	lib_manager_dram.bldr = (struct llext_buf_loader *)(lib_manager_dram.llext + n_llext);
	lib_manager_dram.sym = (struct llext_symbol *)(lib_manager_dram.bldr + n_llext);

	tr_dbg(&lib_manager_tr, "backup %u libs of %u modules with %u LLEXT with %u sections",
	       n_lib, n_mod, n_llext, n_sect);

	tr_dbg(&lib_manager_tr, "backup %p to %p, mod %p, loader %p",
	       lib_manager_dram.ctx, (void *)((uint8_t *)lib_manager_dram.ctx + buf_size),
	       lib_manager_dram.mod, lib_manager_dram.bldr);

	/* Walk all libraries */
	for (i = 0, j = 0, l = 0, n_mod = 0, n_sect = 0, n_sym = 0;
	     i < ARRAY_SIZE(_ext_lib->desc); i++) {
		if (!_ext_lib->desc[i])
			continue;

		struct lib_manager_module *mod = _ext_lib->desc[i]->mod;

		/* Copy all modules in each library */
		lib_manager_dram.ctx[j++] = *_ext_lib->desc[i];
		memcpy(lib_manager_dram.mod + n_mod, mod,
		       sizeof(lib_manager_dram.mod[0]) * _ext_lib->desc[i]->n_mod);
		tr_dbg(&lib_manager_tr, "lib %u base %p", j - 1,
		       lib_manager_dram.ctx[j - 1].base_addr);
		n_mod += _ext_lib->desc[i]->n_mod;

		/*
		 * Copy instantiated extensions. Note that only modules, that
		 * were used, have their LLEXT context instantiated.
		 */
		for (k = 0; k < _ext_lib->desc[i]->n_mod; k++) {
			if (!mod[k].llext)
				continue;

			tr_dbg(&lib_manager_tr, "mod %u of %u sections", k,
			       mod[k].llext->sect_cnt);

			/* Copy the extension and the loader */
			lib_manager_dram.llext[l] = *mod[k].llext;
			lib_manager_dram.bldr[l] = *mod[k].ebl;

			/* Copy the section map */
			memcpy(lib_manager_dram.sect + n_sect, mod[k].ebl->loader.sect_map,
			       mod[k].llext->sect_cnt * sizeof(lib_manager_dram.sect[0]));
			n_sect += mod[k].llext->sect_cnt;

			/* Copy exported symbols */
			if (mod[k].llext->exp_tab.sym_cnt) {
				memcpy(lib_manager_dram.sym + n_sym, mod[k].llext->exp_tab.syms,
				       mod[k].llext->exp_tab.sym_cnt *
				       sizeof(lib_manager_dram.sym[0]));
				lib_manager_dram.llext[l].exp_tab.syms = lib_manager_dram.sym +
					n_sym;
				n_sym += mod[k].llext->exp_tab.sym_cnt;
			}

			l++;
		}
	}

	/* Also flatten dependency lists */
	int ret = llext_relink_dependency(lib_manager_dram.llext, n_llext);

	if (ret < 0) {
		tr_err(&lib_manager_tr, "Inconsistent dependencies!");
		return ret;
	}

	lib_manager_dram.n_llext = n_llext;
	/* Make sure, that the data is actually in the DRAM, not just in data cache */
	dcache_writeback_region((__sparse_force void __sparse_cache *)&lib_manager_dram,
				sizeof(lib_manager_dram));
	dcache_writeback_region((__sparse_force void __sparse_cache *)lib_manager_dram.ctx,
				buf_size);

	return 0;
}

int llext_manager_restore_from_dram(void)
{
	lib_manager_init();

	struct ext_library *_ext_lib = ext_lib_get();
	unsigned int i, j, k, n_mod, n_llext, n_sect, n_sym;

	if (!lib_manager_dram.n_llext || !lib_manager_dram.ctx) {
		tr_dbg(&lib_manager_tr, "No modules saved");
		dcache_writeback_region((__sparse_force void __sparse_cache *)&lib_manager_dram,
					sizeof(lib_manager_dram));
		return 0;
	}

	/* arrays of pointers for llext_restore() */
	void **ptr_array = rmalloc(SOF_MEM_FLAG_KERNEL,
				   sizeof(*ptr_array) * lib_manager_dram.n_llext * 2);

	if (!ptr_array)
		return -ENOMEM;

	struct llext_loader **ldr = (struct llext_loader **)ptr_array;
	struct llext **llext = (struct llext **)(ptr_array + lib_manager_dram.n_llext);

	*_ext_lib = lib_manager_dram.ext_lib;

	/* The external loop walks all the libraries */
	for (i = 0, j = 0, n_mod = 0, n_llext = 0, n_sect = 0, n_sym = 0;
	     i < ARRAY_SIZE(_ext_lib->desc); i++) {
		if (!lib_manager_dram.ext_lib.desc[i]) {
			_ext_lib->desc[i] = NULL;
			continue;
		}

		/* Panics on failure - use the same zone as during the first boot */
		struct lib_manager_mod_ctx *ctx = rmalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
							  sizeof(*ctx));

		/* Restore the library context */
		*ctx = lib_manager_dram.ctx[j++];

		/* Allocate and restore all the modules in the library */
		struct lib_manager_module *mod = rmalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
							 ctx->n_mod * sizeof(ctx->mod[0]));

		if (!mod) {
			tr_err(&lib_manager_tr, "module allocation failure");
			goto nomem;
		}
		tr_dbg(&lib_manager_tr, "%u modules alloc %p base %p copy %#zx",
		       ctx->n_mod, (void *)mod, ctx->base_addr, ctx->n_mod * sizeof(ctx->mod[0]));

		memcpy(mod, lib_manager_dram.mod + n_mod, sizeof(mod[0]) * ctx->n_mod);
		n_mod += ctx->n_mod;
		ctx->mod = mod;

		/* Second level: enumerate modules in each library */
		for (k = 0; k < ctx->n_mod; k++) {
			if (!mod[k].llext)
				/* Not instantiated - nothing to restore */
				continue;

			/* Loaders are supplied by the caller */
			struct llext_buf_loader *bldr = rmalloc(SOF_MEM_FLAG_KERNEL | SOF_MEM_FLAG_COHERENT,
								sizeof(*bldr));

			if (!bldr) {
				tr_err(&lib_manager_tr, "loader allocation failure");
				goto nomem;
			}

			/* Extensions have to be restored by Zephyr, collect pointers first */
			llext[n_llext] = lib_manager_dram.llext + n_llext;

			*bldr = lib_manager_dram.bldr[n_llext];

			bldr->loader.sect_map = lib_manager_dram.sect + n_sect;

			n_sect += llext[n_llext]->sect_cnt;
			if (llext[n_llext]->exp_tab.sym_cnt) {
				/*
				 * Just a check, that we're restoring exported
				 * symbols correctly
				 */
				tr_dbg(&lib_manager_tr, "got %u exported symbols",
				       llext[n_llext]->exp_tab.sym_cnt);

				if (llext[n_llext]->exp_tab.syms != lib_manager_dram.sym + n_sym) {
					tr_err(&lib_manager_tr,
					       "bug detected! pointer mismatch %p vs. %p",
					       (void *)llext[n_llext]->exp_tab.syms,
					       (void *)(lib_manager_dram.sym + n_sym));
					goto nomem;
				}

				n_sym += llext[n_llext]->exp_tab.sym_cnt;
			}

			mod[k].ebl = bldr;

			ldr[n_llext++] = &bldr->loader;
		}

		_ext_lib->desc[i] = ctx;
	}

	/* Let Zephyr restore extensions and its own internal bookkeeping */
	int ret = llext_restore(llext, ldr, lib_manager_dram.n_llext);

	if (ret < 0) {
		tr_err(&lib_manager_tr, "Zephyr failed to restore: %d", ret);
		goto nomem;
	}

	/* Rewrite to correct LLEXT pointers, created by Zephyr */
	for (i = 0, n_llext = 0; i < ARRAY_SIZE(_ext_lib->desc); i++) {
		struct lib_manager_mod_ctx *ctx = _ext_lib->desc[i];

		if (!ctx)
			continue;

		struct lib_manager_module *mod = ctx->mod;

		for (k = 0; k < ctx->n_mod; k++) {
			if (mod[k].llext)
				mod[k].llext = llext[n_llext++];
		}
	}

	tr_info(&lib_manager_tr, "restored %u modules with %u LLEXT", n_mod, n_llext);

	rfree(lib_manager_dram.ctx);
	lib_manager_dram.ctx = NULL;
	lib_manager_dram.sect = NULL;
	lib_manager_dram.llext = NULL;
	lib_manager_dram.bldr = NULL;
	lib_manager_dram.sym = NULL;
	rfree(ldr);

	lib_manager_dram.n_llext = 0;

	return 0;

nomem:
	tr_err(&lib_manager_tr, "Restore failed");
	for (i = 0; i < ARRAY_SIZE(_ext_lib->desc); i++) {
		struct lib_manager_mod_ctx *ctx = _ext_lib->desc[i];

		if (!ctx)
			continue;

		struct lib_manager_module *mod = ctx->mod;

		if (!mod)
			continue;

		for (k = 0; k < ctx->n_mod; k++) {
			if (mod[k].llext)
				llext_unload(&mod[k].llext);

			if (mod[k].ebl)
				rfree(mod[k].ebl);
		}

		rfree(mod);
		rfree(ctx);
	}

	/* at least create a sane empty lib-manager context */
	memset(_ext_lib->desc, 0, sizeof(_ext_lib->desc));

	rfree(ldr);

	return -ENOMEM;
}
