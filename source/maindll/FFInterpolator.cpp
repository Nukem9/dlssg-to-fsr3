#include <FidelityFX/host/ffx_frameinterpolation.h>
#include "FFInterpolator.h"

FFInterpolator::FFInterpolator(
	const FfxInterface& BackendInterface,
	const FfxInterface& SharedBackendInterface,
	FfxUInt32 SharedEffectContextId,
	uint32_t MaxRenderWidth,
	uint32_t MaxRenderHeight)
	: m_BackendInterface(BackendInterface),
	  m_SharedBackendInterface(SharedBackendInterface),
	  m_SharedEffectContextId(SharedEffectContextId),
	  m_MaxRenderWidth(MaxRenderWidth),
	  m_MaxRenderHeight(MaxRenderHeight)
{
}

FFInterpolator::~FFInterpolator()
{
	DestroyContext();
}

FfxErrorCode FFInterpolator::Dispatch(const FFInterpolatorDispatchParameters& Parameters)
{
	if (auto status = CreateContextDeferred(Parameters); status != FFX_OK) // Massive frame hitch on first call
		return status;

	FfxFrameInterpolationDispatchDescription dispatchDesc = {};
	{
		if (Parameters.DebugTearLines)
			dispatchDesc.flags |= FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_TEAR_LINES;

		if (Parameters.DebugView)
			dispatchDesc.flags |= FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_VIEW;

		dispatchDesc.commandList = Parameters.CommandList;
		dispatchDesc.displaySize = Parameters.OutputSize;
		dispatchDesc.renderSize = Parameters.RenderSize;

		dispatchDesc.currentBackBuffer = Parameters.InputColorBuffer;
		dispatchDesc.currentBackBuffer_HUDLess = Parameters.InputHUDLessColorBuffer;
		dispatchDesc.output = Parameters.OutputInterpolatedColorBuffer;

		dispatchDesc.interpolationRect = { 0,
										   0,
										   static_cast<int>(dispatchDesc.displaySize.width),
										   static_cast<int>(dispatchDesc.displaySize.height) };

		dispatchDesc.opticalFlowVector = Parameters.InputOpticalFlowVector;
		dispatchDesc.opticalFlowSceneChangeDetection = Parameters.InputOpticalFlowSceneChangeDetection;
		// dispatchDesc.opticalFlowBufferSize = Parameters.OpticalFlowBufferSize; // Completely unused?
		dispatchDesc.opticalFlowScale = Parameters.OpticalFlowScale;
		dispatchDesc.opticalFlowBlockSize = Parameters.OpticalFlowBlockSize;

		dispatchDesc.cameraNear = Parameters.CameraNear;
		dispatchDesc.cameraFar = Parameters.CameraFar;
		dispatchDesc.cameraFovAngleVertical = Parameters.CameraFovAngleVertical;
		dispatchDesc.viewSpaceToMetersFactor = 1.0f;

		dispatchDesc.frameTimeDelta = 1000.0f / 60.0f; // Unused
		dispatchDesc.reset = Parameters.Reset;

		dispatchDesc.backBufferTransferFunction = Parameters.HDR ? FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ
																 : FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
		dispatchDesc.minMaxLuminance[0] = Parameters.MinMaxLuminance.x;
		dispatchDesc.minMaxLuminance[1] = Parameters.MinMaxLuminance.y;

		dispatchDesc.frameID = 0; // Not async and not bindless. Don't bother.

		dispatchDesc.dilatedDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_DilatedDepth);
		dispatchDesc.dilatedMotionVectors = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_DilatedMotionVectors);
		dispatchDesc.reconstructedPrevDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_ReconstructedPrevDepth);
		dispatchDesc.distortionField = Parameters.InputDistortionField;
	}

	FfxFrameInterpolationPrepareDescription prepareDesc = {};
	{
		prepareDesc.flags = dispatchDesc.flags;
		prepareDesc.commandList = dispatchDesc.commandList;
		prepareDesc.renderSize = dispatchDesc.renderSize;
		prepareDesc.jitterOffset = Parameters.MotionVectorJitterOffsets;
		prepareDesc.motionVectorScale = Parameters.MotionVectorScale;

		prepareDesc.frameTimeDelta = dispatchDesc.frameTimeDelta;
		prepareDesc.cameraNear = dispatchDesc.cameraNear;
		prepareDesc.cameraFar = dispatchDesc.cameraFar;
		prepareDesc.viewSpaceToMetersFactor = 1.0f;
		prepareDesc.cameraFovAngleVertical = dispatchDesc.cameraFovAngleVertical;

		prepareDesc.depth = Parameters.InputDepth;
		prepareDesc.motionVectors = Parameters.InputMotionVectors;

		prepareDesc.frameID = dispatchDesc.frameID;

		prepareDesc.dilatedDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_DilatedDepth);
		prepareDesc.dilatedMotionVectors = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_DilatedMotionVectors);
		prepareDesc.reconstructedPrevDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_ReconstructedPrevDepth);
	}

	if (auto status = ffxFrameInterpolationPrepare(&m_FSRContext.value(), &prepareDesc); status != FFX_OK)
		return status;

	return ffxFrameInterpolationDispatch(&m_FSRContext.value(), &dispatchDesc);
}

FfxErrorCode FFInterpolator::CreateContextDeferred(const FFInterpolatorDispatchParameters& Parameters)
{
	FfxFrameInterpolationContextDescription desc = {};
	desc.backendInterface = m_BackendInterface;

	if (Parameters.DepthInverted)
		desc.flags |= FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INVERTED;

	if (Parameters.DepthPlaneInfinite)
		desc.flags |= FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INFINITE;

	if (Parameters.HDR)
		desc.flags |= FFX_FRAMEINTERPOLATION_ENABLE_HDR_COLOR_INPUT;

	if (Parameters.MotionVectorsFullResolution)
		desc.flags |= FFX_FRAMEINTERPOLATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS;

	if (Parameters.MotionVectorJitterCancellation)
		desc.flags |= FFX_FRAMEINTERPOLATION_ENABLE_JITTER_MOTION_VECTORS;

	if (Parameters.MotionVectorsDilated)
		desc.flags |= FFX_FRAMEINTERPOLATION_ENABLE_PREDILATED_MOTION_VECTORS;

	desc.maxRenderSize = { m_MaxRenderWidth, m_MaxRenderHeight };
	desc.displaySize = desc.maxRenderSize;

	desc.backBufferFormat = Parameters.InputColorBuffer.description.format;
	desc.previousInterpolationSourceFormat = desc.backBufferFormat;

	if (Parameters.InputHUDLessColorBuffer.resource)
		desc.previousInterpolationSourceFormat = Parameters.InputHUDLessColorBuffer.description.format;

	if (std::exchange(m_ContextFlushPending, false))
		DestroyContext();

	if (m_FSRContext)
	{
		if (memcmp(&desc, &m_ContextDescription, sizeof(m_ContextDescription)) == 0)
			return FFX_OK;

		m_ContextFlushPending = true;
		return FFX_EOF; // Description changed. Return fake status to request a flush from our parent.
	}

	m_ContextDescription = desc;
	auto status = ffxFrameInterpolationContextCreate(&m_FSRContext.emplace(), &m_ContextDescription);

	if (status != FFX_OK)
	{
		m_FSRContext.reset();
		return status;
	}

	FfxFrameInterpolationSharedResourceDescriptions fsrFiSharedDescriptions = {};
	status = ffxFrameInterpolationGetSharedResourceDescriptions(&m_FSRContext.value(), &fsrFiSharedDescriptions);

	if (status != FFX_OK)
	{
		DestroyContext();
		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&fsrFiSharedDescriptions.dilatedDepth,
		m_SharedEffectContextId,
		&m_DilatedDepth.emplace());

	if (status != FFX_OK)
	{
		m_DilatedDepth.reset();
		DestroyContext();

		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&fsrFiSharedDescriptions.dilatedMotionVectors,
		m_SharedEffectContextId,
		&m_DilatedMotionVectors.emplace());

	if (status != FFX_OK)
	{
		m_DilatedMotionVectors.reset();
		DestroyContext();

		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&fsrFiSharedDescriptions.reconstructedPrevNearestDepth,
		m_SharedEffectContextId,
		&m_ReconstructedPrevDepth.emplace());

	if (status != FFX_OK)
	{
		m_ReconstructedPrevDepth.reset();
		DestroyContext();

		return status;
	}

	return FFX_OK;
}

void FFInterpolator::DestroyContext()
{
	if (m_FSRContext)
		ffxFrameInterpolationContextDestroy(&m_FSRContext.value());

	if (m_DilatedDepth)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_DilatedDepth, m_SharedEffectContextId);

	if (m_DilatedMotionVectors)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_DilatedMotionVectors, m_SharedEffectContextId);

	if (m_ReconstructedPrevDepth)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_ReconstructedPrevDepth, m_SharedEffectContextId);

	m_FSRContext.reset();
	m_DilatedDepth.reset();
	m_DilatedMotionVectors.reset();
	m_ReconstructedPrevDepth.reset();
}
