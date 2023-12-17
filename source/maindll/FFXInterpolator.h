#pragma once

#include <FidelityFX/host/ffx_frameinterpolation.h>
#include <FidelityFX/host/ffx_opticalflow.h>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include <FidelityFX/gpu/fsr3/ffx_fsr3_resources.h>
#include "nvngx.h"

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

#include "FFXCommon.h"
#include "FFXDilation.h"

class FFXInterpolator
{
private:
	const uint32_t SwapchainWidth;	// Final image presented to the screen
	const uint32_t SwapchainHeight;

	uint32_t RenderWidth = 0;		// GBuffer dimensions
	uint32_t RenderHeight = 0;

	ID3D12Fence *SwapChainInUseFence = nullptr;
	uint64_t SwapChainInUseCounter = 0;

	FfxInterface FrameInterpolationBackendInterface = {};
	FfxInterface SharedBackendInterface = {};
	FfxUInt32 SharedBackendEffectContextId = 0;

	std::unique_ptr<FFXDilation> DilationContext;
	FfxOpticalflowContext OpticalFlowContext = {};
	FfxFrameInterpolationContext FrameInterpolationContext = {};

	FfxResourceInternal GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_COUNT] = {};
	std::vector<std::unique_ptr<uint8_t[]>> ScratchMemoryBuffers;

public:
	FFXInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight, uint32_t BackbufferFormat)
		: SwapchainWidth(OutputWidth),
		  SwapchainHeight(OutputHeight)
	{
		if (FAILED(Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&SwapChainInUseFence))))
			throw std::runtime_error("Failed to create swap chain in-use fence");

		CreateBackend(Device);
		CreateDilationContext();
		CreateOpticalFlowContext();
		CreateFrameInterpolationContext(BackbufferFormat);
	}

	~FFXInterpolator()
	{
		SwapChainInUseFence->Release();

		ffxFrameInterpolationContextDestroy(&FrameInterpolationContext);
		DestroyOpticalFlowContext();
		DestroyDilationContext();
		DestroyBackend();
	}

	bool AnyResourcesInUse() const
	{
		return SwapChainInUseFence->GetCompletedValue() < SwapChainInUseCounter;
	}

public:
	void Dispatch(ID3D12GraphicsCommandList *CommandList, NGXInstanceParameters *Parameters)
	{
		// As far as I know there aren't any direct ways to fetch the current gbuffer dimensions from NGX. I guess
		// pulling it from the depth buffer is good enough.
		{
			ID3D12Resource *depthResource = nullptr;
			Parameters->GetVoidPointer("DLSSG.Depth", reinterpret_cast<void **>(&depthResource));
			auto desc = depthResource->GetDesc();

			RenderWidth = static_cast<uint32_t>(desc.Width);
			RenderHeight = desc.Height;
		}

		ID3D12Resource *outputRealResource = nullptr;
		Parameters->GetVoidPointer("DLSSG.OutputReal", reinterpret_cast<void **>(&outputRealResource));
		ID3D12CommandQueue *queue = nullptr;
		Parameters->GetVoidPointer("DLSSG.CmdQueue", reinterpret_cast<void **>(&queue));
		ID3D12CommandAllocator *allocator = nullptr;
		Parameters->GetVoidPointer("DLSSG.CmdAlloc", reinterpret_cast<void **>(&allocator));

		// Depth and MV dilation
		FFXDilationDispatchParameters fsrDilationDesc = {};
		BuildDilationParameters(&fsrDilationDesc, CommandList, Parameters);

		// Optical flow
		FfxOpticalflowDispatchDescription fsrOfDispatchDesc = {};
		BuildOpticalFlowParameters(&fsrOfDispatchDesc, CommandList, Parameters);

		// Frame interpolation
		FfxFrameInterpolationDispatchDescription fsrFiDispatchDesc = {};
		BuildFrameInterpolationParameters(&fsrFiDispatchDesc, CommandList, Parameters);

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
		setObjectDebugName(static_cast<ID3D12Resource *>(fsrFiDispatchDesc.currentBackBuffer_HUDLess.resource), "DLSSG.HUDLess");
		setObjectDebugName(static_cast<ID3D12Resource *>(fsrFiDispatchDesc.currentBackBuffer.resource), "DLSSG.Backbuffer");

		setObjectDebugName(outputRealResource, "DLSSG.OutputReal");
		setObjectDebugName(static_cast<ID3D12Resource *>(fsrFiDispatchDesc.output.resource), "DLSSG.OutputInterpolated");

		fsrFiDispatchDesc.flags |= FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_VIEW;
#endif

		uint32_t status = FFX_OK;
		status |= DilationContext->Dispatch(fsrDilationDesc);
		status |= ffxOpticalflowContextDispatch(&OpticalFlowContext, &fsrOfDispatchDesc);
		status |= ffxFrameInterpolationDispatch(&FrameInterpolationContext, &fsrFiDispatchDesc);
		FFX_THROW_ON_FAIL(status);

		if (fsrFiDispatchDesc.flags & FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_VIEW)
			fsrFiDispatchDesc.currentBackBuffer.resource = fsrFiDispatchDesc.output.resource;

		D3D12_RESOURCE_BARRIER barriers[2] = {};
		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[0].Transition.pResource = outputRealResource; // Destination
		barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[1].Transition.pResource = static_cast<ID3D12Resource *>(fsrFiDispatchDesc.currentBackBuffer.resource); // Source
		barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

		CommandList->ResourceBarrier(2, barriers);
		CommandList->CopyResource(barriers[0].Transition.pResource, barriers[1].Transition.pResource);
		std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
		std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
		CommandList->ResourceBarrier(2, barriers);
		CommandList->Close();

		queue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList **>(&CommandList));
		queue->Signal(SwapChainInUseFence, ++SwapChainInUseCounter);

		CommandList->Reset(allocator, nullptr);
	}

private:
	void BuildDilationParameters(
		FFXDilationDispatchParameters *OutParameters,
		ID3D12GraphicsCommandList *CommandList,
		NGXInstanceParameters *NGXParameters)
	{
		auto& desc = *OutParameters;
		desc.CommandList = ffxGetCommandListDX12(CommandList);

		LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Depth", &desc.InputDepth, FFX_RESOURCE_STATE_COMMON);
		LoadResourceFromNGXParameters(NGXParameters, "DLSSG.MVecs", &desc.InputMotionVectors);

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
		desc.MotionVectorsFullResolution = NGXParameters->GetUIntOrDefault("DLSSG.MvecDilated", 0) != 0; // ?
		desc.MotionVectorJitterCancellation = NGXParameters->GetUIntOrDefault("DLSSG.MVecJittered", 0) != 0;

		// clang-format off
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
		// clang-format on
	}

	void BuildOpticalFlowParameters(
		FfxOpticalflowDispatchDescription *OutParameters,
		ID3D12GraphicsCommandList *CommandList,
		NGXInstanceParameters *NGXParameters)
	{
		auto& desc = *OutParameters;
		desc.commandList = ffxGetCommandListDX12(CommandList);

		if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &desc.color))
			LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &desc.color, FFX_RESOURCE_STATE_COMPUTE_READ);

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

		desc.minMaxLuminance.x = 0.00001f; // TODO: callbackDesc->minMaxLuminance[0];
		desc.minMaxLuminance.y = 1000.0f;  // TODO: callbackDesc->minMaxLuminance[1];
	}

	void BuildFrameInterpolationParameters(
		FfxFrameInterpolationDispatchDescription *OutParameters,
		ID3D12GraphicsCommandList *CommandList,
		NGXInstanceParameters *NGXParameters)
	{
		auto& desc = *OutParameters;
		desc.commandList = ffxGetCommandListDX12(CommandList);

		LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &desc.currentBackBuffer, FFX_RESOURCE_STATE_COMPUTE_READ);
		LoadResourceFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &desc.currentBackBuffer_HUDLess);
		LoadResourceFromNGXParameters(NGXParameters, "DLSSG.OutputInterpolated", &desc.output);

		desc.dilatedDepth = SharedBackendInterface.fpGetResource(
			&SharedBackendInterface,
			GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0]);

		desc.dilatedMotionVectors = SharedBackendInterface.fpGetResource(
			&SharedBackendInterface,
			GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0]);

		desc.reconstructPrevNearDepth = SharedBackendInterface.fpGetResource(
			&SharedBackendInterface,
			GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0]);

		desc.currentBackBuffer_HUDLess.description.format = desc.currentBackBuffer.description.format; // Workaround

		desc.displaySize = { SwapchainWidth, SwapchainHeight };
		desc.renderSize = { RenderWidth, RenderHeight };

		desc.opticalFlowVector = SharedBackendInterface.fpGetResource(
			&SharedBackendInterface,
			GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]);

		desc.opticalFlowSceneChangeDetection = SharedBackendInterface.fpGetResource(
			&SharedBackendInterface,
			GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]);

		desc.opticalFlowBlockSize = 8;
		desc.opticalFlowScale = { 1.0f / desc.displaySize.width, 1.0f / desc.displaySize.height };

		desc.frameTimeDelta = 1000.0f / 60.0f; // TODO: This is wrong. It's also 100% unused.
		desc.reset = NGXParameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;

		desc.cameraNear = NGXParameters->GetFloatOrDefault("DLSSG.CameraNear", 0);
		desc.cameraFar = NGXParameters->GetFloatOrDefault("DLSSG.CameraFar", 0);
		desc.viewSpaceToMetersFactor = 0.0f; // TODO: upscaleDesc->viewSpaceToMetersFactor;
		desc.cameraFovAngleVertical = NGXParameters->GetFloatOrDefault("DLSSG.CameraFOV", 0);

		desc.interpolationRect.left = 0;
		desc.interpolationRect.top = 0;
		desc.interpolationRect.width = desc.displaySize.width;
		desc.interpolationRect.height = desc.displaySize.height;

		// DLSS-G doesn't support scRGB. That leaves two choices.
		if (NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) == 0)
			desc.backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
		else
			desc.backBufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ;

		desc.minMaxLuminance[0] = 0.00001f; // TODO: callbackDesc->minMaxLuminance[0];
		desc.minMaxLuminance[1] = 1000.0f;	// TODO: callbackDesc->minMaxLuminance[1];
	}

	void CreateBackend(ID3D12Device *Device)
	{
		const auto fsrDevice = ffxGetDeviceDX12(Device);
		const auto scratchBufferSize = ffxGetScratchMemorySizeDX12(3); // Assume 3 contexts per interface

		auto& buffer1 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchBufferSize));
		FFX_THROW_ON_FAIL(ffxGetInterfaceDX12(&SharedBackendInterface, fsrDevice, buffer1.get(), scratchBufferSize, 3));

		auto& buffer2 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchBufferSize));
		FFX_THROW_ON_FAIL(ffxGetInterfaceDX12(&FrameInterpolationBackendInterface, fsrDevice, buffer2.get(), scratchBufferSize, 3));

		FFX_THROW_ON_FAIL(SharedBackendInterface.fpCreateBackendContext(&SharedBackendInterface, &SharedBackendEffectContextId));
	}

	void DestroyBackend()
	{
		SharedBackendInterface.fpDestroyBackendContext(&SharedBackendInterface, SharedBackendEffectContextId);
	}

	void CreateDilationContext()
	{
		DilationContext = std::make_unique<FFXDilation>(SharedBackendInterface, SwapchainWidth, SwapchainHeight);
		const auto resourceDescs = DilationContext->GetSharedResourceDescriptions();

		FFX_THROW_ON_FAIL(SharedBackendInterface.fpCreateResource(
			&SharedBackendInterface,
			&resourceDescs.dilatedDepth,
			SharedBackendEffectContextId,
			&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_DEPTH_0]));

		FFX_THROW_ON_FAIL(SharedBackendInterface.fpCreateResource(
			&SharedBackendInterface,
			&resourceDescs.dilatedMotionVectors,
			SharedBackendEffectContextId,
			&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS_0]));

		FFX_THROW_ON_FAIL(SharedBackendInterface.fpCreateResource(
			&SharedBackendInterface,
			&resourceDescs.reconstructedPrevNearestDepth,
			SharedBackendEffectContextId,
			&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0]));
	}

	void DestroyDilationContext()
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

	void CreateOpticalFlowContext()
	{
		// Set up configuration for optical flow
		FfxOpticalflowContextDescription fsrOfDescription = {};
		fsrOfDescription.backendInterface = FrameInterpolationBackendInterface;
		fsrOfDescription.resolution = { SwapchainWidth, SwapchainHeight };
		FFX_THROW_ON_FAIL(ffxOpticalflowContextCreate(&OpticalFlowContext, &fsrOfDescription));

		FfxOpticalflowSharedResourceDescriptions fsrOfSharedDescriptions = {};
		FFX_THROW_ON_FAIL(ffxOpticalflowGetSharedResourceDescriptions(&OpticalFlowContext, &fsrOfSharedDescriptions));

		FFX_THROW_ON_FAIL(SharedBackendInterface.fpCreateResource(
			&SharedBackendInterface,
			&fsrOfSharedDescriptions.opticalFlowVector,
			SharedBackendEffectContextId,
			&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]));

		FFX_THROW_ON_FAIL(SharedBackendInterface.fpCreateResource(
			&SharedBackendInterface,
			&fsrOfSharedDescriptions.opticalFlowSCD,
			SharedBackendEffectContextId,
			&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]));
	}

	void DestroyOpticalFlowContext()
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

	void CreateFrameInterpolationContext(uint32_t BackBufferFormat)
	{
		// Then set up configuration for frame interpolation
		FfxFrameInterpolationContextDescription fsrFiDescription = {};
		fsrFiDescription.backendInterface = FrameInterpolationBackendInterface;
		fsrFiDescription.flags |= FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INVERTED;
		// fsrFiDescription.flags |= FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INFINITE;
		// fsrFiDescription.flags |= FFX_FRAMEINTERPOLATION_ENABLE_TEXTURE1D_USAGE; DON'T USE THIS FUCKING FLAG
		// fsrFiDescription.flags |= FFX_FRAMEINTERPOLATION_ENABLE_HDR_COLOR_INPUT;

		fsrFiDescription.maxRenderSize = { SwapchainWidth, SwapchainHeight };
		fsrFiDescription.displaySize = fsrFiDescription.maxRenderSize;
		fsrFiDescription.backBufferFormat = ffxGetSurfaceFormatDX12(static_cast<DXGI_FORMAT>(BackBufferFormat));
		FFX_THROW_ON_FAIL(ffxFrameInterpolationContextCreate(&FrameInterpolationContext, &fsrFiDescription));
	}

	static bool LoadResourceFromNGXParameters(
		NGXInstanceParameters *Parameters,
		const char *Name,
		FfxResource *OutFfxResource,
		FfxResourceStates State = FFX_RESOURCE_STATE_UNORDERED_ACCESS)
	{
		// FSR blatantly ignores the FfxResource size fields. I'm not even going to try.
		ID3D12Resource *resource = nullptr;
		Parameters->GetVoidPointer(Name, reinterpret_cast<void **>(&resource));

		if (resource)
		{
			*OutFfxResource = ffxGetResourceDX12(resource, GetFfxResourceDescriptionDX12(resource), nullptr, State);
			return true;
		}

		*OutFfxResource = {};
		return false;
	}
};
