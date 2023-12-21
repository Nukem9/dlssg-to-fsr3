#pragma once

#include <FidelityFX/host/ffx_frameinterpolation.h>

struct FFInterpolatorDispatchParameters
{
	FfxCommandList CommandList;

    FfxDimensions2D RenderSize;
	FfxDimensions2D OutputSize;

    FfxResource InputColorBuffer;
    FfxResource InputHUDLessColorBuffer;
    FfxResource InputDilatedDepth;
    FfxResource InputDilatedMotionVectors;
    FfxResource InputReconstructedPreviousNearDepth;

    FfxResource InputOpticalFlowVector;
    FfxResource InputOpticalFlowSceneChangeDetection;
    FfxDimensions2D OpticalFlowBufferSize;
    FfxFloatCoords2D OpticalFlowScale;
    int OpticalFlowBlockSize;

    FfxResource OutputColorBuffer;

	bool HDR;
	bool DepthInverted;
	bool Reset;
	bool DebugTearLines;
	bool DebugView;

    float CameraNear;
	float CameraFar;
	float CameraFovAngleVertical;
    float ViewSpaceToMetersFactor;
	FfxFloatCoords2D MinMaxLuminance;
};

class FFInterpolator
{
private:
	const uint32_t m_MaxRenderWidth;
	const uint32_t m_MaxRenderHeight;


	FfxInterface m_BackendInterface = {};
	uint32_t m_EffectContextId = 0;

	FfxFrameInterpolationContext m_Context = {};
	bool m_ContextCreated = false;

	FfxSurfaceFormat m_InitialBackBufferFormat = {};
	FfxResourceInternal m_InitialPreviousInterpolationSource = {};

	FfxSurfaceFormat m_BackupBackBufferFormat = {};
	FfxResourceInternal m_BackupPreviousInterpolationSource = {};

	bool m_BackupBackBufferCreated = false;

public:
	FFInterpolator(const FfxInterface& BackendInterface, uint32_t MaxRenderWidth, uint32_t MaxRenderHeight);
	FFInterpolator(const FFInterpolator&) = delete;
	FFInterpolator& operator=(const FFInterpolator&) = delete;
	~FFInterpolator();

	FfxErrorCode Dispatch(const FFInterpolatorDispatchParameters& Parameters);

private:
	FfxErrorCode InternalDeferredSetupContext(const FFInterpolatorDispatchParameters& Parameters);
	FfxErrorCode InternalSwapResources(FfxSurfaceFormat NewFormat);
};
