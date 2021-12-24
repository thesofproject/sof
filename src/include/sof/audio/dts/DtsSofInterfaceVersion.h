// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Xperi. All rights reserved.
//
// Author: Mark Barton <mark.barton@xperi.com>
#ifndef __SOF_AUDIO_DTS_SOF_INTERFACE_VERSION_H__
#define __SOF_AUDIO_DTS_SOF_INTERFACE_VERSION_H__

#define DTS_SOF_INTERFACE_PRODUCT_NAME "DtsSofInterface"
#define DTS_SOF_INTERFACE_PRODUCT_VERSION_MAJOR 1
#define DTS_SOF_INTERFACE_PRODUCT_VERSION_MINOR 0
#define DTS_SOF_INTERFACE_PRODUCT_VERSION_PATCH 1
#define DTS_SOF_INTERFACE_PRODUCT_VERSION_BUILD 0
#define DTS_SOF_INTERFACE_PRODUCT_VERSION_EXTRA "Dev"

#if defined(__cplusplus)
extern "C" {
#endif

/** Version information for the library.
 */
typedef struct DtsSofInterfaceVersionInfo
{
	/** Name of product.
	 */
	const char* product;
	/** Major version number.
	 */
	int major;
	/** Minor version number.
	 */
	int minor;
	/** Patch number.
	 */
	int patch;
	/** Build number.
	 */
	int build;
} DtsSofInterfaceVersionInfo;

#if defined(__cplusplus)
}
#endif

#endif //  __SOF_AUDIO_DTS_SOF_INTERFACE_VERSION_H__
