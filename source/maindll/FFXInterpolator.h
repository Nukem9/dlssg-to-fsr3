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

#define FFX_CHECK(x) do { if ((x) != FFX_OK) { __debugbreak(); } } while (0)

class FFXInterpolator
{
private:
	uint32_t SwapchainWidth = 0;
	uint32_t SwapchainHeight = 0;

	FfxInterface FrameInterpolationBackendInterface = {};
	FfxInterface SharedBackendInterface = {};
	FfxUInt32 SharedBackendEffectContextId = 0;

	FfxOpticalflowContext OpticalFlowContext = {};
	FfxFrameInterpolationContext FrameInterpolationContext = {};

	FfxResourceInternal GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_COUNT] = {};

	std::vector<std::unique_ptr<uint8_t[]>> ScratchMemoryBuffers;

public:
	FFXInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight, uint32_t BackbufferFormat)
	{
		SwapchainWidth = OutputWidth;
		SwapchainHeight = OutputHeight;

		CreateBackend(Device);
		CreateOpticalFlowContext();
		CreateFrameInterpolationContext(BackbufferFormat);
	}

	~FFXInterpolator()
	{
		ffxFrameInterpolationContextDestroy(&FrameInterpolationContext);
		DestroyOpticalFlowContext();
		DestroyBackend();
	}

private:
	void CreateBackend(ID3D12Device *Device)
	{
		const auto fsrDevice = ffxGetDeviceDX12(Device);
		const auto scratchBufferSize = ffxGetScratchMemorySizeDX12(2); // Assume 2 contexts per interface

		auto& buffer1 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchBufferSize));
		FFX_CHECK(ffxGetInterfaceDX12(&SharedBackendInterface, fsrDevice, buffer1.get(), scratchBufferSize, 2));

		auto& buffer2 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchBufferSize));
		FFX_CHECK(ffxGetInterfaceDX12(&FrameInterpolationBackendInterface, fsrDevice, buffer2.get(), scratchBufferSize, 2));

		FFX_CHECK(SharedBackendInterface.fpCreateBackendContext(&SharedBackendInterface, &SharedBackendEffectContextId));
	}

	void DestroyBackend()
	{
		SharedBackendInterface.fpDestroyBackendContext(
			&SharedBackendInterface,
			SharedBackendEffectContextId);
	}

	void CreateOpticalFlowContext()
	{
		// Set up configuration for optical flow
		FfxOpticalflowContextDescription fsrOfDescription = {};
		fsrOfDescription.backendInterface = FrameInterpolationBackendInterface;
		fsrOfDescription.resolution = { SwapchainWidth, SwapchainHeight };
		fsrOfDescription.flags |= FFX_OPTICALFLOW_ENABLE_TEXTURE1D_USAGE;
		FFX_CHECK(ffxOpticalflowContextCreate(&OpticalFlowContext, &fsrOfDescription));

		FfxOpticalflowSharedResourceDescriptions fsrOfSharedDescriptions = {};
		FFX_CHECK(ffxOpticalflowGetSharedResourceDescriptions(&OpticalFlowContext, &fsrOfSharedDescriptions));

		FFX_CHECK(SharedBackendInterface.fpCreateResource(
			&SharedBackendInterface,
			&fsrOfSharedDescriptions.opticalFlowVector,
			SharedBackendEffectContextId,
			&GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]));

		FFX_CHECK(SharedBackendInterface.fpCreateResource(
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

	void CreateFrameInterpolationContext(uint32_t BackbufferFormat)
	{
		// Then set up configuration for frame interpolation
		FfxFrameInterpolationContextDescription fsrFiDescription = {};
		fsrFiDescription.backendInterface = FrameInterpolationBackendInterface;
		fsrFiDescription.flags |= FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INVERTED;
		// fsrFiDescription.flags |= FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INFINITE;
		fsrFiDescription.flags |= FFX_FRAMEINTERPOLATION_ENABLE_TEXTURE1D_USAGE;
		// fsrFiDescription.flags |= FFX_FRAMEINTERPOLATION_ENABLE_HDR_COLOR_INPUT;

		fsrFiDescription.maxRenderSize = { SwapchainWidth, SwapchainHeight }; // Make these equivalent for now
		fsrFiDescription.displaySize = { SwapchainWidth, SwapchainHeight };
		fsrFiDescription.backBufferFormat = ffxGetSurfaceFormatDX12(static_cast<DXGI_FORMAT>(BackbufferFormat));
		FFX_CHECK(ffxFrameInterpolationContextCreate(&FrameInterpolationContext, &fsrFiDescription));
	}

	bool LoadResourceFromNGXParameters(NGXInstanceParameters *Parameters, const char *Name, FfxResource *OutFfxResource)
	{
		ID3D12Resource *resource = nullptr;
		Parameters->GetVoidPointer(Name, reinterpret_cast<void **>(&resource));

		if (resource)
		{
			*OutFfxResource = ffxGetResourceDX12(
				resource,
				GetFfxResourceDescriptionDX12(resource),
				nullptr,
				FFX_RESOURCE_STATE_UNORDERED_ACCESS);

			return true;
		}

		*OutFfxResource = {};
		return false;
	}

public:
	void Evaluate(ID3D12GraphicsCommandList *CommandList, NGXInstanceParameters *Parameters)
	{
		// Frame interpolation
		FfxFrameInterpolationDispatchDescription fsrFiDispatchDesc = {};
		{
			LoadResourceFromNGXParameters(Parameters, "DLSSG.Backbuffer", &fsrFiDispatchDesc.currentBackBuffer);
			LoadResourceFromNGXParameters(Parameters, "DLSSG.MVecs", &fsrFiDispatchDesc.dilatedMotionVectors);
			LoadResourceFromNGXParameters(Parameters, "DLSSG.Depth", &fsrFiDispatchDesc.dilatedDepth);
			LoadResourceFromNGXParameters(Parameters, "DLSSG.HUDLess", &fsrFiDispatchDesc.currentBackBuffer_HUDLess);
			LoadResourceFromNGXParameters(Parameters, "DLSSG.OutputInterpolated", &fsrFiDispatchDesc.output);

			fsrFiDispatchDesc.currentBackBuffer.description.format = fsrFiDispatchDesc.currentBackBuffer_HUDLess.description.format;

			fsrFiDispatchDesc.commandList = ffxGetCommandListDX12(CommandList);
			fsrFiDispatchDesc.displaySize = { SwapchainWidth, SwapchainHeight };

			fsrFiDispatchDesc.renderSize.width = fsrFiDispatchDesc.dilatedDepth.description.width;
			fsrFiDispatchDesc.renderSize.height = fsrFiDispatchDesc.dilatedDepth.description.height;

			fsrFiDispatchDesc.opticalFlowVector = SharedBackendInterface.fpGetResource(
				&SharedBackendInterface,
				GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_VECTOR]);

			fsrFiDispatchDesc.opticalFlowSceneChangeDetection = SharedBackendInterface.fpGetResource(
				&SharedBackendInterface,
				GPUResources[FFX_FSR3_RESOURCE_IDENTIFIER_OPTICAL_FLOW_SCD_OUTPUT]);

			fsrFiDispatchDesc.opticalFlowBlockSize = 8;
			fsrFiDispatchDesc.opticalFlowScale = { 1.f / fsrFiDispatchDesc.displaySize.width, 1.f / fsrFiDispatchDesc.displaySize.height };

			fsrFiDispatchDesc.frameTimeDelta = 1000.0f / 60.0f; // TODO: upscaleDesc->frameTimeDelta;
			fsrFiDispatchDesc.reset = Parameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;

			fsrFiDispatchDesc.cameraNear = Parameters->GetFloatOrDefault("DLSSG.CameraNear", 0);
			fsrFiDispatchDesc.cameraFar = Parameters->GetFloatOrDefault("DLSSG.CameraFar", 0);
			fsrFiDispatchDesc.viewSpaceToMetersFactor = 0.01f; // TODO: upscaleDesc->viewSpaceToMetersFactor;
			fsrFiDispatchDesc.cameraFovAngleVertical = Parameters->GetFloatOrDefault("DLSSG.CameraFOV", 0);

			/*
			fsrFiDispatchDesc.reconstructPrevNearDepth = contextPrivate->SharedBackendInterface.fpGetResource(
				&contextPrivate->SharedBackendInterface,
				contextPrivate->GPUResources
					[FFX_FSR3_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH_0 +
					 (contextPrivate->FeatureEvaluationCount * FFX_FSR3_RESOURCE_IDENTIFIER_UPSCALED_COUNT)]);
			*/

			fsrFiDispatchDesc.interpolationRect.left = 0;
			fsrFiDispatchDesc.interpolationRect.top = 0;
			fsrFiDispatchDesc.interpolationRect.width = fsrFiDispatchDesc.displaySize.width;
			fsrFiDispatchDesc.interpolationRect.height = fsrFiDispatchDesc.displaySize.height;

			const auto colorBuffersHdr = Parameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0);
			fsrFiDispatchDesc.backBufferTransferFunction = colorBuffersHdr ? FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ
																		   : FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;

			fsrFiDispatchDesc.minMaxLuminance[0] = 0.0f; // callbackDesc->minMaxLuminance[0];
			fsrFiDispatchDesc.minMaxLuminance[1] = 1.0f; // callbackDesc->minMaxLuminance[1];

			// fsrFiDispatchDesc.flags |= FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_TEAR_LINES;
			fsrFiDispatchDesc.flags |= FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_VIEW;
		}

		// Optical flow
		FfxOpticalflowDispatchDescription fsrOfDispatchDesc = {};
		{
			fsrOfDispatchDesc.commandList = fsrFiDispatchDesc.commandList;

			if (fsrFiDispatchDesc.currentBackBuffer_HUDLess.resource)
				fsrOfDispatchDesc.color = fsrFiDispatchDesc.currentBackBuffer_HUDLess;
			else
				fsrOfDispatchDesc.color = fsrFiDispatchDesc.currentBackBuffer;

			fsrOfDispatchDesc.reset = fsrFiDispatchDesc.reset;
			fsrOfDispatchDesc.backbufferTransferFunction = fsrFiDispatchDesc.backBufferTransferFunction;
			fsrOfDispatchDesc.minMaxLuminance.x = fsrFiDispatchDesc.minMaxLuminance[0];
			fsrOfDispatchDesc.minMaxLuminance.y = fsrFiDispatchDesc.minMaxLuminance[1];

			fsrOfDispatchDesc.opticalFlowVector = fsrFiDispatchDesc.opticalFlowVector;
			fsrOfDispatchDesc.opticalFlowSCD = fsrFiDispatchDesc.opticalFlowSceneChangeDetection;
		}

		// Submit command list commands
		FFX_CHECK(ffxOpticalflowContextDispatch(&OpticalFlowContext, &fsrOfDispatchDesc));
		FFX_CHECK(ffxFrameInterpolationDispatch(&FrameInterpolationContext, &fsrFiDispatchDesc));

		if (fsrFiDispatchDesc.flags & FFX_FRAMEINTERPOLATION_DISPATCH_DRAW_DEBUG_VIEW)
			fsrFiDispatchDesc.currentBackBuffer.resource = fsrFiDispatchDesc.output.resource;

		ID3D12Resource *outputRealResource = nullptr;
		Parameters->GetVoidPointer("DLSSG.OutputReal", reinterpret_cast<void **>(&outputRealResource));
		ID3D12CommandQueue *queue = nullptr;
		Parameters->GetVoidPointer("DLSSG.CmdQueue", reinterpret_cast<void **>(&queue));
		ID3D12CommandAllocator *allocator = nullptr;
		Parameters->GetVoidPointer("DLSSG.CmdAlloc", reinterpret_cast<void **>(&allocator));

		D3D12_RESOURCE_BARRIER barriers[2] = {};
		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[0].Transition.pResource = outputRealResource;
		barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

		barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[1].Transition.pResource = static_cast<ID3D12Resource *>(fsrFiDispatchDesc.currentBackBuffer.resource);
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
		CommandList->Reset(allocator, nullptr);
	}
};
