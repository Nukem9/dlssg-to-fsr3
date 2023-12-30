#include "nvngx.h"

NGXDLLEXPORT uint32_t NvOptimusEnablementCuda = 0x00000001;

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_CreateFeature()
{
	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_EvaluateFeature()
{
	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	if (!OutBufferSize)
		return NGX_INVALID_PARAMETER;

	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_Init()
{
	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_ReleaseFeature(NGXHandle *InstanceHandle)
{
	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_Shutdown()
{
	return 0xBAD00001;
}
