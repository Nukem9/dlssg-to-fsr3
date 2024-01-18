#pragma once

#include <FidelityFX/host/ffx_opticalflow.h>
#include <vulkan/vulkan.h>
#include "FFDilator.h"
#include "FFInterfaceWrapper.h"
#include "FFInterpolator.h"

struct NGXInstanceParameters;
struct ID3D12Device;

class FFFrameInterpolator
{
private:
	ID3D12Device *const m_LogicalDeviceDX = nullptr;
	const VkDevice m_LogicalDeviceVK = {};
	const VkPhysicalDevice m_PhysicalDeviceVK = {};

	const uint32_t m_SwapchainWidth; // Final image presented to the screen dimensions
	const uint32_t m_SwapchainHeight;

	FFInterfaceWrapper m_FrameInterpolationBackendInterface;
	FFInterfaceWrapper m_SharedBackendInterface;
	std::optional<FfxUInt32> m_SharedEffectContextId;

	std::unique_ptr<FFDilator> m_DilationContext;
	std::optional<FfxOpticalflowContext> m_OpticalFlowContext;
	std::unique_ptr<FFInterpolator> m_FrameInterpolatorContext;

	std::optional<FfxResourceInternal> m_TexSharedDilatedDepth;
	std::optional<FfxResourceInternal> m_TexSharedDilatedMotionVectors;
	std::optional<FfxResourceInternal> m_TexSharedPreviousNearestDepth;
	std::optional<FfxResourceInternal> m_TexSharedOpticalFlowVector;
	std::optional<FfxResourceInternal> m_TexSharedOpticalFlowSCD;

	FfxFloatCoords2D m_HDRLuminanceRange = { 0.0001f, 1000.0f };
	bool m_HDRLuminanceRangeSet = false;

	// Transient
	uint32_t m_PreUpscaleRenderWidth = 0; // GBuffer dimensions
	uint32_t m_PreUpscaleRenderHeight = 0;

	uint32_t m_PostUpscaleRenderWidth = 0;
	uint32_t m_PostUpscaleRenderHeight = 0;

	FfxCommandList m_ActiveCommandList = {};

public:
	FFFrameInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight, NGXInstanceParameters *NGXParameters);
	FFFrameInterpolator(
		VkDevice LogicalDevice,
		VkPhysicalDevice PhysicalDevice,
		uint32_t OutputWidth,
		uint32_t OutputHeight,
		NGXInstanceParameters *NGXParameters);
	FFFrameInterpolator(const FFFrameInterpolator&) = delete;
	FFFrameInterpolator& operator=(const FFFrameInterpolator&) = delete;
	~FFFrameInterpolator();

public:
	FfxErrorCode Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters);

private:
	bool IsVulkanBackend() const;

	bool CalculateResourceDimensions(NGXInstanceParameters *NGXParameters);
	void QueryHDRLuminanceRange(NGXInstanceParameters *NGXParameters);
	bool BuildDilationParameters(FFDilatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters);
	bool BuildOpticalFlowParameters(FfxOpticalflowDispatchDescription *OutParameters, NGXInstanceParameters *NGXParameters);
	bool BuildFrameInterpolationParameters(FFInterpolatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters);

	void Create(NGXInstanceParameters *NGXParameters);
	void Destroy();
	FfxErrorCode CreateBackend(NGXInstanceParameters *NGXParameters);
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
