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
 *
 * SOF ABI versioning is based on Semantic Versioning where we have a given
 * MAJOR.MINOR.PATCH version number. See https://semver.org/
 *
 * Rules for incrementing or changing version :-
 *
 * 1) Increment MAJOR version if you make incompatible API changes. MINOR and
 *    PATCH should be reset to 0.
 *
 * 2) Increment MINOR version if you add backwards compatible features or
 *    changes. PATCH should be reset to 0.
 *
 * 3) Increment PATCH version if you add backwards compatible bug fixes.
 */

#ifndef __INCLUDE_UAPI_ABI_H__
#define __INCLUDE_UAPI_ABI_H__

/** \brief SOF ABI version major, minor and patch numbers */
#define SOF_ABI_MAJOR 3
#define SOF_ABI_MINOR 8
#define SOF_ABI_PATCH 0

/** \brief SOF ABI version number. Format within 32bit word is MMmmmppp */
#define SOF_ABI_MAJOR_SHIFT	24
#define SOF_ABI_MAJOR_MASK	0xff
#define SOF_ABI_MINOR_SHIFT	12
#define SOF_ABI_MINOR_MASK	0xfff
#define SOF_ABI_PATCH_SHIFT	0
#define SOF_ABI_PATCH_MASK	0xfff

#define SOF_ABI_VER(major, minor, patch) \
	(((major) << SOF_ABI_MAJOR_SHIFT) | \
	((minor) << SOF_ABI_MINOR_SHIFT) | \
	((patch) << SOF_ABI_PATCH_SHIFT))

#define SOF_ABI_VERSION_MAJOR(version) \
	(((version) >> SOF_ABI_MAJOR_SHIFT) & SOF_ABI_MAJOR_MASK)
#define SOF_ABI_VERSION_MINOR(version)	\
	(((version) >> SOF_ABI_MINOR_SHIFT) & SOF_ABI_MINOR_MASK)
#define SOF_ABI_VERSION_PATCH(version)	\
	(((version) >> SOF_ABI_PATCH_SHIFT) & SOF_ABI_PATCH_MASK)

#define SOF_ABI_VERSION_INCOMPATIBLE(sof_ver, client_ver)		\
	(SOF_ABI_VERSION_MAJOR((sof_ver)) !=				\
		SOF_ABI_VERSION_MAJOR((client_ver))			\
	)

#define SOF_ABI_VERSION SOF_ABI_VER(SOF_ABI_MAJOR, SOF_ABI_MINOR, SOF_ABI_PATCH)

/** \brief SOF ABI magic number "SOF\0". */
#define SOF_ABI_MAGIC		0x00464F53

#endif

