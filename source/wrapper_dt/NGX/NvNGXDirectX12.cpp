#include "nvngx.h"

struct IDXGIAdapter;
struct ID3D12Device;
struct ID3D12CommandList;
struct ID3D12GraphicsCommandList;

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_CreateFeature(
	ID3D12CommandList *CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_CreateFeature)(CommandList, Unknown, Parameters, OutInstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_EvaluateFeature(
	ID3D12GraphicsCommandList *CommandList,
	NGXHandle *InstanceHandle,
	NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_EvaluateFeature)(CommandList, InstanceHandle, Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_GetFeatureRequirements(
	IDXGIAdapter *Adapter,
	void *FeatureDiscoveryInfo,
	NGXFeatureRequirementInfo *RequirementInfo)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_GetFeatureRequirements)(Adapter, FeatureDiscoveryInfo, RequirementInfo);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_GetScratchBufferSize)(Unknown1, Unknown2, OutBufferSize);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init_Ext(
	void *Unknown1,
	const wchar_t *Path,
	ID3D12Device *D3DDevice,
	uint32_t Unknown2,
	NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_Init_Ext)(Unknown1, Path, D3DDevice, Unknown2, Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init(void *Unknown1, const wchar_t *Path, ID3D12Device *D3DDevice, uint32_t Unknown2)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_Init)(Unknown1, Path, D3DDevice, Unknown2);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl(ID3D12Device *D3DDevice, NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl)(D3DDevice, Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_PopulateParameters_Impl)(Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_ReleaseFeature(NGXHandle *InstanceHandle)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_ReleaseFeature)(InstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Shutdown()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_Shutdown)();
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Shutdown1(ID3D12Device *D3DDevice)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_D3D12_Shutdown1)(D3DDevice);
}
