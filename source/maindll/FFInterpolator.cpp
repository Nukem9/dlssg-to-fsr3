#include <FidelityFX/host/ffx_frameinterpolation.h>
#include <frameinterpolation/ffx_frameinterpolation_private.h>
#include "FFInterpolator.h"

FFInterpolator::FFInterpolator(const FfxInterface& BackendInterface, uint32_t MaxRenderWidth, uint32_t MaxRenderHeight)
	: m_BackendInterface(BackendInterface),
	  m_MaxRenderWidth(MaxRenderWidth),
	  m_MaxRenderHeight(MaxRenderHeight)
{
}

FFInterpolator::~FFInterpolator()
{
	if (m_FSRContext)
	{
		if (m_BackupPreviousInterpolationSource)
		{
			const uint32_t resourceId = FFX_FRAMEINTERPOLATION_RESOURCE_IDENTIFIER_PREVIOUS_INTERPOLATION_SOURCE;
			auto context = reinterpret_cast<FfxFrameInterpolationContext_Private *>(&m_FSRContext.value());

			context->srvResources[resourceId] = *m_InitialPreviousInterpolationSource;
			context->uavResources[resourceId] = *m_InitialPreviousInterpolationSource;

			context->contextDescription.backendInterface.fpDestroyResource(
				&context->contextDescription.backendInterface,
				*m_BackupPreviousInterpolationSource,
				context->effectContextId);
		}

		ffxFrameInterpolationContextDestroy(&m_FSRContext.value());
	}
}

FfxErrorCode FFInterpolator::Dispatch(const FFInterpolatorDispatchParameters& Parameters)
{
	if (auto status = InternalDeferredSetupContext(Parameters); status != FFX_OK) // Massive frame hitch on first call
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
		dispatchDesc.viewSpaceToMetersFactor = Parameters.ViewSpaceToMetersFactor;

		dispatchDesc.frameTimeDelta = 1000.0f / 60.0f; // Unused
		dispatchDesc.reset = Parameters.Reset;

		dispatchDesc.backBufferTransferFunction = Parameters.HDR ? FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ
																 : FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
		dispatchDesc.minMaxLuminance[0] = Parameters.MinMaxLuminance.x;
		dispatchDesc.minMaxLuminance[1] = Parameters.MinMaxLuminance.y;

		dispatchDesc.frameID = 0; // Not async and not bindless. Don't bother.
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
		prepareDesc.viewSpaceToMetersFactor = dispatchDesc.viewSpaceToMetersFactor;
		prepareDesc.cameraFovAngleVertical = dispatchDesc.cameraFovAngleVertical;

		prepareDesc.depth = Parameters.InputDepth;
		prepareDesc.motionVectors = Parameters.InputMotionVectors;

		prepareDesc.frameID = dispatchDesc.frameID;
	}

	if (auto status = ffxFrameInterpolationPrepare(&m_FSRContext.value(), &prepareDesc); status != FFX_OK)
		return status;

	return ffxFrameInterpolationDispatch(&m_FSRContext.value(), &dispatchDesc);
}

FfxErrorCode FFInterpolator::InternalDeferredSetupContext(const FFInterpolatorDispatchParameters& Parameters)
{
	if (!m_FSRContext)
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

		// If provided, pretend the input hudless color format is the back buffer format
		desc.backBufferFormat = Parameters.InputColorBuffer.description.format;

		if (Parameters.InputHUDLessColorBuffer.resource)
			desc.backBufferFormat = Parameters.InputHUDLessColorBuffer.description.format;

		const auto status = ffxFrameInterpolationContextCreate(&m_FSRContext.emplace(), &desc);

		if (status != FFX_OK)
		{
			m_FSRContext.reset();
			return status;
		}

		m_InitialBackBufferFormat = desc.backBufferFormat;
	}

	// What FFX calls "backBufferFormat" is actually the format of the input color buffer. The input
	// color buffer could be the back buffer, or it could be a HUD-less equivalent. Either way, code
	// assumes they're identical formats and trips an assert if they're not. The assert is bogus since
	// AMD's new implementation is format-agnostic. We can work around this limitation by swapping
	// internal resource handles around.
	auto& currentInputColor = Parameters.InputHUDLessColorBuffer.resource ? Parameters.InputHUDLessColorBuffer
																		  : Parameters.InputColorBuffer;

	return InternalSwapResources(currentInputColor.description.format);
}

FfxErrorCode FFInterpolator::InternalSwapResources(FfxSurfaceFormat NewFormat)
{
	const uint32_t resourceId = FFX_FRAMEINTERPOLATION_RESOURCE_IDENTIFIER_PREVIOUS_INTERPOLATION_SOURCE;
	auto context = reinterpret_cast<FfxFrameInterpolationContext_Private *>(&m_FSRContext.value());

	// if (use original allocation)
	if (NewFormat == m_InitialBackBufferFormat)
	{
		if (m_BackupPreviousInterpolationSource)
		{
			context->srvResources[resourceId] = *m_InitialPreviousInterpolationSource;
			context->uavResources[resourceId] = *m_InitialPreviousInterpolationSource;
		}

		return FFX_OK;
	}

	// if (new allocation required)
	if (NewFormat == m_BackupBackBufferFormat || !m_BackupPreviousInterpolationSource)
	{
		// Create an exact texture copy but with the new format
		if (!m_BackupPreviousInterpolationSource)
		{
			auto newCopyDescription = context->contextDescription.backendInterface.fpGetResourceDescription(
				&context->contextDescription.backendInterface,
				context->srvResources[resourceId]);

			m_BackupBackBufferFormat = NewFormat;
			newCopyDescription.format = m_BackupBackBufferFormat;

			// clang-format off
			const FfxCreateResourceDescription createResourceDescription =
			{
				FFX_HEAP_TYPE_DEFAULT,
				newCopyDescription,
				FFX_RESOURCE_STATE_UNORDERED_ACCESS,
				L"FI_PreviousInterpolationSourceHACK",
				resourceId,
				{ FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED }
			};
			// clang-format on

			const auto status = context->contextDescription.backendInterface.fpCreateResource(
				&context->contextDescription.backendInterface,
				&createResourceDescription,
				context->effectContextId,
				&m_BackupPreviousInterpolationSource.emplace());

			if (status != FFX_OK)
			{
				m_BackupPreviousInterpolationSource.reset();
				return status;
			}

			m_InitialPreviousInterpolationSource = context->srvResources[resourceId];
		}

		context->srvResources[resourceId] = *m_BackupPreviousInterpolationSource;
		context->uavResources[resourceId] = *m_BackupPreviousInterpolationSource;

		return FFX_OK;
	}

	return FFX_ERROR_INVALID_ARGUMENT;
}
