// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>
//

#include "dsm_api_public.h"

enum DSM_API_MESSAGE dsm_api_get_mem(struct dsm_api_memory_size_ext_t *iopmmemparam,
				     int iparamsize)
{
	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_init(void *ipmodulehandler,
				  struct dsm_api_init_ext_t *iopparamstruct,
				  int iparamsize)
{
	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_ff_process(void *ipmodulehandler,
					int channelmask,
					short *ibufferorg,
					int *ipnrsamples,
					short *obufferorg,
					int *opnrsamples)
{
	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_fb_process(void *ipmodulehandler,
					int ichannelmask,
					short *icurrbuffer,
					short *ivoltbuffer,
					int *iopnrsamples)
{
	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_set_params(void *ipmodulehandler,
					int icommandnumber, void *ipparamsbuffer)
{
	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_get_params(void *ipmodulehandler,
					int icommandnumber, void *opparams)
{
	return DSM_API_OK;
}
