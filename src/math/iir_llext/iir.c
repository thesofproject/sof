// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation.

/* modular: llext dynamic link */

#include <sof/compiler_attributes.h>
#include <sof/lib/uuid.h>
#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

static const struct sof_man_module_manifest mod_manifest[] __section(".module") __used = {
	SOF_LLEXT_AUX_MANIFEST("IIR", NULL, SOF_REG_UUID(iir)),
};

SOF_LLEXT_BUILDINFO;
