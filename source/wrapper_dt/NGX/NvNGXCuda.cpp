#include "NvNGX.h"

NGXDLLEXPORT uint32_t NvOptimusEnablementCuda = 0x00000001;

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_CreateFeature()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_CUDA_CreateFeature)();
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_EvaluateFeature()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_CUDA_EvaluateFeature)();
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_CUDA_GetScratchBufferSize)(Unknown1, Unknown2, OutBufferSize);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_Init()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_CUDA_Init)();
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_ReleaseFeature(NGXHandle *InstanceHandle)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_CUDA_ReleaseFeature)(InstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_CUDA_Shutdown()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_CUDA_Shutdown)();
}
