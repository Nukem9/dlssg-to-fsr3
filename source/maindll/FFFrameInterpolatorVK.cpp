#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <vulkan/vulkan.h>
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
	  FFFrameInterpolator(OutputWidth, OutputHeight, NGXParameters)
{
}

FFFrameInterpolatorVK::~FFFrameInterpolatorVK()
{
}

FfxErrorCode FFFrameInterpolatorVK::Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters)
{
	const bool enableInterpolation = NGXParameters->GetUIntOrDefault("DLSSG.EnableInterp", 0) != 0;
	const bool isRecordingCommands = NGXParameters->GetUIntOrDefault("DLSSG.IsRecording", 0) != 0;
	const auto cmdListVk = reinterpret_cast<VkCommandBuffer>(CommandList);

	// Begin a new command list in the event our caller didn't set one up
	if (!isRecordingCommands)
	{
		const static bool once = [&]()
		{
			spdlog::warn(
				"Command list wasn't recording. Resetting state: {} 0x{:X}",
				enableInterpolation,
				reinterpret_cast<uintptr_t>(CommandList));

			return false;
		}();

		VkCommandBufferBeginInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

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
	VkDeviceContext vkContext = {
		.vkDevice = m_Device,
		.vkPhysicalDevice = m_PhysicalDevice,
		.vkDeviceProcAddr = nullptr,
	};

	// TODO

	// const auto fsrDevice = ffxGetDeviceVK(&vkContext);
	// const auto scratchSize = ffxGetScratchMemorySizeVK(vkContext.vkPhysicalDevice, maxContexts);

	// FFX_RETURN_ON_FAIL(m_SharedBackendInterface.Initialize(fsrDevice, maxContexts));

	return FFX_ERROR_INCOMPLETE_INTERFACE;
}

FfxCommandList FFFrameInterpolatorVK::GetActiveCommandList() const
{
	return m_ActiveCommandList;
}

std::array<uint8_t, 8> FFFrameInterpolatorVK::GetActiveAdapterLUID() const
{
	VkPhysicalDeviceIDProperties idProperties = {};
	idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

	VkPhysicalDeviceProperties2 properties = {};
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &idProperties;

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

	// Transition
	VkImageMemoryBarrier barriers[2] = {};
	{
		barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barriers[0].srcAccessMask = getVKAccessFlagsFromResourceState(Destination->state);
		barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barriers[0].oldLayout = getVKImageLayoutFromResourceState(Destination->state);
		barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barriers[0].image = static_cast<VkImage>(Destination->resource); // Destination
		barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barriers[0].subresourceRange.baseMipLevel = 0;
		barriers[0].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		barriers[0].subresourceRange.baseArrayLayer = 0;
		barriers[0].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

		barriers[1] = barriers[0];
		barriers[1].srcAccessMask = getVKAccessFlagsFromResourceState(Source->state);
		barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barriers[1].oldLayout = getVKImageLayoutFromResourceState(Source->state);
		barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barriers[1].image = static_cast<VkImage>(Source->resource); // Source

		vkCmdPipelineBarrier(
			cmdListVk,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			2,
			barriers);
	}

	// Copy
	{
		VkImageCopy copyRegion = {};
		copyRegion.extent = { Destination->description.width, Destination->description.height, 1 };

		copyRegion.dstSubresource.aspectMask = barriers[0].subresourceRange.aspectMask;
		copyRegion.dstSubresource.mipLevel = barriers[0].subresourceRange.baseMipLevel;
		copyRegion.dstSubresource.baseArrayLayer = barriers[0].subresourceRange.baseArrayLayer;
		copyRegion.dstSubresource.layerCount = barriers[0].subresourceRange.layerCount;

		copyRegion.srcSubresource.aspectMask = barriers[1].subresourceRange.aspectMask;
		copyRegion.srcSubresource.mipLevel = barriers[1].subresourceRange.baseMipLevel;
		copyRegion.srcSubresource.baseArrayLayer = barriers[1].subresourceRange.baseArrayLayer;
		copyRegion.srcSubresource.layerCount = barriers[1].subresourceRange.layerCount;

		vkCmdCopyImage(cmdListVk, barriers[1].image, barriers[1].newLayout, barriers[0].image, barriers[0].newLayout, 1, &copyRegion);
	}

	// Transition
	{
		std::swap(barriers[0].srcAccessMask, barriers[0].dstAccessMask);
		std::swap(barriers[0].oldLayout, barriers[0].newLayout);
		std::swap(barriers[1].srcAccessMask, barriers[1].dstAccessMask);
		std::swap(barriers[1].oldLayout, barriers[1].newLayout);

		vkCmdPipelineBarrier(
			cmdListVk,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			2,
			barriers);
	}
}

bool FFFrameInterpolatorVK::LoadTextureFromNGXParameters(
	NGXInstanceParameters *NGXParameters,
	const char *Name,
	FfxResource *OutFfxResource,
	FfxResourceStates State)
{
	void *resource = nullptr;
	NGXParameters->GetVoidPointer(Name, &resource);

	if (!resource)
	{
		*OutFfxResource = {};
		return false;
	}

	// Vulkan provides no mechanism to query resource information. Convert it manually.
	auto resourceHandle = static_cast<const NGXVulkanResourceHandle *>(resource);

	if (resourceHandle->Type != 0)
		__debugbreak();

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = resourceHandle->ImageMetadata.Format;
	imageInfo.extent = { resourceHandle->ImageMetadata.Width, resourceHandle->ImageMetadata.Height, 1 };
	imageInfo.mipLevels = resourceHandle->ImageMetadata.Subresource.levelCount;
	imageInfo.arrayLayers = resourceHandle->ImageMetadata.Subresource.layerCount;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	*OutFfxResource = ffxGetResourceVK(resourceHandle->ImageMetadata.Image, GetFfxResourceDescriptionVK(&imageInfo), nullptr, State);
	return true;
}
