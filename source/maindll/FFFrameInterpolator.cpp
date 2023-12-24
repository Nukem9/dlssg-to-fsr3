#include <numbers>
#include "nvngx.h"
#include "FFExt.h"
#include "Util.h"
#include "FFFrameInterpolator.h"

#ifdef _DEBUG
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_fsr3_x64d.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_backend_dx12_x64d.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_frameinterpolation_x64d.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_opticalflow_x64d.lib")
#else
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_fsr3_x64.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_backend_dx12_x64.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_frameinterpolation_x64.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_opticalflow_x64.lib")
#endif

D3D12_RESOURCE_STATES ffxGetDX12StateFromResourceState(FfxResourceStates state);

FFFrameInterpolator::FFFrameInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight, DXGI_FORMAT BackBufferFormat)
	: Device(Device),
	  SwapchainWidth(OutputWidth),
	  SwapchainHeight(OutputHeight)
{
	Device->AddRef();

	if (!FF_SUCCEEDED(CreateBackend()))
		throw std::runtime_error("FFFrameInterpolator: Failed to create backend context.");

	if (!FF_SUCCEEDED(CreateDilationContext()))
		throw std::runtime_error("FFFrameInterpolator: Failed to create dilation context.");

	if (!FF_SUCCEEDED(CreateOpticalFlowContext()))
		throw std::runtime_error("FFFrameInterpolator: Failed to create optical flow context.");

	FrameInterpolatorContext = std::make_unique<FFInterpolator>(FrameInterpolationBackendInterface, SwapchainWidth, SwapchainHeight);
}

FFFrameInterpolator::~FFFrameInterpolator()
{
	FrameInterpolatorContext.reset();
	DestroyOpticalFlowContext();
	DestroyDilationContext();
	DestroyBackend();

	Device->Release();
}

FfxErrorCode FFFrameInterpolator::Dispatch(ID3D12GraphicsCommandList *CommandList, NGXInstanceParameters *NGXParameters)
{
	const bool enableInterpolation = NGXParameters->GetUIntOrDefault("DLSSG.EnableInterp", 0) != 0;
	const bool isRecordingCommands = NGXParameters->GetUIntOrDefault("DLSSG.IsRecording", 0) != 0;
	FfxResource ffxRealOutputResource = {};

	if (!isRecordingCommands)
	{
		ID3D12CommandQueue *recordingQueue = nullptr;
		NGXParameters->GetVoidPointer("DLSSG.CmdQueue", reinterpret_cast<void **>(&recordingQueue));

		ID3D12CommandAllocator *recordingAllocator = nullptr;
		NGXParameters->GetVoidPointer("DLSSG.CmdAlloc", reinterpret_cast<void **>(&recordingAllocator));

		static bool once = [&]()
		{
			spdlog::warn(
				"Command list wasn't recording. Resetting state: {} 0x{:X} 0x{:X} 0x{:X}",
				enableInterpolation,
				reinterpret_cast<uintptr_t>(CommandList),
				reinterpret_cast<uintptr_t>(recordingQueue),
				reinterpret_cast<uintptr_t>(recordingAllocator));

			return false;
		}();

		CommandList->Reset(recordingAllocator, nullptr);
	}

	const auto dispatchStatus = [&]() -> FfxErrorCode
	{
		if (!enableInterpolation)
			return FFX_OK;

		// As far as I know there aren't any direct ways to fetch the current gbuffer dimensions from NGX. I guess
		// pulling it from depth buffer extents is enough.
		{
			ID3D12Resource *depthResource = nullptr;
			NGXParameters->GetVoidPointer("DLSSG.Depth", reinterpret_cast<void **>(&depthResource));

			const auto desc = depthResource->GetDesc();
			RenderWidth = NGXParameters->GetUIntOrDefault("DLSSG.DepthSubrectWidth", static_cast<uint32_t>(desc.Width));
			RenderHeight = NGXParameters->GetUIntOrDefault("DLSSG.DepthSubrectHeight", desc.Height);
		}

		if (RenderWidth <= 32 || RenderHeight <= 32)
			return FFX_ERROR_INVALID_ARGUMENT;

		//
		// Parameter setup
		//
		// Depth, MV dilation, and previous depth reconstruction
		FFDilatorDispatchParameters fsrDilationDesc = {};
		if (!BuildDilationParameters(&fsrDilationDesc, CommandList, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		// Optical flow
		FfxOpticalflowDispatchDescription fsrOfDispatchDesc = {};
		if (!BuildOpticalFlowParameters(&fsrOfDispatchDesc, CommandList, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		// Frame interpolation
		FFInterpolatorDispatchParameters fsrFiDispatchDesc = {};
		if (!BuildFrameInterpolationParameters(&fsrFiDispatchDesc, CommandList, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		const static bool doDebugOverlay = Util::GetSetting(L"Debug", L"EnableDebugOverlay", false);
		const static bool doDebugTearLines = Util::GetSetting(L"Debug", L"EnableDebugTearLines", false);
		const static bool doInterpolatedOnly = Util::GetSetting(L"Debug", L"EnableInterpolatedFramesOnly", false);

		fsrFiDispatchDesc.DebugView = doDebugOverlay;
		fsrFiDispatchDesc.DebugTearLines = doDebugTearLines;

#if 0
		auto setObjectDebugName = [](ID3D12Object *Object, const char *Name)
		{
			if (!Object || !Name || strlen(Name) <= 0)
				return;

			wchar_t tempOut[1024];
			if (mbstowcs_s(nullptr, tempOut, Name, _TRUNCATE) == 0)
				Object->SetName(tempOut);
		};

		setObjectDebugName(static_cast<ID3D12Resource *>(fsrDilationDesc.InputDepth.resource), "DLSSG.Depth");
		setObjectDebugName(static_cast<ID3D12Resource *>(fsrDilationDesc.InputMotionVectors.resource), "DLSSG.MVecs");
		setObjectDebugName(static_cast<ID3D12Resource *>(fsrFiDispatchDesc.InputHUDLessColorBuffer.resource), "DLSSG.HUDLess");
		setObjectDebugName(static_cast<ID3D12Resource *>(fsrFiDispatchDesc.InputColorBuffer.resource), "DLSSG.Backbuffer");
		setObjectDebugName(static_cast<ID3D12Resource *>(fsrFiDispatchDesc.OutputInterpolatedColorBuffer.resource), "DLSSG.OutputInterpolated");

		//setObjectDebugName(outputRealResource, "DLSSG.OutputReal");
#endif

		//
		// Main pass dispatches
		//
		auto status = DilationContext->Dispatch(fsrDilationDesc);
		FFX_RETURN_ON_FAIL(status);

		status = ffxOpticalflowContextDispatch(&OpticalFlowContext, &fsrOfDispatchDesc);
		FFX_RETURN_ON_FAIL(status);

		status = FrameInterpolatorContext->Dispatch(fsrFiDispatchDesc);
		FFX_RETURN_ON_FAIL(status);

		if (fsrFiDispatchDesc.DebugView || doInterpolatedOnly)
			ffxRealOutputResource = fsrFiDispatchDesc.OutputInterpolatedColorBuffer;
		else
			ffxRealOutputResource = fsrFiDispatchDesc.InputColorBuffer;

		return FFX_OK;
	}();

	// Command list state has to be restored before we can return an error code. Don't bother
	// adding copy commands when dispatch fails.
	if (dispatchStatus == FFX_OK)
	{
		ID3D12Resource *dlssgRealOutputResource = nullptr;
		NGXParameters->GetVoidPointer("DLSSG.OutputReal", reinterpret_cast<void **>(&dlssgRealOutputResource));

		if (dlssgRealOutputResource)
		{
			D3D12_RESOURCE_BARRIER barriers[2] = {};
			barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[0].Transition.pResource = dlssgRealOutputResource; // Destination
			barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

			barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[1].Transition.pResource = static_cast<ID3D12Resource *>(ffxRealOutputResource.resource); // Source
			barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barriers[1].Transition.StateBefore = ffxGetDX12StateFromResourceState(ffxRealOutputResource.state);
			barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

			CommandList->ResourceBarrier(2, barriers);
			CommandList->CopyResource(barriers[0].Transition.pResource, barriers[1].Transition.pResource);
			std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
			std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
			CommandList->ResourceBarrier(2, barriers);
		}
	}

	if (!isRecordingCommands)
	{
		// Finish what we started
		CommandList->Close();
	}

	NGXParameters->Set4("DLSSG.FlushRequired", 0);
	return dispatchStatus;
}

bool FFFrameInterpolator::BuildDilationParameters(
	FFDilatorDispatchParameters *OutParameters,
	ID3D12GraphicsCommandList *CommandList,
	NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.CommandList = ffxGetCommandListDX12(CommandList);

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Depth", &desc.InputDepth, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.MVecs", &desc.InputMotionVectors, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	desc.OutputDilatedDepth = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0]);

	desc.OutputDilatedMotionVectors = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0]);

	desc.OutputReconstructedPrevNearestDepth = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0]);

	desc.RenderSize = { RenderWidth, RenderHeight };
	desc.OutputSize = { SwapchainWidth, SwapchainHeight };

	desc.HDR = NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) != 0;
	desc.DepthInverted = NGXParameters->GetUIntOrDefault("DLSSG.DepthInverted", 0) != 0;

	// clang-format off
	const FfxDimensions2D mvecExtents =
	{
		NGXParameters->GetUIntOrDefault("DLSSG.MVecsSubrectWidth", desc.InputMotionVectors.description.width),
		NGXParameters->GetUIntOrDefault("DLSSG.MVecsSubrectHeight", desc.InputMotionVectors.description.height),
	};

	desc.MotionVectorScale =
	{
		NGXParameters->GetFloatOrDefault("DLSSG.MvecScaleX", 0),
		NGXParameters->GetFloatOrDefault("DLSSG.MvecScaleY", 0),
	};

	desc.MotionVectorJitterOffsets =
	{
		NGXParameters->GetFloatOrDefault("DLSSG.JitterOffsetX", 0),
		NGXParameters->GetFloatOrDefault("DLSSG.JitterOffsetY", 0),
	};

	desc.MotionVectorJitterCancellation = NGXParameters->GetUIntOrDefault("DLSSG.MVecJittered", 0) != 0;
	desc.MotionVectorsFullResolution = desc.OutputSize.width == mvecExtents.width && desc.OutputSize.height == mvecExtents.height;
	// clang-format on

	return true;
}

bool FFFrameInterpolator::BuildOpticalFlowParameters(
	FfxOpticalflowDispatchDescription *OutParameters,
	ID3D12GraphicsCommandList *CommandList,
	NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.commandList = ffxGetCommandListDX12(CommandList);

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &desc.color, FFX_RESOURCE_STATE_COPY_DEST) &&
		!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &desc.color, FFX_RESOURCE_STATE_COMPUTE_READ))
		return false;

	desc.opticalFlowVector = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]);

	desc.opticalFlowSCD = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]);

	desc.reset = NGXParameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;

	if (NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) == 0)
		desc.backbufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
	else
		desc.backbufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ;

	desc.minMaxLuminance.x = 0.00001f; // TODO
	desc.minMaxLuminance.y = 1000.0f;  // TODO

	return true;
}

bool FFFrameInterpolator::BuildFrameInterpolationParameters(
	FFInterpolatorDispatchParameters *OutParameters,
	ID3D12GraphicsCommandList *CommandList,
	NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.CommandList = ffxGetCommandListDX12(CommandList);

	LoadResourceFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &desc.InputHUDLessColorBuffer, FFX_RESOURCE_STATE_COPY_DEST);

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &desc.InputColorBuffer, FFX_RESOURCE_STATE_COMPUTE_READ) &&
		!desc.InputHUDLessColorBuffer.resource)
		return false;

	if (!LoadResourceFromNGXParameters(
			NGXParameters,
			"DLSSG.OutputInterpolated",
			&desc.OutputInterpolatedColorBuffer,
			FFX_RESOURCE_STATE_UNORDERED_ACCESS))
		return false;

	desc.InputDilatedDepth = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0]);

	desc.InputDilatedMotionVectors = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0]);

	desc.InputReconstructedPreviousNearDepth = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0]);

	desc.InputOpticalFlowVector = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]);

	desc.InputOpticalFlowSceneChangeDetection = SharedBackendInterface.fpGetResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]);

	desc.RenderSize = { RenderWidth, RenderHeight };
	desc.OutputSize = { SwapchainWidth, SwapchainHeight };

	desc.OpticalFlowBufferSize = { desc.InputOpticalFlowVector.description.width, desc.InputOpticalFlowVector.description.height };
	desc.OpticalFlowBlockSize = 8;
	desc.OpticalFlowScale = { 1.0f / desc.OutputSize.width, 1.0f / desc.OutputSize.height };

	// Games require a deg2rad fixup because...reasons
	desc.CameraFovAngleVertical = NGXParameters->GetFloatOrDefault("DLSSG.CameraFOV", 0);

	if (desc.CameraFovAngleVertical > 10.0f)
		desc.CameraFovAngleVertical *= std::numbers::pi_v<float> / 180.0f;

	desc.CameraNear = NGXParameters->GetFloatOrDefault("DLSSG.CameraNear", 0);
	desc.CameraFar = NGXParameters->GetFloatOrDefault("DLSSG.CameraFar", 0);
	desc.ViewSpaceToMetersFactor = 0.0f; // TODO

	// Generic flags...
	desc.HDR = NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) != 0;
	desc.DepthInverted = NGXParameters->GetUIntOrDefault("DLSSG.DepthInverted", 0) != 0;
	desc.Reset = NGXParameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;

	// HDR nits
	desc.MinMaxLuminance = { 0.00001f, 1000.0f }; // TODO

	return true;
}

bool FFFrameInterpolator::LoadResourceFromNGXParameters(
	NGXInstanceParameters *NGXParameters,
	const char *Name,
	FfxResource *OutFfxResource,
	FfxResourceStates State)
{
	// FSR blatantly ignores the FfxResource size fields. I'm not even going to try.
	ID3D12Resource *resource = nullptr;
	NGXParameters->GetVoidPointer(Name, reinterpret_cast<void **>(&resource));

	if (resource)
	{
		*OutFfxResource = ffxGetResourceDX12(resource, GetFfxResourceDescriptionDX12(resource), nullptr, State);
		return true;
	}

	*OutFfxResource = {};
	return false;
}

FfxErrorCode FFFrameInterpolator::CreateBackend()
{
	const auto fsrDevice = ffxGetDeviceDX12(Device);
	const auto scratchBufferSize = ffxGetScratchMemorySizeDX12(3); // Assume 3 contexts per interface

	auto& buffer1 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchBufferSize));
	FFX_RETURN_ON_FAIL(ffxGetInterfaceDX12(&SharedBackendInterface, fsrDevice, buffer1.get(), scratchBufferSize, 3));

	auto& buffer2 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchBufferSize));
	FFX_RETURN_ON_FAIL(ffxGetInterfaceDX12(&FrameInterpolationBackendInterface, fsrDevice, buffer2.get(), scratchBufferSize, 3));

	FFX_RETURN_ON_FAIL(SharedBackendInterface.fpCreateBackendContext(&SharedBackendInterface, &SharedBackendEffectContextId));

	return FFX_OK;
}

void FFFrameInterpolator::DestroyBackend()
{
	SharedBackendInterface.fpDestroyBackendContext(&SharedBackendInterface, SharedBackendEffectContextId);
}

FfxErrorCode FFFrameInterpolator::CreateDilationContext()
{
	DilationContext = std::make_unique<FFDilator>(SharedBackendInterface, SwapchainWidth, SwapchainHeight);
	const auto resourceDescs = DilationContext->GetSharedResourceDescriptions();

	FFX_RETURN_ON_FAIL(SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&resourceDescs.dilatedDepth,
		SharedBackendEffectContextId,
		&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0]));

	FFX_RETURN_ON_FAIL(SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&resourceDescs.dilatedMotionVectors,
		SharedBackendEffectContextId,
		&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0]));

	FFX_RETURN_ON_FAIL(SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&resourceDescs.reconstructedPrevNearestDepth,
		SharedBackendEffectContextId,
		&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0]));

	return FFX_OK;
}

void FFFrameInterpolator::DestroyDilationContext()
{
	DilationContext.reset();

	SharedBackendInterface.fpDestroyResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0],
		SharedBackendEffectContextId);

	SharedBackendInterface.fpDestroyResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0],
		SharedBackendEffectContextId);

	SharedBackendInterface.fpDestroyResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0],
		SharedBackendEffectContextId);
}

FfxErrorCode FFFrameInterpolator::CreateOpticalFlowContext()
{
	// Set up configuration for optical flow
	FfxOpticalflowContextDescription fsrOfDescription = {};
	fsrOfDescription.backendInterface = FrameInterpolationBackendInterface;
	fsrOfDescription.flags = 0;
	fsrOfDescription.resolution = { SwapchainWidth, SwapchainHeight };
	FFX_RETURN_ON_FAIL(ffxOpticalflowContextCreate(&OpticalFlowContext, &fsrOfDescription));

	FfxOpticalflowSharedResourceDescriptions fsrOfSharedDescriptions = {};
	FFX_RETURN_ON_FAIL(ffxOpticalflowGetSharedResourceDescriptions(&OpticalFlowContext, &fsrOfSharedDescriptions));

	FFX_RETURN_ON_FAIL(SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&fsrOfSharedDescriptions.opticalFlowVector,
		SharedBackendEffectContextId,
		&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]));

	FFX_RETURN_ON_FAIL(SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&fsrOfSharedDescriptions.opticalFlowSCD,
		SharedBackendEffectContextId,
		&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]));

	return FFX_OK;
}

void FFFrameInterpolator::DestroyOpticalFlowContext()
{
	ffxOpticalflowContextDestroy(&OpticalFlowContext);

	SharedBackendInterface.fpDestroyResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR],
		SharedBackendEffectContextId);

	SharedBackendInterface.fpDestroyResource(
		&SharedBackendInterface,
		GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT],
		SharedBackendEffectContextId);
}
