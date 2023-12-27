#pragma once

#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include <FidelityFX/host/ffx_opticalflow.h>
#include <FidelityFX/gpu/fsr3/ffx_fsr3_resources.h>
#include "FFDilator.h"
#include "FFInterpolator.h"

class NGXInstanceParameters;
struct ID3D12Device;

class FFFrameInterpolator
{
private:
	ID3D12Device *const Device;

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

public:
	FFFrameInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight);
	FFFrameInterpolator(const FFFrameInterpolator&) = delete;
	FFFrameInterpolator& operator=(const FFFrameInterpolator&) = delete;
	~FFFrameInterpolator();

public:
	FfxErrorCode Dispatch(ID3D12GraphicsCommandList *CommandList, NGXInstanceParameters *NGXParameters);

private:
	bool BuildDilationParameters(
		FFDilatorDispatchParameters *OutParameters,
		ID3D12GraphicsCommandList *CommandList,
		NGXInstanceParameters *NGXParameters);

	bool BuildOpticalFlowParameters(
		FfxOpticalflowDispatchDescription *OutParameters,
		ID3D12GraphicsCommandList *CommandList,
		NGXInstanceParameters *NGXParameters);

	bool BuildFrameInterpolationParameters(
		FFInterpolatorDispatchParameters *OutParameters,
		ID3D12GraphicsCommandList *CommandList,
		NGXInstanceParameters *NGXParameters);

	static bool LoadResourceFromNGXParameters(
		NGXInstanceParameters *NGXParameters,
		const char *Name,
		FfxResource *OutFfxResource,
		FfxResourceStates State);

	FfxErrorCode CreateBackend();
	void DestroyBackend();
	FfxErrorCode CreateDilationContext();
	void DestroyDilationContext();
	FfxErrorCode CreateOpticalFlowContext();
	void DestroyOpticalFlowContext();
};
