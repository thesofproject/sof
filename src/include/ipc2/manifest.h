/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020 Intel Corporation. All rights reserved.
 *
 */

#ifndef SRC_INCLUDE_IPC2_MANIFEST_H_
#define SRC_INCLUDE_IPC2_MANIFEST_H_

#include <stdint.h>
#include <kernel/ext_manifest.h>

/*
 * IPC2 uses the extended manifest to tell the kernel about the supported
 * firmware features and IPC ABI. This way the kernel knows exactly how to
 * communicate directly with firmware for each feature and use case for
 * each version of firmware.
 *
 * The extended manifest contains the following IPC2 information that is
 * used by the drivers
 *
 * 1) A list of features classes - this is used by the core driver to
 *    enumerate feature drivers like audio, sensing, shell, debug, etc.
 *    i.e. if feature class is listed in the manifest then the core driver
 *    should enumerate that feature driver. The feature driver can then
 *    parse the manifest data relevant to itself.
 *
 * 2) A list of sub classes per feature - this is used by the feature driver
 *    to determine the feature sub classes supported by the firmware. e.g for
 *    audio, the feature driver sub classes could be PCMs, controls, media
 *    decode, WoV, etc.
 *
 * 3) A list of actions per sub class - this is used by the feature driver to
 *    determine the ABI used for each sub class action. This includes
 *    mandatory and optional parameters for each sub class action so that
 *    feature driver can build the correct IPC per ABI version.
 */

/*
 * IPC2 Action
 *
 * Contains a list of optional and mandatory tuples needed by the driver
 * to perform this action. Tuples are uint16_t. Structure is aligned on
 * word.
 */
struct sof_ipc2_action {
	uint32_t action_id;		/**< action ID - maps to IPC message action */
	uint32_t action_size;		/**< action size in bytes*/
	uint16_t num_mandatory;		/**< number of mandatory tuple IDs */
	uint16_t num_optional;		/**< number of optional tuple IDs */

	/* tuple IDs follow here - mandatory then optional */
	uint16_t tuple[0];		/**< size is num_mandatory + num_optional */
} __attribute__((packed));


/*
 * IPC2 Feature Sub Class - (spelled with a K for C++ users)
 *
 * Describes the capabilities of a use case within a feature driver.
 */
struct sof_ipc2_subclass {
	uint32_t subclass_id;		/**< subclass ID - use to map feature use case */
	uint32_t subclass_size;		/**< subclass size in bytes */
	uint32_t num_actions;		/**< number of actions in this subclass */

	/* action data follows here */
	struct sof_ipc2_action actions[0];	/**< actions - size is num_actions */
} __attribute__((packed));

/*
 * IPC2 Feature Class - (spelled with a K for C++ users)
 *
 * Describes the capabilities of a feature driver.
 */
struct sof_ipc2_klass {
	uint32_t klass_id;		/**< class ID - used to match feature driver */
	uint32_t klass_size;		/**< class size in bytes */
	uint32_t num_subklasses;	/**< number of subclasses in this class */

	/* sub class data follows here */
	struct sof_ipc2_subclass subklass[0];	/*<< subklasses - size is num_subclasses */
} __attribute__((packed));


/*
 * IPC2 manifest header.
 *
 * Identifies this data as IPC2 extended manifest and first feature class
 * follows the end of the structure.
 */
struct sof_ipc2_manifest_hdr {
	struct ext_man_elem_header hdr;	/**< extended manifest header for IPC2 */
	uint32_t num_klasses;		/**< number of classes in manifest */

	/* class data follows here */
	struct sof_ipc2_klass klass[0];	/**< klasses - size is num_klasses */
} __attribute__((packed));

#endif
