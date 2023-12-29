#include "NvNGX.h"

typedef void *VkCommandBuffer;
typedef void *VkDevice;
typedef void *VkInstance;
typedef void *VkPhysicalDevice;

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_CreateFeature(VkCommandBuffer CommandList, void *Unknown, NGXInstanceParameters *Parameters, NGXHandle **OutInstanceHandle)
{
	spdlog::info(__FUNCTION__);

	if (!Parameters || !OutInstanceHandle)
		return NGX_INVALID_PARAMETER;

	return NGX_SUCCESS;
}

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

	return NGX_SUCCESS;
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_EvaluateFeature(VkCommandBuffer CommandList, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	if (!CommandList || !InstanceHandle || !Parameters)
		return NGX_INVALID_PARAMETER;

	return NGX_SUCCESS;
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
