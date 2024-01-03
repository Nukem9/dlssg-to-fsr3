#pragma once

#include <FidelityFX/host/ffx_opticalflow.h>
#include <vulkan/vulkan.h>
#include "FFDilator.h"
#include "FFInterpolator.h"

struct NGXInstanceParameters;
struct ID3D12Device;

class FFFrameInterpolator
{
private:
	ID3D12Device *const DXLogicalDevice = nullptr;
	const VkDevice VKLogicalDevice = {};
	const VkPhysicalDevice VKPhysicalDevice = {};

	const uint32_t SwapchainWidth; // Final image presented to the screen dimensions
	const uint32_t SwapchainHeight;

	std::vector<std::unique_ptr<uint8_t[]>> ScratchMemoryBuffers;
	FfxInterface FrameInterpolationBackendInterface = {};
	FfxInterface SharedBackendInterface = {};
	std::optional<FfxUInt32> m_SharedEffectContextId;

	std::unique_ptr<FFDilator> DilationContext;
	std::optional<FfxOpticalflowContext> OpticalFlowContext;
	std::unique_ptr<FFInterpolator> FrameInterpolatorContext;

	std::optional<FfxResourceInternal> m_TexSharedDilatedDepth;
	std::optional<FfxResourceInternal> m_TexSharedDilatedMotionVectors;
	std::optional<FfxResourceInternal> m_TexSharedPreviousNearestDepth;
	std::optional<FfxResourceInternal> m_TexSharedOpticalFlowVector;
	std::optional<FfxResourceInternal> m_TexSharedOpticalFlowSCD;

	// Transient
	uint32_t m_RenderWidth = 0; // GBuffer dimensions
	uint32_t m_RenderHeight = 0;
	FfxCommandList m_ActiveCommandList = {};

public:
	FFFrameInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight);
	FFFrameInterpolator(VkDevice LogicalDevice, VkPhysicalDevice PhysicalDevice, uint32_t OutputWidth, uint32_t OutputHeight);
	FFFrameInterpolator(const FFFrameInterpolator&) = delete;
	FFFrameInterpolator& operator=(const FFFrameInterpolator&) = delete;
	~FFFrameInterpolator();

public:
	FfxErrorCode Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters);

private:
	bool IsVulkanBackend() const;

	bool BuildDilationParameters(FFDilatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters);
	bool BuildOpticalFlowParameters(FfxOpticalflowDispatchDescription *OutParameters, NGXInstanceParameters *NGXParameters);
	bool BuildFrameInterpolationParameters(FFInterpolatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters);

	void Create();
	void Destroy();
	FfxErrorCode CreateBackend();
	void DestroyBackend();
	FfxErrorCode CreateDilationContext();
	void DestroyDilationContext();
	FfxErrorCode CreateOpticalFlowContext();
	void DestroyOpticalFlowContext();

	bool LoadResourceFromNGXParameters(
		NGXInstanceParameters *NGXParameters,
		const char *Name,
		FfxResource *OutFfxResource,
		FfxResourceStates State);
};
