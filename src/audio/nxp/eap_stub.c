// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2025 NXP


#include <sof/audio/nxp/eap/eap_lib_defines.h>
#include <nxp/eap/EAP_Includes/EAP16.h>

LVM_ReturnStatus_en LVM_GetVersionInfo(LVM_VersionInfo_st  *pVersion)
{
	return LVM_SUCCESS;
}

LVM_ReturnStatus_en LVM_GetInstanceHandle(LVM_Handle_t *phInstance,
					  LVM_MemTab_t        *pMemoryTable,
					  LVM_InstParams_t    *pInstParams)
{
	return LVM_SUCCESS;
};

LVM_ReturnStatus_en LVM_GetMemoryTable(LVM_Handle_t hInstance, LVM_MemTab_t *pMemoryTable,
				       LVM_InstParams_t *pInstParams)
{
	return LVM_SUCCESS;
}

LVM_ReturnStatus_en LVM_Process(LVM_Handle_t hInstance, const LVM_INT16 *pInData,
				LVM_INT16 **pOutData, LVM_UINT16 NumSamples,
				LVM_UINT32 AudioTime)
{
	return LVM_SUCCESS;
}

LVM_ReturnStatus_en LVM_SetControlParameters(LVM_Handle_t hInstance, LVM_ControlParams_t *pParams)
{
	return LVM_SUCCESS;
}
