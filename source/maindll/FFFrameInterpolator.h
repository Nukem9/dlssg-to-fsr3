#pragma once

#include <FidelityFX/host/ffx_opticalflow.h>
#include "FFInterfaceWrapper.h"
#include "FFInterpolator.h"

struct NGXInstanceParameters;

class FFFrameInterpolator
{
private:
	FFInterfaceWrapper m_FrameInterpolationBackendInterface;
	FFInterfaceWrapper m_SharedBackendInterface;
	std::optional<FfxUInt32> m_SharedEffectContextId;

	std::optional<FfxOpticalflowContext> m_OpticalFlowContext;
	std::optional<FFInterpolator> m_FrameInterpolatorContext;

	std::optional<FfxResourceInternal> m_TexSharedDilatedDepth;
	std::optional<FfxResourceInternal> m_TexSharedDilatedMotionVectors;
	std::optional<FfxResourceInternal> m_TexSharedPreviousNearestDepth;
	std::optional<FfxResourceInternal> m_TexSharedOpticalFlowVector;
	std::optional<FfxResourceInternal> m_TexSharedOpticalFlowSCD;

	const uint32_t m_SwapchainWidth; // Final image presented to the screen dimensions
	const uint32_t m_SwapchainHeight;

	FfxFloatCoords2D m_HDRLuminanceRange = { 0.0001f, 1000.0f };
	bool m_HDRLuminanceRangeSet = false;

	// Transient
	uint32_t m_PreUpscaleRenderWidth = 0; // GBuffer dimensions
	uint32_t m_PreUpscaleRenderHeight = 0;

	uint32_t m_PostUpscaleRenderWidth = 0;
	uint32_t m_PostUpscaleRenderHeight = 0;

public:
	FFFrameInterpolator(uint32_t OutputWidth, uint32_t OutputHeight);
	FFFrameInterpolator(const FFFrameInterpolator&) = delete;
	FFFrameInterpolator& operator=(const FFFrameInterpolator&) = delete;
	~FFFrameInterpolator();

	virtual FfxErrorCode Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters);

protected:
	virtual FfxErrorCode InitializeBackendInterface(
		FFInterfaceWrapper *BackendInterface,
		uint32_t MaxContexts,
		NGXInstanceParameters *NGXParameters) = 0;

	virtual std::array<uint8_t, 8> GetActiveAdapterLUID() const = 0;
	virtual FfxCommandList GetActiveCommandList() const = 0;

	virtual void CopyTexture(FfxCommandList CommandList, const FfxResource *Destination, const FfxResource *Source) = 0;

	virtual bool LoadTextureFromNGXParameters(
		NGXInstanceParameters *NGXParameters,
		const char *Name,
		FfxResource *OutFfxResource,
		FfxResourceStates State) = 0;

protected:
	void Create(NGXInstanceParameters *NGXParameters);
	void Destroy();

private:
	bool CalculateResourceDimensions(NGXInstanceParameters *NGXParameters);
	void QueryHDRLuminanceRange(NGXInstanceParameters *NGXParameters);
	bool BuildOpticalFlowParameters(FfxOpticalflowDispatchDescription *OutParameters, NGXInstanceParameters *NGXParameters);
	bool BuildFrameInterpolationParameters(FFInterpolatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters);

	FfxErrorCode CreateBackend(NGXInstanceParameters *NGXParameters);
	void DestroyBackend();
	FfxErrorCode CreateOpticalFlowContext();
	void DestroyOpticalFlowContext();
};
