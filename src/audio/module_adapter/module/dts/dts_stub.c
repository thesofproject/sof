// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org>

#include <sof/audio/dts/DtsSofInterface.h>

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceInit(
		DtsSofInterfaceInst           **ppInst,
		DtsSofInterfaceAllocateMemory pMemoryAllocationFn,
		DtsSofInterfaceFreeMemory     pMemoryFreeFn,
		void                          *MemoryAllocationContext) DTS_SOF_INTERFACE_NOEXCEPT
{
	return DTS_SOF_INTERFACE_RESULT_SUCCESS;
}

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfacePrepare(
	DtsSofInterfaceInst* pInst,
	const DtsSofInterfaceBufferConfiguration* pBufferConfiguration,
	void** ppSofInputBuffer,
	unsigned int* pSofInputBufferSize,
	void** ppSofOutputBuffer,
	unsigned int* pSofOutputBufferSize) DTS_SOF_INTERFACE_NOEXCEPT
{
	return DTS_SOF_INTERFACE_RESULT_SUCCESS;
}

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceInitProcess(
	DtsSofInterfaceInst* pInst) DTS_SOF_INTERFACE_NOEXCEPT
{
	return DTS_SOF_INTERFACE_RESULT_SUCCESS;
}

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceProcess(
	DtsSofInterfaceInst* pInst,
	 unsigned int* pNumBytesProcessed) DTS_SOF_INTERFACE_NOEXCEPT
{
	return DTS_SOF_INTERFACE_RESULT_SUCCESS;
}

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceApplyConfig(
	DtsSofInterfaceInst* pInst,
	int parameterId,
	const void *pData,
	unsigned int dataSize) DTS_SOF_INTERFACE_NOEXCEPT
{
	return DTS_SOF_INTERFACE_RESULT_SUCCESS;
}

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceReset(
	DtsSofInterfaceInst* pInst) DTS_SOF_INTERFACE_NOEXCEPT
{
	return DTS_SOF_INTERFACE_RESULT_SUCCESS;
}

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceFree(
	DtsSofInterfaceInst* pInst) DTS_SOF_INTERFACE_NOEXCEPT
{
	return DTS_SOF_INTERFACE_RESULT_SUCCESS;
}

DtsSofInterfaceResult DTS_SOF_INTERFACE_API dtsSofInterfaceGetVersion(
	DtsSofInterfaceVersionInfo* pInterfaceVersion,
	DtsSofInterfaceVersionInfo* pSdkVersion) DTS_SOF_INTERFACE_NOEXCEPT
{
	return DTS_SOF_INTERFACE_RESULT_SUCCESS;
}
