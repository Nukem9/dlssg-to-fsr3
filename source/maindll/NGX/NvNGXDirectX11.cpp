#include "NvNGX.h"

struct IDXGIAdapter;
struct ID3D11Device;
struct ID3D11DeviceContext;

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_CreateFeature(
	ID3D11DeviceContext *DeviceContext,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_EvaluateFeature(ID3D11DeviceContext *DeviceContext, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_GetFeatureRequirements(IDXGIAdapter *Adapter, void *FeatureDiscoveryInfo, NGXFeatureRequirementInfo *RequirementInfo)
{
	if (!FeatureDiscoveryInfo || !RequirementInfo)
		return NGX_INVALID_PARAMETER;

	RequirementInfo->Flags = 0;
	RequirementInfo->RequiredGPUArchitecture = NGXHardcodedArchitecture;
	strcpy_s(RequirementInfo->RequiredOperatingSystemVersion, "10.0.0");

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	*OutBufferSize = 0;
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Init(void *Unknown1, const wchar_t *Path, ID3D11Device *D3DDevice, uint32_t Unknown3)
{
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_ReleaseFeature(NGXHandle *InstanceHandle)
{
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Shutdown()
{
	return NGX_SUCCESS;
}
