#include <vulkan/vulkan.h>
#include "NvNGX.h"

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_CreateFeature1(
	VkDevice LogicalDevice,
	VkCommandBuffer CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_CreateFeature1)(LogicalDevice, CommandList, Unknown, Parameters, OutInstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_CreateFeature(
	VkCommandBuffer CommandList,
	void *Unknown,
	NGXInstanceParameters *Parameters,
	NGXHandle **OutInstanceHandle)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_CreateFeature)(CommandList, Unknown, Parameters, OutInstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_EvaluateFeature(VkCommandBuffer CommandList, NGXHandle *InstanceHandle, NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_EvaluateFeature)(CommandList, InstanceHandle, Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_GetFeatureRequirements(
	VkInstance VulkanInstance,
	VkPhysicalDevice PhysicalDevice,
	void *FeatureDiscoveryInfo,
	NGXFeatureRequirementInfo *RequirementInfo)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_GetFeatureRequirements)(VulkanInstance, PhysicalDevice, FeatureDiscoveryInfo, RequirementInfo);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_GetScratchBufferSize(void *Unknown1, void *Unknown2, uint64_t *OutBufferSize)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_GetScratchBufferSize)(Unknown1, Unknown2, OutBufferSize);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_Init(
	void *Unknown1,
	void *Unknown2,
	VkInstance VulkanInstance,
	VkPhysicalDevice PhysicalDevice,
	VkDevice LogicalDevice,
	uint32_t Unknown3)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_Init)(Unknown1, Unknown2, VulkanInstance, PhysicalDevice, LogicalDevice, Unknown3);
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
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_Init_Ext)(Unknown1, Unknown2, VulkanInstance, PhysicalDevice, LogicalDevice, Unknown3, Unknown4);
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
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_Init_Ext2)(
		Unknown1,
		Unknown2,
		VulkanInstance,
		PhysicalDevice,
		LogicalDevice,
		Unknown3,
		Unknown4,
		Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_PopulateParameters_Impl(NGXInstanceParameters *Parameters)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_PopulateParameters_Impl)(Parameters);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_ReleaseFeature(NGXHandle *InstanceHandle)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_ReleaseFeature)(InstanceHandle);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_Shutdown()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_Shutdown)();
}

NGXDLLEXPORT NGXResult NVSDK_NGX_VULKAN_Shutdown1(VkDevice LogicalDevice)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_VULKAN_Shutdown1)(LogicalDevice);
}
