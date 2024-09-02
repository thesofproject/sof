// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation.

/* modular: llext dynamic link */

#include <sof/compiler_attributes.h>
#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

#include <stddef.h>

/* 93446e12-1864-4e04-afe0-3b1d778ffb79 */
#define UUID_FIR 0x12, 0x6e, 0x44, 0x93, 0x64, 0x18, 0x04, 0x4e, \
		 0xaf, 0xe0, 0x3b, 0x1d, 0x77, 0x8f, 0xfb, 0x79

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_AUX_MANIFEST("FIR", NULL, UUID_FIR);

SOF_LLEXT_BUILDINFO;
