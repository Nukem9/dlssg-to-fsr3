#include <shared_mutex>
#include "nvngx.h"
#include "FFFrameInterpolator.h"
#include "Util.h"

std::shared_mutex NGXInstanceHandleLock;
std::unordered_map<uint32_t, std::shared_ptr<FFFrameInterpolator>> NGXInstanceHandles;

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_CreateFeature(
	ID3D12CommandList *CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	spdlog::info(__FUNCTION__);

	if (!CommandList || !Parameters || !OutInstanceHandle)
		return 0xBAD00005;

	// Grab NGX parameters from sl.dlss_g.dll
	// https://forums.developer.nvidia.com/t/using-dlssg-without-idxgiswapchain-present/247260/8?u=user81906
	Parameters->Set4("DLSSG.MustCallEval", 1);

	uint32_t swapchainWidth = 0;
	Parameters->Get5("Width", &swapchainWidth);

	uint32_t swapchainHeight = 0;
	Parameters->Get5("Height", &swapchainHeight);

	uint32_t backbufferFormat = 0;
	Parameters->Get5("DLSSG.BackbufferFormat", &backbufferFormat);

	// Grab the device from the command list
	ID3D12Device *device = nullptr;

	if (FAILED(CommandList->GetDevice(IID_PPV_ARGS(&device))))
		return 0xBAD00004;

	// Finally initialize FSR
	try
	{
		auto instance = std::make_shared<FFFrameInterpolator>(
			device,
			swapchainWidth,
			swapchainHeight,
			static_cast<DXGI_FORMAT>(backbufferFormat));

		device->Release(); // TODO: RAII

		std::scoped_lock lock(NGXInstanceHandleLock);
		{
			const auto id = static_cast<uint32_t>(NGXInstanceHandles.size()) + 1;
			NGXInstanceHandles.emplace(id, std::move(instance));

			*OutInstanceHandle = new NGXHandle { id, 11 };
		}
	}
	catch (const std::exception &e)
	{
		spdlog::error("NVSDK_NGX_D3D12_CreateFeature: Failed to initialize: {}", e.what());
		return 0xBAD00004;
	}

	spdlog::info("NVSDK_NGX_D3D12_CreateFeature: Succeeded.");
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList *CommandList, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!CommandList || !InstanceHandle || !Parameters)
		return 0xBAD00005;

	std::shared_ptr<FFFrameInterpolator> instance;
	{
		std::shared_lock lock(NGXInstanceHandleLock);
		auto itr = NGXInstanceHandles.find(InstanceHandle->InternalId);

		if (itr == NGXInstanceHandles.end())
			return 0xBAD00004;

		instance = itr->second;
	}

	const auto status = instance->Dispatch(CommandList, Parameters);

	if (status != FFX_OK)
	{
		static bool once = [&]()
		{
			spdlog::error("Evaluation call failed with status {:X}.", status);
			return true;
		}();
	}

	switch (status)
	{
	case FFX_OK:
		return NGX_SUCCESS;

	case FFX_ERROR_INVALID_ARGUMENT:
		return 0xBAD00005;
	}

	return 0xBAD00005;
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
	spdlog::info(__FUNCTION__);

	if (!D3DDevice)
		return 0xBAD00005;

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init_Ext(void *, const wchar_t *Path, void *, uint32_t Unknown4, NGXInstanceParameters *Parameters)
{
	spdlog::info(__FUNCTION__);

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

	// Assume 300MB
	*EstimatedSize = 300 * 1024 * 1024;

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
	spdlog::info(__FUNCTION__);

	if (!InstanceHandle)
		return 0xBAD00005;

	const auto node = [&]()
	{
		std::scoped_lock lock(NGXInstanceHandleLock);
		return NGXInstanceHandles.extract(InstanceHandle->InternalId);
	}();

	if (node.empty())
		return 0xBAD00004;

	// Node is handled by RAII. Apparently, InstanceHandle isn't supposed to be deleted.
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Shutdown()
{
	spdlog::info(__FUNCTION__);
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Shutdown1(ID3D12Device *D3DDevice)
{
	spdlog::info(__FUNCTION__);

	if (!D3DDevice)
		return 0xBAD00005;

	return NGX_SUCCESS;
}
