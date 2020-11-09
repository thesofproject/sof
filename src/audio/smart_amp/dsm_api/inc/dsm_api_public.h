/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Maxim Integrated All rights reserved.
 *
 * Author: Ryan Lee <ryans.lee@maximintegrated.com>
 */
#ifndef _DSM_API_PUBLIC_H_
#define _DSM_API_PUBLIC_H_

#define DSM_CH_MASK(cmd_id)		((cmd_id) & 0x00FFFFFF)
#define DSM_CH1_BITMASK			0x1000000
#define DSM_CH2_BITMASK			0x2000000
#define DSM_SET_STEREO_CMD_ID(cmd_id)	(DSM_CH_MASK(cmd_id) | 0x03000000)
#define DSM_SET_CMD_ID(cmd_id)		DSM_SET_STEREO_CMD_ID(cmd_id)

#define DSM_DEFAULT_NUM_CHANNEL		2
#define DSM_DEFAULT_SAMPLE_RATE		48000
#define DSM_DEFAULT_NUM_EQ		8
#define DSM_DEFAULT_MAX_NUM_PARAM	250

#define DSM_GET_PARAM_SZ_PAYLOAD	(1 + DSM_DEFAULT_NUM_CHANNEL)
#define DSM_SET_PARAM_SZ_PAYLOAD	(DSM_DEFAULT_NUM_CHANNEL * 2 + 1)

#define DSM_API_GET_MAXIMUM_CMD_ID	0

enum dsm_getparam_payload_index {
	DSM_GET_ID_IDX = 0,
	DSM_GET_CH1_IDX,
	DSM_GET_CH2_IDX,
	DSM_GET_IDX_MAX
};

enum dsm_setparam_payload_index {
	DSM_SET_ID_IDX = 0,
	DSM_SET_VALUE_IDX,
	DSM_SET_IDX_MAX
};

struct dsm_api_memory_size_ext_t {
	int isamplingrate;
	int ichannels;
	int *ipcircbuffersizebytes;
	int omemsizerequestedbytes;
	int numeqfilters;
};

struct dsm_api_init_ext_t {
	int isamplingrate;
	int ichannels;
	int off_framesizesamples;
	int ofb_framesizesamples;
	int *ipcircbuffersizebytes;
	int *ipdelayedsamples;
	int isamplebitwidth;
};

enum DSM_API_MESSAGE {
	DSM_API_OK = 0,
	DSM_API_MSG_NULL_MODULE_HANDLER = 1 << 1,
	DSM_API_MSG_NULL_PARAM_POINTER = 1 << 2,
	DSM_API_MSG_NULL_INPUT_BUFFER_POINTER = 1 << 3,
	DSM_API_MSG_NULL_OUTPUT_BUFFER_POINTER = 1 << 4,
	DSM_API_MSG_INVALID_CMD_ID = 1 << 5,
	DSM_API_MSG_INVALID_PARAM = 1 << 6,
	DSM_API_MSG_INVALID_PARAMS_NUM = 1 << 7,
	DSM_API_MSG_INVALID_SAMPLING_RATE = 1 << 8,
	DSM_API_MSG_NOT_IMPLEMENTED = 1 << 9,
	DSM_API_MSG_INVALID_MEMORY = 1 << 10,
	DSM_API_MSG_ZERO_I = 1 << 11,
	DSM_API_MSG_ZERO_V = 1 << 12,
	DSM_API_MSG_MIN_RDC_BEYOND_THRESHOLD = 1 << 13,
	DSM_API_MSG_MAX_RDC_BEYOND_THRESHOLD = 1 << 14,
	DSM_API_MISMATCHED_SETGET_CMD = 1 << 15,
	DSM_API_MSG_IV_DATA_WARNING = 1 << 16,
	DSM_API_MSG_COIL_TEMPERATURE_WARNING = 1 << 17,
	DSM_API_MSG_EXCURSION_WARNING = 1 << 18,
	DSM_API_MSG_WRONG_COMMAND_TYPE = 1 << 19,
	DSM_API_MSG_COMMAND_OBSOLETE = 1 << 20,
	DSM_API_MSG_INSUFFICIENT_INPUT_DATA = 1 << 21,
	DSM_API_MSG_FF_NOT_START = 1 << 22,
	DSM_API_MSG_INVALID
};

#define DSM_API_ADAPTIVE_PARAM_START	0x10
#define DSM_API_ADAPTIVE_PARAM_END	0x14

/*******************************************************************************
 *    dsm_api_get_mem()
 *
 * Description:
 *     This function returns the size of data memory which is required by DSM module
 *     and must be called before any other DSM API functions.
 *     The DSP framework should be responsible for allocating memory for
 *     DSM module.
 *
 * Input:
 *     iparamsize: the size of the data structure dsm_api_memory_size_ext_t.
 *
 * Output:
 *     N/A
 *
 * I/O:
 *     iopMemParam: the address of data structure dsm_api_memory_size_ext_t
 *     which contains input and output arguments.
 *
 * Returns:
 *     DSM_API_OK: successful
 *     Otherwise:  error code
 *
 *******************************************************************************/
enum DSM_API_MESSAGE dsm_api_get_mem(struct dsm_api_memory_size_ext_t *iopmmemparam,
				     int iparamsize);

/*******************************************************************************
 *    dsm_api_init()
 *
 * Description:
 *     This function is used to initialize DSM module and must be called after
 *     dsm_api_get_mem() and before all other DSM API functions.
 *
 * Inputs:
 *     ipmodulehandler:
 *                  the handler of DSM module which is allocated by framework caller.
 *     iparamsize:  the size of the data structure dsm_api_init_ext_t.
 *
 * Outputs:
 *     N/A
 *
 * I/O:
 *     iopparamstruct: the address of a data structure dsm_api_init_ext_t
 *     which contains input and output arguments.
 *
 * Returns:
 *     DSM_API_OK: successful
 *     Otherwise:  error code
 *
 *******************************************************************************/
enum DSM_API_MESSAGE dsm_api_init(void *ipmodulehandler,
				  struct dsm_api_init_ext_t *iopparamstruct,
				  int iparamsize);

/*******************************************************************************
 *    dsm_api_ff_processs()
 *
 * Description:
 *     This function is used to process the input audio PCM DSM data.
 *
 * Inputs:
 *     ipmodulehandler:
 *                    the handler of DSM module which is allocated by framework caller.
 *     ichannelmask: the low 8-bits indicate which channels should be executed.
 *                  0: default channels setting, mono or stereo
 *                  1: L channel
 *                  2: R channel
 *                  3: L and R channels
 *                 -1: place input L channel onto output R channel
 *                 -2: place input R channel onto output L channel
 *                 -3: switch L and R channel
 *     ibufferorg:    the input buffer which contains 16-bit audio PCM input data.
 *                 The multi-channel input PCM data are ordered in the format below:
 *                 +-----------------------+    ...    +-------------------------+
 *                 |    one frame samples  |    ...    |   one frame samples     |
 *                 +-----------------------+    ...    +-------------------------+
 *                      1st channel             ...           N-th channel
 *
 * Outputs:
 *     obufferorg: the output buffer which contains the 16-bit PCM data processed by DSM.
 *		   The multi-channel output PCM data are ordered in the format below:
 *                 +-----------------------+    ...    +-------------------------+
 *                 |    one frame samples  |    ...    |   one frame samples     |
 *                 +-----------------------+    ...    +-------------------------+
 *                      1st channel             ...           N-th channel
 *     opnrsamples: the address of a variable which contains the number of samples
 *                  in output buffer.
 *
 * I/O:
 *     ipnrsamples: the number of audio PCM samples to be processed, in 32-bit
 *                   long integer.
 *                       The returned value indicates how many samples of input data are
 *                   not used in the input buffer. In this case, DSP framework should
 *                   send back the remaining data in next process.
 *
 * Returns:
 *     DSM_API_OK: successful
 *     Otherwise:  error codes
 *
 *******************************************************************************/
enum DSM_API_MESSAGE dsm_api_ff_process(void *ipmodulehandler,
					int channelmask,
					short *ibufferorg,
					int *ipnrsamples,
					short *obufferorg,
					int *opnrsamples);

/*******************************************************************************
 *    dsm_api_fb_processs()
 *
 * Description:
 *     This function is used to process current(I) and voltage(V) feedback data.
 *
 * Inputs:
 *     ipmodulehandler:
 *                    the handler of DSM module which is allocated by framework caller.
 *     ichannelmask:   the low 8-bits indicate which channels should be executed.
 *                  0: default channels setting, mono or stereo
 *                  1: L channel
 *                  2: R channel
 *                  3: L and R channels
 *                 -1: place input L channel onto output R channel
 *                 -2: place input R channel onto output L channel
 *                 -3: switch L and R channel
 *     icurrbuffer:   the input buffer which contains I data.
 *                 The multi-channel I data are ordered in the format below:
 *                 +-----------------------+    ...    +-------------------------+
 *                 |    one frame samples  |    ...    |   one frame samples     |
 *                 +-----------------------+    ...    +-------------------------+
 *                      1st channel             ...           N-th channel
 *     ivoltbuffer:   the input buffer which contains V data.
 *                 The multi-channel V data are ordered in the format below:
 *                 +-----------------------+    ...    +-------------------------+
 *                 |    one frame samples  |    ...    |   one frame samples     |
 *                 +-----------------------+    ...    +-------------------------+
 *                      1st channel             ...           N-th channel
 *
 * I/O:
 *     iopnrsamples:  the address of a variable which contains number of I/V data to
 *                    be processed.
 *                        The returned value indicates how many samples of I/V data are
 *                    not used in the I/V buffer. In this case, DSP framework should send
 *                    back the remaining I and V data in next process.
 *
 * Outputs:
 *     N/A
 *
 * Returns:
 *     DSM_API_OK: successful
 *     Otherwise:  error code
 *
 *******************************************************************************/
enum DSM_API_MESSAGE dsm_api_fb_process(void *ipmodulehandler,
					int ichannelmask,
					short *icurrbuffer,
					short *ivoltbuffer,
					int *iopnrsamples);

/*******************************************************************************
 *    dsm_api_set_params()
 *
 * Description:
 *     This function is used to set a serial of DSM parameters in one call.
 *
 * Inputs:
 *     ipmodulehandler:
 *               the handler of DSM module which is allocated by framework caller.
 *     icommandnumber:   the number of commands. The total memory size of this
 *               input argument
 *                    = (cmdNum * 2) * sizeof(int) bytes.
 *     ipparamsbuffer: the buffer of input parameters which are stored in the format below.
 *               The parameters should be set separately for each channel.
 *               ipValue memory map:
 *               ---+-----------+-------------------
 *                  |   cmd_1   |  32-bits integer command
 *                  +-----------+
 *                  |  param_1  |  32-bits data
 *               ---+-----------+-------------------
 *                  |    ...    |
 *                  |    ...    |
 *                  |    ...    |
 *               ---+-----------+-------------------
 *                  |   cmd_N   |  32-bits integer command
 *                  +-----------+
 *                  |  param_N  |  N-th 32-bit data
 *               ---+-----------+-------------------
 *            Total: N parameters
 *
 * Outputs:
 *     N/A.
 *
 * Returns:
 *     DSM_API_OK: successful
 *     Otherwise:  error code
 *
 *******************************************************************************/
enum DSM_API_MESSAGE dsm_api_set_params(void *ipmodulehandler,
					int icommandnumber, void *ipparamsbuffer);

/*******************************************************************************
 *    dsm_api_get_paramss()
 *
 * Description:
 *     This function is used to get a serial of DSM parameters in one call.
 *
 * Inputs:
 *     ipmodulehandler:
 *              the handler of DSM module which is allocated by framework caller.
 *     icommandnumber:   the number of parameters. The total memory size of this
 *               input argument
 *                    = cmdNum * (1 + channel_number) ) * sizeof(int) bytes
 *
 * Outputs:
 *     opparams: the buffer of the output parameters which are stored in the format below.
 *               opParams memory map:
 *             -----+-------------+-----------------------------
 *                  |    cmd_1    |  32-bits integer command Id
 *                  +-------------+
 *                  |  param_1_1  |  32-bits data of channel 1\
 *                  +-------------+                            \
 *                  +    ......   +                             -> parameters of N channels
 *                  +-------------+                            /
 *                  |  param_1_C  |  32-bits data of channel N/
 *             -----+-------------+-----------------------------
 *                  +    ......   +
 *                  +    ......   +
 *                  +    ......   +
 *             -----+-------------+-----------------------------
 *                  +    cmd_M    +  32-bits integer command Id
 *                  +-------------+
 *                  |  param_M_1  |  32-bits data of channel 1\
 *                  +-------------+                            \
 *                  +    ......   +                             -> parameters of N channels
 *                  +-------------+                            /
 *                  |  param_M_C  |  32-bits data of channel N/
 *             -----+-------------+------------------------------
 *            Total: M parameters of N channels
 *
 *               Exeption: the parameter sizes of the commands
 *		 DSM_API_GET_FIRMWARE_BUILD_TIME      = 88, //C string
 *		 DSM_API_GET_FIRMWARE_BUILD_DATE      = 89, //C string
 *		 DSM_API_GET_FIRMWARE_VERSION         = 90, //C string
 *		 DSM_API_GET_CHIPSET_MODEL            = 91, //C string
 *		 are DSM_MAX_STRING_PARAM_SIZE (=32) bytes rather than 4 bytes of
 *		 32-bits data because these parameters are in the format C string.
 *
 * Returns:
 *     DSM_API_OK: successful
 *     Otherwise:  error code
 *
 *******************************************************************************/
enum DSM_API_MESSAGE dsm_api_get_params(void *ipmodulehandler,
					int icommandnumber, void *opparams);

#endif
