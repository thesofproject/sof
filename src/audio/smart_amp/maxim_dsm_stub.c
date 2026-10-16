// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>
//

/* Stub implementation of the Maxim DSM API.
 *
 * The upstream stub returns DSM_API_OK from every function without writing any
 * output field, which compiles but cannot run: with omemsizerequestedbytes left
 * unset the handle size comes out 0, smart_amp_alloc_mod_memblk() skips the
 * allocation, dsmhandle stays NULL and maxim_dsm_init() returns -EINVAL, so
 * widget setup and card registration fail.
 *
 * This stub writes every field the caller reads back, and passes feed-forward
 * audio straight through, so the glue layer, topology and blob layout can be
 * exercised without libdsm.a.
 */

#include "dsm_api_public.h"

/* must match DSM_FRM_SZ in smart_amp_maxim_dsm.c, which hard-codes it in the
 * ff/fb interleave loops; a mismatch misaligns the samples
 */
#define DSM_STUB_FRAME_SZ_SAMPLES	48

/* any non-zero value works -- the stub never touches this memory; 4 KB also
 * exercises the PRIVATE memblk allocation path
 */
#define DSM_STUB_HANDLE_SZ_BYTES	4096

/* must exceed DSM_API_ADAPTIVE_PARAM_END, which
 * maxim_dsm_get_volatile_param() iterates up to, or caldata overflows
 */
#define DSM_STUB_MAX_NUM_PARAM		DSM_DEFAULT_MAX_NUM_PARAM

enum DSM_API_MESSAGE dsm_api_get_mem(struct dsm_api_memory_size_ext_t *iopmmemparam,
				     int iparamsize)
{
	if (!iopmmemparam)
		return DSM_API_MSG_NULL_PARAM_POINTER;
	if (iparamsize != (int)sizeof(*iopmmemparam))
		return DSM_API_MSG_INVALID_PARAM;

	iopmmemparam->omemsizerequestedbytes = DSM_STUB_HANDLE_SZ_BYTES;

	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_init(void *ipmodulehandler,
				  struct dsm_api_init_ext_t *iopparamstruct,
				  int iparamsize)
{
	if (!ipmodulehandler)
		return DSM_API_MSG_NULL_MODULE_HANDLER;
	if (!iopparamstruct)
		return DSM_API_MSG_NULL_PARAM_POINTER;
	if (iparamsize != (int)sizeof(*iopparamstruct))
		return DSM_API_MSG_INVALID_PARAM;

	/* Caller uses these two fields to compute ifsamples / ibsamples. */
	iopparamstruct->off_framesizesamples = DSM_STUB_FRAME_SZ_SAMPLES;
	iopparamstruct->ofb_framesizesamples = DSM_STUB_FRAME_SZ_SAMPLES;

	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_ff_process(void *ipmodulehandler, int channelmask,
					short *ibufferorg, int *ipnrsamples,
					short *obufferorg, int *opnrsamples)
{
	int nsamples;
	int idx;

	if (!ipmodulehandler)
		return DSM_API_MSG_NULL_MODULE_HANDLER;
	if (!ibufferorg || !ipnrsamples)
		return DSM_API_MSG_NULL_INPUT_BUFFER_POINTER;
	if (!obufferorg || !opnrsamples)
		return DSM_API_MSG_NULL_OUTPUT_BUFFER_POINTER;

	/* Pass-through: copy input to output unchanged so audio is audible under the stub. */
	nsamples = *ipnrsamples;
	for (idx = 0; idx < nsamples; idx++)
		obufferorg[idx] = ibufferorg[idx];

	*opnrsamples = nsamples;

	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_fb_process(void *ipmodulehandler, int ichannelmask,
					short *icurrbuffer, short *ivoltbuffer,
					int *iopnrsamples)
{
	if (!ipmodulehandler)
		return DSM_API_MSG_NULL_MODULE_HANDLER;
	if (!icurrbuffer || !ivoltbuffer || !iopnrsamples)
		return DSM_API_MSG_NULL_INPUT_BUFFER_POINTER;

	/* Stub does no adaptation; IV feedback data is silently discarded. */

	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_set_params(void *ipmodulehandler, int icommandnumber,
					void *ipparamsbuffer)
{
	if (!ipmodulehandler)
		return DSM_API_MSG_NULL_MODULE_HANDLER;
	if (!ipparamsbuffer)
		return DSM_API_MSG_NULL_PARAM_POINTER;

	/* Stub has no internal state; written tuning parameters are discarded. */

	return DSM_API_OK;
}

enum DSM_API_MESSAGE dsm_api_get_params(void *ipmodulehandler, int icommandnumber,
					void *opparams)
{
	int *cmdblock = (int *)opparams;

	if (!ipmodulehandler)
		return DSM_API_MSG_NULL_MODULE_HANDLER;
	if (!cmdblock)
		return DSM_API_MSG_NULL_PARAM_POINTER;

	/* The upstream stub leaves CH1/CH2 unwritten and the caller reads
	 * uninitialised stack data -- maxim_dsm_get_num_param() sizes the
	 * caldata allocation from it.
	 */
	if (DSM_CH_MASK(cmdblock[DSM_GET_ID_IDX]) == DSM_API_GET_MAXIMUM_CMD_ID) {
		cmdblock[DSM_GET_CH1_IDX] = DSM_STUB_MAX_NUM_PARAM;
		cmdblock[DSM_GET_CH2_IDX] = DSM_STUB_MAX_NUM_PARAM;
	} else {
		cmdblock[DSM_GET_CH1_IDX] = 0;
		cmdblock[DSM_GET_CH2_IDX] = 0;
	}

	return DSM_API_OK;
}
