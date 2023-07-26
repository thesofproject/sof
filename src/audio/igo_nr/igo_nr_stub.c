// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>

#include <sof/audio/igo_nr/igo_nr_comp.h>

enum IgoRet IgoLibGetInfo(struct IgoLibInfo *info)
{
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
