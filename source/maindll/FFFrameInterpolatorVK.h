#pragma once

#include <vulkan/vulkan.h>
#include "FFFrameInterpolator.h"

class FFFrameInterpolatorVK final : public FFFrameInterpolator
{
private:
	const VkDevice m_Device;
	const VkPhysicalDevice m_PhysicalDevice;

	// Transient
	FfxCommandList m_ActiveCommandList = {};

public:
	FFFrameInterpolatorVK(
		VkDevice LogicalDevice,
		VkPhysicalDevice PhysicalDevice,
		uint32_t OutputWidth,
		uint32_t OutputHeight,
		NGXInstanceParameters *NGXParameters);
	FFFrameInterpolatorVK(const FFFrameInterpolatorVK&) = delete;
	FFFrameInterpolatorVK& operator=(const FFFrameInterpolatorVK&) = delete;
	~FFFrameInterpolatorVK();

	FfxErrorCode Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters) override;

private:
	FfxErrorCode InitializeBackendInterface(
		FFInterfaceWrapper *BackendInterface,
		uint32_t MaxContexts,
		NGXInstanceParameters *NGXParameters) override;

	std::array<uint8_t, 8> GetActiveAdapterLUID() const override;
	FfxCommandList GetActiveCommandList() const override;

	void CopyTexture(FfxCommandList CommandList, const FfxResource *Destination, const FfxResource *Source) override;

	bool LoadTextureFromNGXParameters(
		NGXInstanceParameters *NGXParameters,
		const char *Name,
		FfxResource *OutFfxResource,
		FfxResourceStates State) override;

	static VkImageMemoryBarrier MakeVulkanBarrier(
		VkImage Resource,
		FfxResourceStates SourceState,
		FfxResourceStates DestinationState,
		bool IsDepthAspect);
};
