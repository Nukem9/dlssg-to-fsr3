#include <numbers>
#include <vulkan/vulkan.h>
#include "NGX/NvNGX.h"
#include "FFExt.h"
#include "FFFrameInterpolator.h"
#include "Util.h"

#ifdef _DEBUG
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_fsr3_x64d.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_backend_dx12_x64d.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_backend_vk_x64d.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_frameinterpolation_x64d.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_opticalflow_x64d.lib")
#else
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_fsr3_x64.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_backend_dx12_x64.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_backend_vk_x64.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_frameinterpolation_x64.lib")
#pragma comment(lib, "../../dependencies/ffx-sdk/ffx_opticalflow_x64.lib")
#endif

D3D12_RESOURCE_STATES ffxGetDX12StateFromResourceState(FfxResourceStates state);
VkAccessFlags getVKAccessFlagsFromResourceState(FfxResourceStates state);
VkImageLayout getVKImageLayoutFromResourceState(FfxResourceStates state);

FFFrameInterpolator::FFFrameInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight)
	: VulkanBackend(false),
	  DXLogicalDevice(Device),
	  SwapchainWidth(OutputWidth),
	  SwapchainHeight(OutputHeight)
{
	Device->AddRef();

	if (!FF_SUCCEEDED(Create()))
		throw std::runtime_error("FFFrameInterpolator: Failed to initialize.");
}

FFFrameInterpolator::FFFrameInterpolator(VkDevice LogicalDevice, VkPhysicalDevice PhysicalDevice, uint32_t OutputWidth, uint32_t OutputHeight)
	: VulkanBackend(true),
	  VKLogicalDevice(LogicalDevice),
	  VKPhysicalDevice(PhysicalDevice),
	  SwapchainWidth(OutputWidth),
	  SwapchainHeight(OutputHeight)
{
	if (!FF_SUCCEEDED(Create()))
		throw std::runtime_error("FFFrameInterpolator: Failed to initialize.");
}

FFFrameInterpolator::~FFFrameInterpolator()
{
	Destroy();

	if (!VulkanBackend)
		DXLogicalDevice->Release();
}

FfxErrorCode FFFrameInterpolator::Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters)
{
	const bool enableInterpolation = NGXParameters->GetUIntOrDefault("DLSSG.EnableInterp", 0) != 0;
	const bool isRecordingCommands = NGXParameters->GetUIntOrDefault("DLSSG.IsRecording", 0) != 0;

	auto cmdListVk = reinterpret_cast<VkCommandBuffer>(CommandList);
	auto cmdList12 = reinterpret_cast<ID3D12GraphicsCommandList *>(CommandList);

	if (VulkanBackend)
		ActiveCommandList = ffxGetCommandListVK(static_cast<VkCommandBuffer>(CommandList));
	else
		ActiveCommandList = ffxGetCommandListDX12(static_cast<ID3D12GraphicsCommandList *>(CommandList));

	//
	// Begin a new command list in the event our caller didn't set one up
	//
	if (!isRecordingCommands)
	{
		void *recordingQueue = nullptr;
		NGXParameters->GetVoidPointer("DLSSG.CmdQueue", reinterpret_cast<void **>(&recordingQueue));

		void *recordingAllocator = nullptr;
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

		if (VulkanBackend)
		{
			const VkCommandBufferBeginInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

			vkResetCommandBuffer(cmdListVk, 0);
			vkBeginCommandBuffer(cmdListVk, &info);
		}
		else
		{
			cmdList12->Reset(static_cast<ID3D12CommandAllocator *>(recordingAllocator), nullptr);
		}
	}

	//
	// Main pass setup and dispatches
	//
	FfxResource fsrGameBackBufferResource = {};
	FfxResource fsrGameRealOutputResource = {};

	const auto dispatchStatus = [&]() -> FfxErrorCode
	{
		LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &fsrGameBackBufferResource, FFX_RESOURCE_STATE_COMPUTE_READ);
		LoadResourceFromNGXParameters(NGXParameters, "DLSSG.OutputReal", &fsrGameRealOutputResource, FFX_RESOURCE_STATE_UNORDERED_ACCESS);

		if (!enableInterpolation)
			return FFX_OK;

		// As far as I know there aren't any direct ways to fetch the current gbuffer dimensions from NGX. I guess
		// pulling it from depth buffer extents is enough.
		{
			FfxResource temp = {};
			LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Depth", &temp, FFX_RESOURCE_STATE_COPY_DEST);

			RenderWidth = NGXParameters->GetUIntOrDefault("DLSSG.DepthSubrectWidth", temp.description.width);
			RenderHeight = NGXParameters->GetUIntOrDefault("DLSSG.DepthSubrectHeight", temp.description.height);
		}

		if (RenderWidth <= 32 || RenderHeight <= 32)
			return FFX_ERROR_INVALID_ARGUMENT;

		// Parameter setup
		FFDilatorDispatchParameters fsrDilationDesc = {};
		FfxOpticalflowDispatchDescription fsrOfDispatchDesc = {};
		FFInterpolatorDispatchParameters fsrFiDispatchDesc = {};

		if (!BuildDilationParameters(&fsrDilationDesc, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		if (!BuildOpticalFlowParameters(&fsrOfDispatchDesc, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		if (!BuildFrameInterpolationParameters(&fsrFiDispatchDesc, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		const static bool doDebugOverlay = Util::GetSetting(L"Debug", L"EnableDebugOverlay", false);
		const static bool doDebugTearLines = Util::GetSetting(L"Debug", L"EnableDebugTearLines", false);
		const static bool doInterpolatedOnly = Util::GetSetting(L"Debug", L"EnableInterpolatedFramesOnly", false);

		fsrFiDispatchDesc.DebugView = doDebugOverlay;
		fsrFiDispatchDesc.DebugTearLines = doDebugTearLines;

		// Record/submit commands
		auto status = DilationContext->Dispatch(fsrDilationDesc);
		FFX_RETURN_ON_FAIL(status);

		status = ffxOpticalflowContextDispatch(&OpticalFlowContext, &fsrOfDispatchDesc);
		FFX_RETURN_ON_FAIL(status);

		status = FrameInterpolatorContext->Dispatch(fsrFiDispatchDesc);
		FFX_RETURN_ON_FAIL(status);

		if (fsrFiDispatchDesc.DebugView || doInterpolatedOnly)
			fsrGameBackBufferResource = fsrFiDispatchDesc.OutputInterpolatedColorBuffer;

		return FFX_OK;
	}();

	// Command list state has to be restored before we can return an error code. Don't bother
	// adding copy commands when dispatch fails.
	if (dispatchStatus == FFX_OK && fsrGameRealOutputResource.resource && fsrGameBackBufferResource.resource)
	{
		if (VulkanBackend)
		{
			// Transition
			VkImageMemoryBarrier barriers[2] = {};
			{
				barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				barriers[0].srcAccessMask = getVKAccessFlagsFromResourceState(fsrGameRealOutputResource.state);
				barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barriers[0].oldLayout = getVKImageLayoutFromResourceState(fsrGameRealOutputResource.state);
				barriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				barriers[0].image = static_cast<VkImage>(fsrGameRealOutputResource.resource); // Destination
				barriers[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				barriers[0].subresourceRange.baseMipLevel = 0;
				barriers[0].subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
				barriers[0].subresourceRange.baseArrayLayer = 0;
				barriers[0].subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

				barriers[1] = barriers[0];
				barriers[1].srcAccessMask = getVKAccessFlagsFromResourceState(fsrGameBackBufferResource.state);
				barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
				barriers[1].oldLayout = getVKImageLayoutFromResourceState(fsrGameBackBufferResource.state);
				barriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				barriers[1].image = static_cast<VkImage>(fsrGameBackBufferResource.resource); // Source

				vkCmdPipelineBarrier(
					cmdListVk,
					VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					0,
					0,
					nullptr,
					0,
					nullptr,
					2,
					barriers);
			}

			// Copy
			{
				VkImageCopy copyRegion = {};
				copyRegion.extent = { fsrGameRealOutputResource.description.width, fsrGameRealOutputResource.description.height, 1 };

				copyRegion.dstSubresource.aspectMask = barriers[0].subresourceRange.aspectMask;
				copyRegion.dstSubresource.mipLevel = barriers[0].subresourceRange.baseMipLevel;
				copyRegion.dstSubresource.baseArrayLayer = barriers[0].subresourceRange.baseArrayLayer;
				copyRegion.dstSubresource.layerCount = barriers[0].subresourceRange.layerCount;

				copyRegion.srcSubresource.aspectMask = barriers[1].subresourceRange.aspectMask;
				copyRegion.srcSubresource.mipLevel = barriers[1].subresourceRange.baseMipLevel;
				copyRegion.srcSubresource.baseArrayLayer = barriers[1].subresourceRange.baseArrayLayer;
				copyRegion.srcSubresource.layerCount = barriers[1].subresourceRange.layerCount;

				vkCmdCopyImage(
					cmdListVk,
					barriers[1].image,
					barriers[1].newLayout,
					barriers[0].image,
					barriers[0].newLayout,
					1,
					&copyRegion);
			}

			// Transition
			{
				std::swap(barriers[0].srcAccessMask, barriers[0].dstAccessMask);
				std::swap(barriers[0].oldLayout, barriers[0].newLayout);
				std::swap(barriers[1].srcAccessMask, barriers[1].dstAccessMask);
				std::swap(barriers[1].oldLayout, barriers[1].newLayout);

				vkCmdPipelineBarrier(
					cmdListVk,
					VK_PIPELINE_STAGE_TRANSFER_BIT,
					VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
					0,
					0,
					nullptr,
					0,
					nullptr,
					2,
					barriers);
			}
		}
		else
		{
			D3D12_RESOURCE_BARRIER barriers[2] = {};
			barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barriers[0].Transition.pResource = static_cast<ID3D12Resource *>(fsrGameRealOutputResource.resource); // Destination
			barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barriers[0].Transition.StateBefore = ffxGetDX12StateFromResourceState(fsrGameRealOutputResource.state);
			barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

			barriers[1] = barriers[0];
			barriers[1].Transition.pResource = static_cast<ID3D12Resource *>(fsrGameBackBufferResource.resource); // Source
			barriers[1].Transition.StateBefore = ffxGetDX12StateFromResourceState(fsrGameBackBufferResource.state);
			barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

			cmdList12->ResourceBarrier(2, barriers);
			cmdList12->CopyResource(barriers[0].Transition.pResource, barriers[1].Transition.pResource);
			std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
			std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
			cmdList12->ResourceBarrier(2, barriers);
		}
	}

	//
	// Finish what we started. Restore the command list to its previous state when necessary.
	//
	if (!isRecordingCommands)
	{
		if (VulkanBackend)
			vkEndCommandBuffer(cmdListVk);
		else
			cmdList12->Close();
	}

	NGXParameters->Set4("DLSSG.FlushRequired", 0);
	return dispatchStatus;
}

bool FFFrameInterpolator::BuildDilationParameters(FFDilatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.CommandList = ActiveCommandList;

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
	NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.commandList = ActiveCommandList;

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
	NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.CommandList = ActiveCommandList;

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

FfxErrorCode FFFrameInterpolator::Create()
{
	FFX_RETURN_ON_FAIL(CreateBackend());
	FFX_RETURN_ON_FAIL(CreateDilationContext());
	FFX_RETURN_ON_FAIL(CreateOpticalFlowContext());
	FrameInterpolatorContext = std::make_unique<FFInterpolator>(FrameInterpolationBackendInterface, SwapchainWidth, SwapchainHeight);

	return FFX_OK;
}

void FFFrameInterpolator::Destroy()
{
	FrameInterpolatorContext.reset();
	DestroyOpticalFlowContext();
	DestroyDilationContext();
	DestroyBackend();
}

FfxErrorCode FFFrameInterpolator::CreateBackend()
{
	const uint32_t maxContexts = 3; // Assume 3 contexts per interface

	if (VulkanBackend)
	{
		VkDeviceContext vkContext =
		{
			.vkDevice = VKLogicalDevice,
			.vkPhysicalDevice = VKPhysicalDevice,
			.vkDeviceProcAddr = nullptr,
		};

		const auto fsrDevice = ffxGetDeviceVK(&vkContext);
		const auto scratchSize = ffxGetScratchMemorySizeVK(vkContext.vkPhysicalDevice, maxContexts);

		auto& buffer1 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceVK(&SharedBackendInterface, fsrDevice, buffer1.get(), scratchSize, maxContexts));

		auto& buffer2 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceVK(&FrameInterpolationBackendInterface, fsrDevice, buffer2.get(), scratchSize, maxContexts));
	}
	else
	{
		const auto fsrDevice = ffxGetDeviceDX12(DXLogicalDevice);
		const auto scratchSize = ffxGetScratchMemorySizeDX12(maxContexts);

		auto& buffer1 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceDX12(&SharedBackendInterface, fsrDevice, buffer1.get(), scratchSize, maxContexts));

		auto& buffer2 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceDX12(&FrameInterpolationBackendInterface, fsrDevice, buffer2.get(), scratchSize, maxContexts));

	}

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

bool FFFrameInterpolator::LoadResourceFromNGXParameters(
	NGXInstanceParameters *NGXParameters,
	const char *Name,
	FfxResource *OutFfxResource,
	FfxResourceStates State)
{
	// FSR blatantly ignores the FfxResource size fields. I'm not even going to try using extents.
	void *resource = nullptr;
	NGXParameters->GetVoidPointer(Name, reinterpret_cast<void **>(&resource));

	if (!resource)
	{
		*OutFfxResource = {};
		return false;
	}

	if (VulkanBackend)
	{
		// Vulkan provides no mechanism to query resource information. Convert it manually.
		auto resourceHandle = static_cast<const NGXVulkanResourceHandle *>(resource);

		if (resourceHandle->Type != 0)
			__debugbreak();

		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = resourceHandle->ImageMetadata.Format;
		imageInfo.extent = { resourceHandle->ImageMetadata.Width, resourceHandle->ImageMetadata.Height, 1 };
		imageInfo.mipLevels = resourceHandle->ImageMetadata.Subresource.levelCount;
		imageInfo.arrayLayers = resourceHandle->ImageMetadata.Subresource.layerCount;
		imageInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

		*OutFfxResource = ffxGetResourceVK(resourceHandle->ImageMetadata.Image, GetFfxResourceDescriptionVK(&imageInfo), nullptr, State);
	}
	else
	{
		// D3D12 provides information via GetDesc()
		*OutFfxResource = ffxGetResourceDX12(
			static_cast<ID3D12Resource *>(resource),
			GetFfxResourceDescriptionDX12(static_cast<ID3D12Resource *>(resource)),
			nullptr,
			State);
	}

	return true;
}
