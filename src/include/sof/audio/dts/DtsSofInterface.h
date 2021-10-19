// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2021 Xperi. All rights reserved.
//
// Author: Mark Barton <mark.barton@xperi.com>
#ifndef __SOF_AUDIO_DTS_SOF_INTERFACE_H__
#define __SOF_AUDIO_DTS_SOF_INTERFACE_H__

#include "DtsSofInterfaceResult.h"
#include "DtsSofInterfaceVersion.h"

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef _WINDOWS
#define DTS_SOF_INTERFACE_API __declspec(dllexport)
#else
#define DTS_SOF_INTERFACE_API
#endif

#if !defined(DTS_SOF_INTERFACE_NOEXCEPT)
#if (defined(__cplusplus) && __cplusplus >= 201103L) && (defined(_MSC_VER) && _MSC_VER >= 1900 && defined(_CPPUNWIND))
#define DTS_SOF_INTERFACE_NOEXCEPT noexcept
#elif (defined(_MSC_VER) && defined(_CPPUNWIND))
#define DTS_SOF_INTERFACE_NOEXCEPT throw()
#else
#define DTS_SOF_INTERFACE_NOEXCEPT
#endif
#endif

typedef struct DtsSofInterfaceInst DtsSofInterfaceInst;

typedef enum
{
	DTS_SOF_INTERFACE_BUFFER_LAYOUT_INTERLEAVED = 0,
	DTS_SOF_INTERFACE_BUFFER_LAYOUT_NONINTERLEAVED,
} DtsSofInterfaceBufferLayout;

typedef enum
{
	DTS_SOF_INTERFACE_BUFFER_FORMAT_SINT16LE = 0,
	DTS_SOF_INTERFACE_BUFFER_FORMAT_SINT24LE,
	DTS_SOF_INTERFACE_BUFFER_FORMAT_SINT32LE,
	DTS_SOF_INTERFACE_BUFFER_FORMAT_FLOAT32,
} DtsSofInterfaceBufferFormat;

typedef struct DtsSofInterfaceBufferConfiguration
{
	DtsSofInterfaceBufferLayout bufferLayout;
	DtsSofInterfaceBufferFormat bufferFormat;
	unsigned int                sampleRate;
	unsigned int                numChannels;
	unsigned int                totalBufferLengthInBytes;
} DtsSofInterfaceBufferConfiguration;

typedef void* (*DtsSofInterfaceAllocateMemory)(
	void* pMemoryAllocationContext,
	unsigned int length,
	unsigned int alignment);

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceInit(
	DtsSofInterfaceInst** ppInst,
	DtsSofInterfaceAllocateMemory pMemoryAllocationFn,
	void* MemoryAllocationContext) DTS_SOF_INTERFACE_NOEXCEPT;

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfacePrepare(
	DtsSofInterfaceInst* pInst,
	const DtsSofInterfaceBufferConfiguration* pBufferConfiguration,
	void** ppSofInputBuffer,
	unsigned int* pSofInputBufferSize,
	void** ppSofOutputBuffer,
	unsigned int* pSofOutputBufferSize) DTS_SOF_INTERFACE_NOEXCEPT;

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceInitProcess(
	DtsSofInterfaceInst* pInst) DTS_SOF_INTERFACE_NOEXCEPT;

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceProcess(
	DtsSofInterfaceInst* pInst,
	 unsigned int* pNumBytesProcessed) DTS_SOF_INTERFACE_NOEXCEPT;

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceApplyConfig(
	DtsSofInterfaceInst* pInst,
	int parameterId,
	void* pData,
	unsigned int dataSize) DTS_SOF_INTERFACE_NOEXCEPT;

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceReset(
	DtsSofInterfaceInst* pInst) DTS_SOF_INTERFACE_NOEXCEPT;

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceFree(
	DtsSofInterfaceInst* pInst) DTS_SOF_INTERFACE_NOEXCEPT;

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceGetVersion(
	DtsSofInterfaceVersionInfo* pInterfaceVersion,
	DtsSofInterfaceVersionInfo* pSdkVersion) DTS_SOF_INTERFACE_NOEXCEPT;

#if defined(__cplusplus)
}
#endif

#endif //  __SOF_AUDIO_DTS_SOF_INTERFACE_H__
