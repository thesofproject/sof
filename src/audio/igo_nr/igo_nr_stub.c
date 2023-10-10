// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>

#include <sof/audio/igo_nr/igo_nr_comp.h>

/* Set handle_size to Just something instead of 0 from rzalloc() of
 * component data to avoid rballoc() in igo_nr_init() to fail.
 */
#define IGO_NR_STUB_HANDLE_SIZE		42

enum IgoRet IgoLibGetInfo(struct IgoLibInfo *info)
{
	info->handle_size = IGO_NR_STUB_HANDLE_SIZE;

	return IGO_RET_OK;
}

enum IgoRet IgoLibInit(void *handle,
		       const struct IgoLibConfig *config,
		       void *param)
{
	return IGO_RET_OK;
}

enum IgoRet IgoLibProcess(void *handle,
			  const struct IgoStreamData *in,
			  const struct IgoStreamData *ref,
			  const struct IgoStreamData *out)
{
	return IGO_RET_OK;
}
