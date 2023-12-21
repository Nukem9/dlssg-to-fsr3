#include <shared_mutex>
#include "nvngx.h"
#include "FFXInterpolator.h"

std::shared_mutex NGXInstanceHandleLock;
std::unordered_map<uint32_t, std::shared_ptr<FFXInterpolator>> NGXInstanceHandles;

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_CreateFeature(
	ID3D12CommandList *CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	if (!CommandList || !Parameters || !OutInstanceHandle)
		return 0xBAD00005;

	// Grab NGX parameters from sl.dlss_g.dll
	// https://forums.developer.nvidia.com/t/using-dlssg-without-idxgiswapchain-present/247260/8?u=user81906
	// Parameters->Get5("CreationNodeMask", &temp);
	// Parameters->Get5("VisibilityNodeMask", &temp);

	uint32_t swapchainWidth = 0;
	Parameters->Get5("Width", &swapchainWidth);

	uint32_t swapchainHeight = 0;
	Parameters->Get5("Height", &swapchainHeight);

	uint32_t backbufferFormat = 0;
	Parameters->Get5("DLSSG.BackbufferFormat", &backbufferFormat);

	Parameters->Set4("DLSSG.MustCallEval", 1);

	// Grab the device from the command list
	ID3D12Device *device = nullptr;

	if (FAILED(CommandList->GetDevice(IID_PPV_ARGS(&device))))
		return 0xBAD00004;

	// Finally initialize FSR
	auto instance = std::make_shared<FFXInterpolator>(device, swapchainWidth, swapchainHeight, backbufferFormat);

	NGXInstanceHandleLock.lock();
	{
		const auto id = static_cast<uint32_t>(NGXInstanceHandles.size()) + 1;
		NGXInstanceHandles.emplace(id, std::move(instance));

		*OutInstanceHandle = new NGXHandle { id, 11 };
	}
	NGXInstanceHandleLock.unlock();

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList *CommandList, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!CommandList || !InstanceHandle || !Parameters)
		return 0xBAD00005;

	std::shared_ptr<FFXInterpolator> instance;
	{
		std::shared_lock lock(NGXInstanceHandleLock);
		auto itr = NGXInstanceHandles.find(InstanceHandle->InternalId);

		if (itr == NGXInstanceHandles.end())
			return 0xBAD00004;

		instance = itr->second;
	}

	instance->Dispatch(CommandList, Parameters);
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_GetFeatureRequirements(IDXGIAdapter *Adapter, void *FeatureDiscoveryInfo, NGXFeatureRequirementInfo *RequirementInfo)
{
	if (!FeatureDiscoveryInfo || !RequirementInfo)
		return 0xBAD00005;

	RequirementInfo->Flags = 0;
	RequirementInfo->RequiredGPUArchitecture = NGXHardcodedArchitecture;
	strcpy_s(RequirementInfo->RequiredOperatingSystemVersion, "10.0.0");

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	if (OutBufferSize)
	{
		*OutBufferSize = 0;
		return NGX_SUCCESS;
	}

	return 0xBAD00005;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init(void *Unknown1, const wchar_t *Path, ID3D12Device *D3DDevice, uint32_t Unknown3)
{
	if (!D3DDevice)
		return 0xBAD00005;

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init_Ext(void *, const wchar_t *Path, void *, uint32_t Unknown4, NGXInstanceParameters *Parameters)
{
	// Seems to create the base instance but does nothing with the parameters other than
	// setting up logging
	return NGX_SUCCESS;
}

NGXResult GetCurrentSettingsCallback(NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!InstanceHandle || !Parameters)
		return 0xBAD00005;

	Parameters->Set4("DLSSG.MustCallEval", 1);
	Parameters->Set4("DLSSG.BurstCaptureRunning", 0);

	return NGX_SUCCESS;
}

NGXResult EstimateVRAMCallback(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, size_t *EstimatedSize)
{
	if (!EstimatedSize)
		return 0xBAD00005;

	*EstimatedSize = 300 * 1024 * 1024; // Assume 300MB
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	if (!Parameters)
		return 0xBAD00005;

	Parameters->SetVoidPointer("DLSSG.GetCurrentSettingsCallback", &GetCurrentSettingsCallback);
	Parameters->SetVoidPointer("DLSSG.EstimateVRAMCallback", &EstimateVRAMCallback);

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_ReleaseFeature(NGXHandle *InstanceHandle)
{
	if (!InstanceHandle)
		return 0xBAD00005;

	const auto node = [&]()
	{
		NGXInstanceHandleLock.lock();
		auto node = NGXInstanceHandles.extract(InstanceHandle->InternalId);
		NGXInstanceHandleLock.unlock();

		return node;
	}();

	if (node.empty())
		return 0xBAD00004;

	while (node.mapped()->AnyResourcesInUse())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	// Node is handled by RAII. Apparently, InstanceHandle isn't supposed to be deleted.
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Shutdown()
{
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Shutdown1(ID3D12Device *D3DDevice)
{
	if (!D3DDevice)
		return 0xBAD00005;

	return NGX_SUCCESS;
}
