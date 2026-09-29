// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>
//

/* Stub implementation of the Maxim DSM closed source API.
 *
 * The upstream stub returns DSM_API_OK from every function without writing
 * any output fields.  That satisfies the compiler (the Kconfig help text
 * also says "only be used for CI and testing"), but always fails at runtime
 * during comp_new:
 *
 *   dsm_api_get_mem() never writes omemsizerequestedbytes
 *     -> maxim_dsm_get_handle_size() returns 0
 *     -> smart_amp_alloc_mod_memblk(): the `if (ret == 0) return 0` branch
 *        skips both the allocation and the set_memblk() call
 *     -> hspk->dsmhandle remains NULL
 *     -> maxim_dsm_init(): `if (!hspk->dsmhandle) return -EINVAL` fires
 *     -> host sees `ipc tx error for 0x30010000 ... -22`
 *        + `Failed to setup widget SMART_AMP2.0`, card registration fails.
 *
 * This stub fills all output fields read back by the caller so that the
 * smart_amp Maxim glue layer can fully initialise and run the data path.
 * Feed-forward processing is a straight pass-through (input -> output), so
 * audio is still audible under the stub, allowing the glue layer + topology
 * + blob layout to be validated independently from the closed-source
 * libdsm.a itself.
 */

#include "dsm_api_public.h"

/* Must match DSM_FRM_SZ in smart_amp_maxim_dsm.c: that file hard-codes
 * DSM_FRM_SZ samples per frame in its ff/fb interleave/de-interleave loops,
 * and a mismatch here causes sample misalignment.
 */
#define DSM_STUB_FRAME_SZ_SAMPLES	48

/* The real libdsm.a handle size is unknown.  The stub itself does not use
 * this memory; any non-zero value works.  4 KB is chosen to also exercise
 * the PRIVATE memblk allocation path.
 */
#define DSM_STUB_HANDLE_SZ_BYTES	4096

/* maxim_dsm_get_volatile_param() iterates up to DSM_API_ADAPTIVE_PARAM_END
 * (0x14); the parameter count must exceed that value or caldata overflows.
 * Use the maximum value the API allows.
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

	/* Caller places the command id in cmdblock[DSM_GET_ID_IDX]; return
	 * values are read from CH1/CH2.  The upstream stub leaves these fields
	 * unwritten, so the caller reads uninitialised stack data.
	 * maxim_dsm_get_num_param() is especially critical: it uses this result
	 * to size the caldata allocation.
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
