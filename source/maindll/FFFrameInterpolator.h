#pragma once

#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <FidelityFX/host/ffx_opticalflow.h>
#include <FidelityFX/gpu/fsr3/ffx_fsr3_resources.h>
#include "FFDilator.h"
#include "FFInterpolator.h"

struct NGXVulkanResourceHandle;
class NGXInstanceParameters;

struct ID3D12Device;

class FFFrameInterpolator
{
private:
	const bool VulkanBackend;

	ID3D12Device *const DXLogicalDevice = nullptr;

	VkDevice VKLogicalDevice = {};
	VkPhysicalDevice VKPhysicalDevice = {};

	const uint32_t SwapchainWidth; // Final image presented to the screen
	const uint32_t SwapchainHeight;

	uint32_t RenderWidth = 0; // GBuffer dimensions
	uint32_t RenderHeight = 0;

	FfxInterface FrameInterpolationBackendInterface = {};
	FfxInterface SharedBackendInterface = {};
	FfxUInt32 SharedBackendEffectContextId = 0;

	std::unique_ptr<FFDilator> DilationContext;
	FfxOpticalflowContext OpticalFlowContext = {};
	std::unique_ptr<FFInterpolator> FrameInterpolatorContext;

	FfxResourceInternal GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_COUNT] = {};
	std::vector<std::unique_ptr<uint8_t[]>> ScratchMemoryBuffers;

	FfxCommandList ActiveCommandList = {};

public:
	FFFrameInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight);
	FFFrameInterpolator(VkDevice LogicalDevice, VkPhysicalDevice PhysicalDevice, uint32_t OutputWidth, uint32_t OutputHeight);
	FFFrameInterpolator(const FFFrameInterpolator&) = delete;
	FFFrameInterpolator& operator=(const FFFrameInterpolator&) = delete;
	~FFFrameInterpolator();

public:
	FfxErrorCode Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters);

private:
	bool BuildDilationParameters(FFDilatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters);
	bool BuildOpticalFlowParameters(FfxOpticalflowDispatchDescription *OutParameters, NGXInstanceParameters *NGXParameters);
	bool BuildFrameInterpolationParameters(FFInterpolatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters);

	FfxErrorCode Create();
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
