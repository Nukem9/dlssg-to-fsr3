#include "NvNGX.h"
#include "FFFrameInterpolatorVKToDX.h"

static std::shared_mutex FeatureInstanceHandleLock;
static std::unordered_map<uint32_t, std::shared_ptr<FFFrameInterpolatorVKToDX>> FeatureInstanceHandles;

VkDevice g_LogicalDevice = VK_NULL_HANDLE;
VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_CreateFeature1(
	VkDevice LogicalDevice,
	VkCommandBuffer CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	spdlog::info(__FUNCTION__);

	if (!LogicalDevice || !Parameters || !OutInstanceHandle)
		return NGX_INVALID_PARAMETER;

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
		auto instance = std::make_shared<FFFrameInterpolatorVKToDX>(
			LogicalDevice,
			g_PhysicalDevice,
			swapchainWidth,
			swapchainHeight,
			Parameters);

		std::scoped_lock lock(FeatureInstanceHandleLock);
		{
			const auto handle = NGXHandle::Allocate(11);
			*OutInstanceHandle = handle;

			FeatureInstanceHandles.emplace(handle->InternalId, std::move(instance));
		}
	}
	catch (const std::exception& e)
	{
		spdlog::error("NVSDK_NGX_VULKAN_CreateFeature1: Failed to initialize: {}", e.what());
		return NGX_FEATURE_NOT_FOUND;
	}

	spdlog::info("NVSDK_NGX_VULKAN_CreateFeature1: Succeeded.");
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_CreateFeature(
	VkCommandBuffer CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	spdlog::info(__FUNCTION__);

	if (!Parameters || !OutInstanceHandle)
		return NGX_INVALID_PARAMETER;

	return NVSDK_NGX_VULKAN_CreateFeature1(g_LogicalDevice, CommandList, Unknown, Parameters, OutInstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_EvaluateFeature(VkCommandBuffer CommandList, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!CommandList || !InstanceHandle || !Parameters)
		return NGX_INVALID_PARAMETER;

	const auto instance = [&]()
	{
		std::shared_lock lock(FeatureInstanceHandleLock);
		auto itr = FeatureInstanceHandles.find(InstanceHandle->InternalId);

		if (itr == FeatureInstanceHandles.end())
			return decltype(itr->second) {};

		return itr->second;
	}();

	if (!instance)
		return NGX_FEATURE_NOT_FOUND;

	const auto status = instance->Dispatch(CommandList, Parameters);

	switch (status)
	{
	case FFX_OK:
	{
		static bool once = [&]()
		{
			spdlog::info("NVSDK_NGX_VULKAN_EvaluateFeature: Succeeded.");
			return true;
		}();

		return NGX_SUCCESS;
	}

	default:
	{
		static bool once = [&]()
		{
			spdlog::error("Evaluation call failed with status {:X}.", static_cast<uint32_t>(status));
			return true;
		}();

		return NGX_INVALID_PARAMETER;
	}
	}
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_GetFeatureRequirements(
	VkInstance VulkanInstance,
	VkPhysicalDevice PhysicalDevice,
	void *FeatureDiscoveryInfo,
	NGXFeatureRequirementInfo *RequirementInfo)
{
	if (!FeatureDiscoveryInfo || !RequirementInfo)
		return NGX_INVALID_PARAMETER;

	RequirementInfo->Flags = 0;
	RequirementInfo->RequiredGPUArchitecture = NGXHardcodedArchitecture;
	strcpy_s(RequirementInfo->RequiredOperatingSystemVersion, "10.0.0");

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	if (!OutBufferSize)
		return NGX_INVALID_PARAMETER;

	*OutBufferSize = 0;
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_Init(
	void *Unknown1,
	void *Unknown2,
	VkInstance VulkanInstance,
	VkPhysicalDevice PhysicalDevice,
	VkDevice LogicalDevice,
	uint32_t Unknown3)
{
	spdlog::info(__FUNCTION__);

	if (!VulkanInstance || !PhysicalDevice || !LogicalDevice)
		return NGX_INVALID_PARAMETER;

	g_LogicalDevice = LogicalDevice;
	g_PhysicalDevice = PhysicalDevice;

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_Init_Ext(
	void *Unknown1,
	void *Unknown2,
	VkInstance VulkanInstance,
	VkPhysicalDevice PhysicalDevice,
	VkDevice LogicalDevice,
	uint32_t Unknown3,
	void *Unknown4)
{
	spdlog::info(__FUNCTION__);

	if (!VulkanInstance || !PhysicalDevice || !LogicalDevice)
		return NGX_INVALID_PARAMETER;

	g_LogicalDevice = LogicalDevice;
	g_PhysicalDevice = PhysicalDevice;

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_Init_Ext2(
	void *Unknown1,
	void *Unknown2,
	VkInstance VulkanInstance,
	VkPhysicalDevice PhysicalDevice,
	VkDevice LogicalDevice,
	void *Unknown3,
	uint32_t Unknown4,
	NGXInstanceParameters *Parameters)
{
	spdlog::info(__FUNCTION__);

	if (!VulkanInstance || !PhysicalDevice || !LogicalDevice)
		return NGX_INVALID_PARAMETER;

	g_LogicalDevice = LogicalDevice;
	g_PhysicalDevice = PhysicalDevice;

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

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	spdlog::info(__FUNCTION__);

	if (!Parameters)
		return NGX_INVALID_PARAMETER;

	Parameters->SetVoidPointer("DLSSG.GetCurrentSettingsCallback", &GetCurrentSettingsCallback);
	Parameters->SetVoidPointer("DLSSG.EstimateVRAMCallback", &EstimateVRAMCallback);

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_ReleaseFeature(NGXHandle *InstanceHandle)
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

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_Shutdown()
{
	spdlog::info(__FUNCTION__);
	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_Shutdown1(VkDevice LogicalDevice)
{
	spdlog::info(__FUNCTION__);

	if (!LogicalDevice)
		return NGX_INVALID_PARAMETER;

	return NGX_SUCCESS;
}
