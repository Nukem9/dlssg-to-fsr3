#include <shared_mutex>
#include "NvNGX.h"
#include "FFFrameInterpolator.h"

static std::shared_mutex FeatureInstanceHandleLock;
static std::unordered_map<uint32_t, std::shared_ptr<FFFrameInterpolator>> FeatureInstanceHandles;

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_CreateFeature(
	ID3D12CommandList *CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	spdlog::info(__FUNCTION__);

	if (!CommandList || !Parameters || !OutInstanceHandle)
		return NGX_INVALID_PARAMETER;

	// Grab the device from the command list
	ID3D12Device *device = nullptr;

	if (FAILED(CommandList->GetDevice(IID_PPV_ARGS(&device))))
		return 0xBAD00002;

	// Grab NGX parameters from sl.dlss_g.dll
	// https://forums.developer.nvidia.com/t/using-dlssg-without-idxgiswapchain-present/247260/8?u=user81906
	Parameters->Set4("DLSSG.MustCallEval", 1);

	uint32_t swapchainWidth = 0;
	Parameters->Get5("Width", &swapchainWidth);

	uint32_t swapchainHeight = 0;
	Parameters->Get5("Height", &swapchainHeight);

	// Then initialize FSR
	try
	{
		auto instance = std::make_shared<FFFrameInterpolator>(
			device,
			swapchainWidth,
			swapchainHeight);

		std::scoped_lock lock(FeatureInstanceHandleLock);
		{
			const auto handle = NGXHandle::Allocate(11);
			*OutInstanceHandle = handle;

			FeatureInstanceHandles.emplace(handle->InternalId, std::move(instance));
		}
	}
	catch (const std::exception &e)
	{
		spdlog::error("NVSDK_NGX_D3D12_CreateFeature: Failed to initialize: {}", e.what());
		device->Release();

		return NGX_FEATURE_NOT_FOUND;
	}

	spdlog::info("NVSDK_NGX_D3D12_CreateFeature: Succeeded.");
	device->Release();

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList *CommandList, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!CommandList || !InstanceHandle || !Parameters)
		return NGX_INVALID_PARAMETER;

	std::shared_ptr<FFFrameInterpolator> instance;
	{
		std::shared_lock lock(FeatureInstanceHandleLock);
		auto itr = FeatureInstanceHandles.find(InstanceHandle->InternalId);

		if (itr == FeatureInstanceHandles.end())
			return NGX_FEATURE_NOT_FOUND;

		instance = itr->second;
	}

	const auto status = instance->Dispatch(CommandList, Parameters);

	switch (status)
	{
	case FFX_OK:
		return NGX_SUCCESS;

	case FFX_ERROR_INVALID_ARGUMENT:
	default:
	{
		static bool once = [&]()
		{
			spdlog::error("Evaluation call failed with status {:X}.", status);
			return true;
		}();

		return NGX_INVALID_PARAMETER;
	}
	}
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_GetFeatureRequirements(IDXGIAdapter *Adapter, void *FeatureDiscoveryInfo, NGXFeatureRequirementInfo *RequirementInfo)
{
	if (!FeatureDiscoveryInfo || !RequirementInfo)
		return NGX_INVALID_PARAMETER;

	RequirementInfo->Flags = 0;
	RequirementInfo->RequiredGPUArchitecture = NGXHardcodedArchitecture;
	strcpy_s(RequirementInfo->RequiredOperatingSystemVersion, "10.0.0");

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	if (!OutBufferSize)
		return NGX_INVALID_PARAMETER;

	*OutBufferSize = 0;
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init(void *Unknown1, const wchar_t *Path, ID3D12Device *D3DDevice, uint32_t Unknown3)
{
	spdlog::info(__FUNCTION__);

	if (!D3DDevice)
		return NGX_INVALID_PARAMETER;

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_Init_Ext(void *, const wchar_t *Path, void *, uint32_t Unknown4, NGXInstanceParameters *Parameters)
{
	spdlog::info(__FUNCTION__);

	// Seems to create the base instance but does nothing with the parameters other than
	// setting up logging
	return NGX_SUCCESS;
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

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	spdlog::info(__FUNCTION__);

	if (!Parameters)
		return NGX_INVALID_PARAMETER;

	Parameters->SetVoidPointer("DLSSG.GetCurrentSettingsCallback", &GetCurrentSettingsCallback);
	Parameters->SetVoidPointer("DLSSG.EstimateVRAMCallback", &EstimateVRAMCallback);

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_D3D12_ReleaseFeature(NGXHandle *InstanceHandle)
{
	spdlog::info(__FUNCTION__);

	if (!InstanceHandle)
		return NGX_INVALID_PARAMETER;

	const auto node = [&]()
	{
		std::scoped_lock lock(FeatureInstanceHandleLock);
		return FeatureInstanceHandles.extract(InstanceHandle->InternalId);
	}();

	if (node.empty())
		return NGX_FEATURE_NOT_FOUND;

	// Node is handled by RAII. Apparently, InstanceHandle isn't supposed to be deleted.
	// NGXHandle::Free(InstanceHandle);
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
		return NGX_INVALID_PARAMETER;

	return NGX_SUCCESS;
}
