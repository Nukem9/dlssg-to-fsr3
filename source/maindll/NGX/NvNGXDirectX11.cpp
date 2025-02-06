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
	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_EvaluateFeature(ID3D11DeviceContext *DeviceContext, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	return 0xBAD00001;
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
	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Init_Ext(void *Unknown1, const wchar_t *Path, ID3D11Device *D3DDevice, uint32_t Unknown3)
{
	return 0xBAD00001;
}

static NGXResult GetCurrentSettingsCallback(NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!InstanceHandle || !Parameters)
		return NGX_INVALID_PARAMETER;

	Parameters->Set4("DLSSG.MustCallEval", 1);
	Parameters->Set4("DLSSG.BurstCaptureRunning", 0);

	return NGX_SUCCESS;
}

static NGXResult EstimateVRAMCallback(
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	uint32_t,
	size_t *EstimatedSize)
{
	// Assume 300MB
	if (EstimatedSize)
		*EstimatedSize = 300 * 1024 * 1024;

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_PopulateDeviceParameters_Impl(ID3D11Device *D3DDevice, NGXInstanceParameters *Parameters)
{
	if (!D3DDevice || !Parameters)
		return NGX_INVALID_PARAMETER;

	Parameters->SetVoidPointer("DLSSG.GetCurrentSettingsCallback", &GetCurrentSettingsCallback);
	Parameters->SetVoidPointer("DLSSG.EstimateVRAMCallback", &EstimateVRAMCallback);
	Parameters->Set5("DLSSG.MultiFrameCountMax", 1);
	Parameters->Set4("DLSSG.ReflexWarp.Available", 0);

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	if (!Parameters)
		return NGX_INVALID_PARAMETER;

	Parameters->SetVoidPointer("DLSSG.GetCurrentSettingsCallback", &GetCurrentSettingsCallback);
	Parameters->SetVoidPointer("DLSSG.EstimateVRAMCallback", &EstimateVRAMCallback);
	Parameters->Set5("DLSSG.MultiFrameCountMax", 1);
	Parameters->Set4("DLSSG.ReflexWarp.Available", 0);

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_ReleaseFeature(NGXHandle *InstanceHandle)
{
	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Shutdown()
{
	return 0xBAD00001;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Shutdown1(ID3D11Device *D3DDevice)
{
	return 0xBAD00001;
}
