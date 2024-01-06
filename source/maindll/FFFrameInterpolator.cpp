#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <numbers>
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
	: DXLogicalDevice(Device),
	  SwapchainWidth(OutputWidth),
	  SwapchainHeight(OutputHeight)
{
	DXLogicalDevice->AddRef();
	Create();
}

FFFrameInterpolator::FFFrameInterpolator(
	VkDevice LogicalDevice,
	VkPhysicalDevice PhysicalDevice,
	uint32_t OutputWidth,
	uint32_t OutputHeight)
	: VKLogicalDevice(LogicalDevice),
	  VKPhysicalDevice(PhysicalDevice),
	  SwapchainWidth(OutputWidth),
	  SwapchainHeight(OutputHeight)
{
	Create();
}

FFFrameInterpolator::~FFFrameInterpolator()
{
	Destroy();

	if (DXLogicalDevice)
		DXLogicalDevice->Release();
}

FfxErrorCode FFFrameInterpolator::Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters)
{
	const bool enableInterpolation = NGXParameters->GetUIntOrDefault("DLSSG.EnableInterp", 0) != 0;
	const bool isRecordingCommands = NGXParameters->GetUIntOrDefault("DLSSG.IsRecording", 0) != 0;

	const auto cmdListVk = reinterpret_cast<VkCommandBuffer>(CommandList);
	const auto cmdList12 = reinterpret_cast<ID3D12GraphicsCommandList *>(CommandList);

	if (IsVulkanBackend())
		m_ActiveCommandList = ffxGetCommandListVK(cmdListVk);
	else
		m_ActiveCommandList = ffxGetCommandListDX12(cmdList12);

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

		if (IsVulkanBackend())
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

		if (!CalculateResourceDimensions(NGXParameters))
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

		status = ffxOpticalflowContextDispatch(&OpticalFlowContext.value(), &fsrOfDispatchDesc);
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
		if (IsVulkanBackend())
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
		if (IsVulkanBackend())
			vkEndCommandBuffer(cmdListVk);
		else
			cmdList12->Close();
	}

	NGXParameters->Set4("DLSSG.FlushRequired", 0);
	return dispatchStatus;
}

bool FFFrameInterpolator::IsVulkanBackend() const
{
	return DXLogicalDevice == nullptr;
}

bool FFFrameInterpolator::CalculateResourceDimensions(NGXInstanceParameters *NGXParameters)
{
	// NGX doesn't provide a direct method to query current gbuffer dimensions so we'll grab them
	// from the depth buffer instead. Depth is suitable because it's the one resource guaranteed to
	// be the same size as the gbuffer. Hopefully.
	FfxResource temp = {};
	{
		auto width = NGXParameters->GetUIntOrDefault("DLSSG.DepthSubrectWidth", 0);
		auto height = NGXParameters->GetUIntOrDefault("DLSSG.DepthSubrectHeight", 0);

		if (width == 0 || height == 0)
		{
			LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Depth", &temp, FFX_RESOURCE_STATE_COPY_DEST);
			width = temp.description.width;
			height = temp.description.height;
		}

		m_PreUpscaleRenderWidth = width;
		m_PreUpscaleRenderHeight = height;
	}

	if (m_PreUpscaleRenderWidth <= 32 || m_PreUpscaleRenderHeight <= 32)
		return false;

	// HUD-less dimensions are the "ground truth" final render resolution. These aren't necessarily
	// equal to back buffer dimensions. Letterboxing in The Witcher 3 is a good test case.
#if 0
	if (LoadResourceFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &temp, FFX_RESOURCE_STATE_COPY_DEST))
	{
		auto width = NGXParameters->GetUIntOrDefault("DLSSG.HUDLessSubrectWidth", 0);
		auto height = NGXParameters->GetUIntOrDefault("DLSSG.HUDLessSubrectHeight", 0);

		if (width == 0 || height == 0)
		{
			width = temp.description.width;
			height = temp.description.height;
		}

		m_PostUpscaleRenderWidth = width;
		m_PostUpscaleRenderHeight = height;
	}
	else
#endif
	{
		// No HUD-less resource. Default to back buffer resolution.
		m_PostUpscaleRenderWidth = SwapchainWidth;
		m_PostUpscaleRenderHeight = SwapchainHeight;
	}

	if (m_PostUpscaleRenderWidth <= 32 || m_PostUpscaleRenderHeight <= 32)
		return false;

	return true;
}

bool FFFrameInterpolator::BuildDilationParameters(FFDilatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.CommandList = m_ActiveCommandList;

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Depth", &desc.InputDepth, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.MVecs", &desc.InputMotionVectors, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	desc.OutputDilatedDepth = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedDilatedDepth);
	desc.OutputDilatedMotionVectors = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedDilatedMotionVectors);
	desc.OutputReconstructedPrevNearestDepth = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedPreviousNearestDepth);

	desc.RenderSize = { m_PreUpscaleRenderWidth, m_PreUpscaleRenderHeight };
	desc.OutputSize = { m_PostUpscaleRenderWidth, m_PostUpscaleRenderHeight };

	desc.HDR = NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) != 0;
	desc.DepthInverted = NGXParameters->GetUIntOrDefault("DLSSG.DepthInverted", 0) != 0;

	const FfxDimensions2D mvecExtents = {
		NGXParameters->GetUIntOrDefault("DLSSG.MVecsSubrectWidth", desc.InputMotionVectors.description.width),
		NGXParameters->GetUIntOrDefault("DLSSG.MVecsSubrectHeight", desc.InputMotionVectors.description.height),
	};

	desc.MotionVectorScale = {
		NGXParameters->GetFloatOrDefault("DLSSG.MvecScaleX", 1.0f),
		NGXParameters->GetFloatOrDefault("DLSSG.MvecScaleY", 1.0f),
	};

	desc.MotionVectorJitterOffsets = {
		NGXParameters->GetFloatOrDefault("DLSSG.JitterOffsetX", 0),
		NGXParameters->GetFloatOrDefault("DLSSG.JitterOffsetY", 0),
	};

	desc.MotionVectorJitterCancellation = NGXParameters->GetUIntOrDefault("DLSSG.MVecJittered", 0) != 0;
	desc.MotionVectorsFullResolution = m_PostUpscaleRenderWidth == mvecExtents.width && m_PostUpscaleRenderHeight == mvecExtents.height;

	return true;
}

bool FFFrameInterpolator::BuildOpticalFlowParameters(
	FfxOpticalflowDispatchDescription *OutParameters,
	NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.commandList = m_ActiveCommandList;

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &desc.color, FFX_RESOURCE_STATE_COPY_DEST) &&
		!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &desc.color, FFX_RESOURCE_STATE_COMPUTE_READ))
		return false;

	desc.color.description.width = m_PostUpscaleRenderWidth; // Explicit override
	desc.color.description.height = m_PostUpscaleRenderHeight;

	desc.opticalFlowVector = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedOpticalFlowVector);
	desc.opticalFlowSCD = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedOpticalFlowSCD);

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
	desc.CommandList = m_ActiveCommandList;

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

	desc.InputDilatedDepth = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedDilatedDepth);
	desc.InputDilatedMotionVectors = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedDilatedMotionVectors);
	desc.InputReconstructedPreviousNearDepth = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedPreviousNearestDepth);
	desc.InputOpticalFlowVector = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedOpticalFlowVector);
	desc.InputOpticalFlowSceneChangeDetection = SharedBackendInterface.fpGetResource(&SharedBackendInterface, *m_TexSharedOpticalFlowSCD);

	desc.RenderSize = { m_PreUpscaleRenderWidth, m_PreUpscaleRenderHeight };
	desc.OutputSize = { SwapchainWidth, SwapchainHeight };

	desc.OpticalFlowBufferSize = { desc.InputOpticalFlowVector.description.width, desc.InputOpticalFlowVector.description.height };
	desc.OpticalFlowBlockSize = 8;
	desc.OpticalFlowScale = { 1.0f / m_PostUpscaleRenderWidth, 1.0f / m_PostUpscaleRenderHeight };

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

void FFFrameInterpolator::Create()
{
	if (CreateBackend() != FFX_OK)
		throw std::runtime_error(__FUNCTION__ ": Failed to create backend context.");

	if (CreateDilationContext() != FFX_OK)
		throw std::runtime_error(__FUNCTION__ ": Failed to create dilation context.");

	if (CreateOpticalFlowContext() != FFX_OK)
		throw std::runtime_error(__FUNCTION__ ": Failed to create optical flow context.");

	FrameInterpolatorContext = std::make_unique<FFInterpolator>(FrameInterpolationBackendInterface, SwapchainWidth, SwapchainHeight);
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
	if (IsVulkanBackend())
	{
		VkDeviceContext vkContext = {
			.vkDevice = VKLogicalDevice,
			.vkPhysicalDevice = VKPhysicalDevice,
			.vkDeviceProcAddr = nullptr,
		};

		const uint32_t maxContexts = 6; // One interface, six contexts
		const auto fsrDevice = ffxGetDeviceVK(&vkContext);
		const auto scratchSize = ffxGetScratchMemorySizeVK(vkContext.vkPhysicalDevice, maxContexts);

		auto& buffer1 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceVK(&SharedBackendInterface, fsrDevice, buffer1.get(), scratchSize, maxContexts));

		FrameInterpolationBackendInterface = SharedBackendInterface;
	}
	else
	{
		const uint32_t maxContexts = 3; // Assume 3 contexts per interface
		const auto fsrDevice = ffxGetDeviceDX12(DXLogicalDevice);
		const auto scratchSize = ffxGetScratchMemorySizeDX12(maxContexts);

		auto& buffer1 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceDX12(&SharedBackendInterface, fsrDevice, buffer1.get(), scratchSize, maxContexts));

		auto& buffer2 = ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceDX12(&FrameInterpolationBackendInterface, fsrDevice, buffer2.get(), scratchSize, maxContexts));
	}

	const auto status = SharedBackendInterface.fpCreateBackendContext(&SharedBackendInterface, &m_SharedEffectContextId.emplace());

	if (status != FFX_OK)
	{
		m_SharedEffectContextId.reset();
		return status;
	}

	return FFX_OK;
}

void FFFrameInterpolator::DestroyBackend()
{
	if (m_SharedEffectContextId)
		SharedBackendInterface.fpDestroyBackendContext(&SharedBackendInterface, *m_SharedEffectContextId);
}

FfxErrorCode FFFrameInterpolator::CreateDilationContext()
{
	DilationContext = std::make_unique<FFDilator>(SharedBackendInterface, SwapchainWidth, SwapchainHeight);
	const auto resourceDescs = DilationContext->GetSharedResourceDescriptions();

	auto status = SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&resourceDescs.dilatedDepth,
		*m_SharedEffectContextId,
		&m_TexSharedDilatedDepth.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedDilatedDepth.reset();
		return status;
	}

	status = SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&resourceDescs.dilatedMotionVectors,
		*m_SharedEffectContextId,
		&m_TexSharedDilatedMotionVectors.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedDilatedMotionVectors.reset();
		return status;
	}

	status = SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&resourceDescs.reconstructedPrevNearestDepth,
		*m_SharedEffectContextId,
		&m_TexSharedPreviousNearestDepth.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedPreviousNearestDepth.reset();
		return status;
	}

	return FFX_OK;
}

void FFFrameInterpolator::DestroyDilationContext()
{
	DilationContext.reset();

	if (m_TexSharedDilatedDepth)
		SharedBackendInterface.fpDestroyResource(&SharedBackendInterface, *m_TexSharedDilatedDepth, *m_SharedEffectContextId);

	if (m_TexSharedDilatedMotionVectors)
		SharedBackendInterface.fpDestroyResource(&SharedBackendInterface, *m_TexSharedDilatedMotionVectors, *m_SharedEffectContextId);

	if (m_TexSharedPreviousNearestDepth)
		SharedBackendInterface.fpDestroyResource(&SharedBackendInterface, *m_TexSharedPreviousNearestDepth, *m_SharedEffectContextId);
}

FfxErrorCode FFFrameInterpolator::CreateOpticalFlowContext()
{
	// Set up configuration for optical flow
	FfxOpticalflowContextDescription fsrOfDescription = {};
	fsrOfDescription.backendInterface = FrameInterpolationBackendInterface;
	fsrOfDescription.flags = 0;
	fsrOfDescription.resolution = { SwapchainWidth, SwapchainHeight };
	auto status = ffxOpticalflowContextCreate(&OpticalFlowContext.emplace(), &fsrOfDescription);

	if (status != FFX_OK)
	{
		OpticalFlowContext.reset();
		return status;
	}

	FfxOpticalflowSharedResourceDescriptions fsrOfSharedDescriptions = {};
	FFX_RETURN_ON_FAIL(ffxOpticalflowGetSharedResourceDescriptions(&OpticalFlowContext.value(), &fsrOfSharedDescriptions));

	status = SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&fsrOfSharedDescriptions.opticalFlowVector,
		*m_SharedEffectContextId,
		&m_TexSharedOpticalFlowVector.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedOpticalFlowVector.reset();
		return status;
	}

	status = SharedBackendInterface.fpCreateResource(
		&SharedBackendInterface,
		&fsrOfSharedDescriptions.opticalFlowSCD,
		*m_SharedEffectContextId,
		&m_TexSharedOpticalFlowSCD.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedOpticalFlowSCD.reset();
		return status;
	}

	return FFX_OK;
}

void FFFrameInterpolator::DestroyOpticalFlowContext()
{
	if (OpticalFlowContext)
		ffxOpticalflowContextDestroy(&OpticalFlowContext.value());

	if (m_TexSharedOpticalFlowVector)
		SharedBackendInterface.fpDestroyResource(&SharedBackendInterface, *m_TexSharedOpticalFlowVector, *m_SharedEffectContextId);

	if (m_TexSharedOpticalFlowSCD)
		SharedBackendInterface.fpDestroyResource(&SharedBackendInterface, *m_TexSharedOpticalFlowSCD, *m_SharedEffectContextId);
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

	if (IsVulkanBackend())
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
