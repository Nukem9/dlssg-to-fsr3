#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include "NGX/NvNGX.h"
#include "FFFrameInterpolatorVK.h"

VkAccessFlags getVKAccessFlagsFromResourceState(FfxResourceStates state);
VkImageLayout getVKImageLayoutFromResourceState(FfxResourceStates state);

FFFrameInterpolatorVK::FFFrameInterpolatorVK(
	VkDevice LogicalDevice,
	VkPhysicalDevice PhysicalDevice,
	uint32_t OutputWidth,
	uint32_t OutputHeight,
	NGXInstanceParameters *NGXParameters)
	: m_Device(LogicalDevice),
	  m_PhysicalDevice(PhysicalDevice),
	  FFFrameInterpolator(OutputWidth, OutputHeight)
{
	FFFrameInterpolator::Create(NGXParameters);
}

FFFrameInterpolatorVK::~FFFrameInterpolatorVK()
{
	FFFrameInterpolator::Destroy();
}

FfxErrorCode FFFrameInterpolatorVK::Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters)
{
	const bool enableInterpolation = NGXParameters->GetUIntOrDefault("DLSSG.EnableInterp", 0) != 0;
	const bool isRecordingCommands = NGXParameters->GetUIntOrDefault("DLSSG.IsRecording", 0) != 0;
	const auto cmdListVk = reinterpret_cast<VkCommandBuffer>(CommandList);

	NGXParameters->Set4("DLSSG.FlushRequired", 0);

	// Begin a new command list in the event our caller didn't set one up
	if (!isRecordingCommands)
	{
		const static bool once = [&]()
		{
			spdlog::warn(
				"Vulkan command list wasn't recording. Resetting state: {} 0x{:X}",
				enableInterpolation,
				reinterpret_cast<uintptr_t>(CommandList));

			return false;
		}();

		VkCommandBufferBeginInfo info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};

		vkResetCommandBuffer(cmdListVk, 0);
		vkBeginCommandBuffer(cmdListVk, &info);
	}

	m_ActiveCommandList = ffxGetCommandListVK(cmdListVk);
	const auto interpolationResult = FFFrameInterpolator::Dispatch(nullptr, NGXParameters);

	// Finish what we started. Restore the command list to its previous state when necessary.
	if (!isRecordingCommands)
		vkEndCommandBuffer(cmdListVk);

	return interpolationResult;
}

FfxErrorCode FFFrameInterpolatorVK::InitializeBackendInterface(
	FFInterfaceWrapper *BackendInterface,
	uint32_t MaxContexts,
	NGXInstanceParameters *NGXParameters)
{
	return BackendInterface->Initialize(m_Device, m_PhysicalDevice, MaxContexts, NGXParameters);
}

FfxCommandList FFFrameInterpolatorVK::GetActiveCommandList() const
{
	return m_ActiveCommandList;
}

std::array<uint8_t, 8> FFFrameInterpolatorVK::GetActiveAdapterLUID() const
{
	VkPhysicalDeviceIDProperties idProperties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
	};

	VkPhysicalDeviceProperties2 properties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &idProperties,
	};

	vkGetPhysicalDeviceProperties2(m_PhysicalDevice, &properties);

	if (!idProperties.deviceLUIDValid)
		return {};

	std::array<uint8_t, sizeof(idProperties.deviceLUID)> result;
	memcpy(result.data(), &idProperties.deviceLUID, result.size());

	return result;
}

void FFFrameInterpolatorVK::CopyTexture(FfxCommandList CommandList, const FfxResource *Destination, const FfxResource *Source)
{
	const auto cmdListVk = reinterpret_cast<VkCommandBuffer>(CommandList);

	const uint32_t srcStageMask = MakeVulkanStageFlags(Source->state) | MakeVulkanStageFlags(Destination->state);
	const uint32_t destStageMask = MakeVulkanStageFlags(FFX_RESOURCE_STATE_COPY_SRC) | MakeVulkanStageFlags(FFX_RESOURCE_STATE_COPY_DEST);

	std::array barriers = {
		MakeVulkanBarrier(static_cast<VkImage>(Source->resource), Source->state, FFX_RESOURCE_STATE_COPY_SRC, false),
		MakeVulkanBarrier(static_cast<VkImage>(Destination->resource), Destination->state, FFX_RESOURCE_STATE_COPY_DEST, false),
	};

	vkCmdPipelineBarrier(
		cmdListVk,
		srcStageMask,
		destStageMask,
		0,
		0,
		nullptr,
		0,
		nullptr,
		static_cast<uint32_t>(barriers.size()),
		barriers.data());

	VkImageCopy copyRegion = {};
	copyRegion.extent.width = Destination->description.width;
	copyRegion.extent.height = Destination->description.height;
	copyRegion.extent.depth = Destination->description.depth;
	copyRegion.dstSubresource.aspectMask = barriers[0].subresourceRange.aspectMask;
	copyRegion.dstSubresource.mipLevel = barriers[0].subresourceRange.baseMipLevel;
	copyRegion.dstSubresource.baseArrayLayer = barriers[0].subresourceRange.baseArrayLayer;
	copyRegion.dstSubresource.layerCount = barriers[0].subresourceRange.layerCount;
	copyRegion.srcSubresource = copyRegion.dstSubresource;

	vkCmdCopyImage(cmdListVk, barriers[0].image, barriers[0].newLayout, barriers[1].image, barriers[1].newLayout, 1, &copyRegion);

	std::swap(barriers[0].srcAccessMask, barriers[0].dstAccessMask);
	std::swap(barriers[0].oldLayout, barriers[0].newLayout);
	std::swap(barriers[1].srcAccessMask, barriers[1].dstAccessMask);
	std::swap(barriers[1].oldLayout, barriers[1].newLayout);

	vkCmdPipelineBarrier(
		cmdListVk,
		destStageMask,
		srcStageMask,
		0,
		0,
		nullptr,
		0,
		nullptr,
		static_cast<uint32_t>(barriers.size()),
		barriers.data());
}

bool FFFrameInterpolatorVK::LoadTextureFromNGXParameters(
	NGXInstanceParameters *NGXParameters,
	const char *Name,
	FfxResource *OutFfxResource,
	FfxResourceStates State)
{
	NGXVulkanResourceHandle *resourceHandle = nullptr;
	NGXParameters->GetVoidPointer(Name, reinterpret_cast<void **>(&resourceHandle));

	if (!resourceHandle)
	{
		*OutFfxResource = {};
		return false;
	}

	if (resourceHandle->Type != 0) // TODO: Figure out where this is defined
	{
		*OutFfxResource = {};
		return false;
	}

	// Vulkan provides no mechanism to query resource information. Convert it manually.
	VkImageCreateInfo imageInfo = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = resourceHandle->ImageMetadata.Format,
		.extent = { resourceHandle->ImageMetadata.Width, resourceHandle->ImageMetadata.Height, 1 },
		.mipLevels = resourceHandle->ImageMetadata.Subresource.levelCount,
		.arrayLayers = resourceHandle->ImageMetadata.Subresource.layerCount,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	if (imageInfo.format == VK_FORMAT_D32_SFLOAT_S8_UINT)
	{
		imageInfo.format = VK_FORMAT_D32_SFLOAT;
		imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}

	*OutFfxResource = ffxGetResourceVK(
		resourceHandle->ImageMetadata.Image,
		ffxGetImageResourceDescriptionVK(resourceHandle->ImageMetadata.Image, imageInfo),
		nullptr,
		State);

	return true;
}

VkImageMemoryBarrier FFFrameInterpolatorVK::MakeVulkanBarrier(
	VkImage Resource,
	FfxResourceStates SourceState,
	FfxResourceStates DestinationState,
	bool IsDepthAspect)
{
	return VkImageMemoryBarrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = getVKAccessFlagsFromResourceState(SourceState),
		.dstAccessMask = getVKAccessFlagsFromResourceState(DestinationState),
		.oldLayout = getVKImageLayoutFromResourceState(SourceState),
		.newLayout = getVKImageLayoutFromResourceState(DestinationState),
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // All on compute queue
		.image = Resource,
		.subresourceRange = {
			.aspectMask = static_cast<VkImageAspectFlags>(IsDepthAspect ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = 1,
		},
	};
}

VkPipelineStageFlags FFFrameInterpolatorVK::MakeVulkanStageFlags(FfxResourceStates State)
{
	switch (State)
	{
	case FFX_RESOURCE_STATE_INDIRECT_ARGUMENT:
		return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

	case FFX_RESOURCE_STATE_COPY_SRC:
	case FFX_RESOURCE_STATE_COPY_DEST:
		return VK_PIPELINE_STAGE_TRANSFER_BIT;

	case FFX_RESOURCE_STATE_PRESENT:
		return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	case FFX_RESOURCE_STATE_RENDER_TARGET:
		return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	}

	return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
}
