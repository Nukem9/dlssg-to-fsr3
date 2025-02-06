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
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_CreateFeature)(DeviceContext, Unknown, Parameters, OutInstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_EvaluateFeature(ID3D11DeviceContext *DeviceContext, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_EvaluateFeature)(DeviceContext, InstanceHandle, Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_GetFeatureRequirements(IDXGIAdapter *Adapter, void *FeatureDiscoveryInfo, NGXFeatureRequirementInfo *RequirementInfo)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_GetFeatureRequirements)(Adapter, FeatureDiscoveryInfo, RequirementInfo);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_GetScratchBufferSize)(Unknown1, Unknown2, OutBufferSize);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Init(void *Unknown1, const wchar_t *Path, ID3D11Device *D3DDevice, uint32_t Unknown3)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_Init)(Unknown1, Path, D3DDevice, Unknown3);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Init_Ext(void *Unknown1, const wchar_t *Path, ID3D11Device *D3DDevice, uint32_t Unknown3)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_Init_Ext)(Unknown1, Path, D3DDevice, Unknown3);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_PopulateDeviceParameters_Impl(ID3D11Device *D3DDevice, NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_PopulateDeviceParameters_Impl)(D3DDevice, Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_PopulateParameters_Impl)(Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_ReleaseFeature(NGXHandle *InstanceHandle)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_ReleaseFeature)(InstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Shutdown()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_Shutdown)();
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D11_Shutdown1(ID3D11Device *D3DDevice)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D11_Shutdown1)(D3DDevice);
}
