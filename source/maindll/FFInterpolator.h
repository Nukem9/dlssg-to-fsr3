#pragma once

#include <FidelityFX/host/ffx_frameinterpolation.h>

struct FFInterpolatorDispatchParameters
{
	FfxCommandList CommandList;

    FfxDimensions2D RenderSize;
	FfxDimensions2D OutputSize;

    FfxResource InputColorBuffer;
    FfxResource InputHUDLessColorBuffer;
    FfxResource InputDepth;
    FfxResource InputMotionVectors;

    FfxResource InputOpticalFlowVector;
    FfxResource InputOpticalFlowSceneChangeDetection;
    FfxFloatCoords2D OpticalFlowScale;
    int OpticalFlowBlockSize;

    FfxResource OutputInterpolatedColorBuffer;

	bool MotionVectorsFullResolution;
	bool MotionVectorJitterCancellation;
	bool MotionVectorsDilated;

	FfxFloatCoords2D MotionVectorScale;
	FfxFloatCoords2D MotionVectorJitterOffsets;

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

	const FfxInterface m_BackendInterface;
	std::optional<FfxFrameInterpolationContext> m_FSRContext;

	FfxSurfaceFormat m_InitialBackBufferFormat = {};
	std::optional<FfxResourceInternal> m_InitialPreviousInterpolationSource;

	FfxSurfaceFormat m_BackupBackBufferFormat = {};
	std::optional<FfxResourceInternal> m_BackupPreviousInterpolationSource;

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
