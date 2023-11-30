/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 *
 * \file common.h
 * \brief Extract from generic.h definitions that need to be included
 *        in intel module adapter code.
 * \author Jaroslaw Stelter <jaroslaw.stelter@intel.com>
 *
 */

#ifndef __SOF_MODULE_INTERFACE__
#define __SOF_MODULE_INTERFACE__

/**
 * \enum module_cfg_fragment_position
 * \brief Fragment position in config
 * MODULE_CFG_FRAGMENT_FIRST: first fragment of the large configuration
 * MODULE_CFG_FRAGMENT_SINGLE: only fragment of the configuration
 * MODULE_CFG_FRAGMENT_LAST: last fragment of the configuration
 * MODULE_CFG_FRAGMENT_MIDDLE: intermediate fragment of the large configuration
 */


struct comp_dev;
struct timestamp_data;
/**
 * \struct module_endpoint_ops
 * \brief Ops relevant only for the endpoint devices such as the host copier or DAI copier.
 *	  Other modules should not implement these.
 */
struct module_endpoint_ops {
	/**
	 * Returns total data processed in number bytes.
	 * @param dev Component device
	 * @param stream_no Index of input/output stream
	 * @param input Selects between input (true) or output (false) stream direction
	 * @return total data processed if succeeded, 0 otherwise.
	 */
	uint64_t (*get_total_data_processed)(struct comp_dev *dev, uint32_t stream_no, bool input);
	/**
	 * Retrieves component rendering position.
	 * @param dev Component device.
	 * @param posn Receives reported position.
	 */
	int (*position)(struct comp_dev *dev, struct sof_ipc_stream_posn *posn);
	/**
	 * Configures timestamping in attached DAI.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_config)(struct comp_dev *dev);

	/**
	 * Starts timestamping.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_start)(struct comp_dev *dev);

	/**
	 * Stops timestamping.
	 * @param dev Component device.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_stop)(struct comp_dev *dev);

	/**
	 * Gets timestamp.
	 * @param dev Component device.
	 * @param tsd Receives timestamp data.
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_ts_get)(struct comp_dev *dev, struct timestamp_data *tsd);

	/**
	 * Fetches hardware stream parameters.
	 * @param dev Component device.
	 * @param params Receives copy of stream parameters retrieved from
	 *	DAI hw settings.
	 * @param dir Stream direction (see enum sof_ipc_stream_direction).
	 *
	 * Mandatory for components that allocate DAI.
	 */
	int (*dai_get_hw_params)(struct comp_dev *dev,
				 struct sof_ipc_stream_params *params, int dir);

	/**
	 * Triggers device state.
	 * @param dev Component device.
	 * @param cmd Trigger command.
	 */
	int (*trigger)(struct comp_dev *dev, int cmd);
};

struct processing_module;


/* Convert first_block/last_block indicator to fragment position */
static inline enum module_cfg_fragment_position
first_last_block_to_frag_pos(bool first_block, bool last_block)
{
	int frag_position = (last_block << 1) | first_block;

	return (enum module_cfg_fragment_position)frag_position;
}

#endif /* __SOF_MODULE_INTERFACE__ */
