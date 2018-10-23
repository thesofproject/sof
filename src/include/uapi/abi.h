/*
 * Copyright (c) 2016, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

/**
 * \file include/uapi/abi.h
 * \brief ABI definitions
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_ABI_H__
#define __INCLUDE_UAPI_ABI_H__

/** \brief SOF ABI version number. */
#define SOF_ABI_VER(major, minor, micro) \
	(((major) << 8) | ((minor) << 4) | (micro))
#define SOF_ABI_VERSION_MAJOR(version)	(((version) >> 8) & 0xff)
#define SOF_ABI_VERSION_MINOR(version)	(((version) >> 4) & 0xf)
#define SOF_ABI_VERSION_MICRO(version)	((version) & 0xf)
#define SOF_ABI_VERSION_INCOMPATIBLE(sof_ver, client_ver)		\
	(SOF_ABI_VERSION_MAJOR((sof_ver)) !=				\
		SOF_ABI_VERSION_MAJOR((client_ver)) ||			\
		(							\
			SOF_ABI_VERSION_MAJOR((sof_ver)) ==		\
				SOF_ABI_VERSION_MAJOR((client_ver)) &&	\
			SOF_ABI_VERSION_MINOR((sof_ver)) !=		\
				SOF_ABI_VERSION_MINOR((client_ver))	\
		)							\
	)

#define SOF_ABI_MAJOR 1
#define SOF_ABI_MINOR 0
#define SOF_ABI_MICRO 0

#define SOF_ABI_VERSION SOF_ABI_VER(SOF_ABI_MAJOR, SOF_ABI_MINOR, SOF_ABI_MICRO)

/** \brief SOF ABI magic number "SOF\0". */
#define SOF_ABI_MAGIC		0x00464F53

/**
 * \brief Header for all non IPC ABI data.
 *
 * Identifies data type, size and ABI.
 * Used by any bespoke component data structures or binary blobs.
 */
struct sof_abi_hdr {
	uint32_t magic;		/**< 'S', 'O', 'F', '\0' */
	uint32_t type;		/**< component specific type */
	uint32_t size;		/**< size in bytes of data excl. this struct */
	uint32_t abi;		/**< SOF ABI version */
	uint32_t comp_abi;	/**< component specific ABI version */
	char data[0];		/**< data */
}  __attribute__((packed));

#endif

