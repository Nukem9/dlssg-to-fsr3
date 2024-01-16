#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <numbers>
#include "NGX/NvNGX.h"
#include "FFExt.h"
#include "FFFrameInterpolator.h"
#include "Util.h"

D3D12_RESOURCE_STATES ffxGetDX12StateFromResourceState(FfxResourceStates state);
VkAccessFlags getVKAccessFlagsFromResourceState(FfxResourceStates state);
VkImageLayout getVKImageLayoutFromResourceState(FfxResourceStates state);

FFFrameInterpolator::FFFrameInterpolator(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight)
	: m_LogicalDeviceDX(Device),
	  m_SwapchainWidth(OutputWidth),
	  m_SwapchainHeight(OutputHeight)
{
	m_LogicalDeviceDX->AddRef();
	Create();
}

FFFrameInterpolator::FFFrameInterpolator(
	VkDevice LogicalDevice,
	VkPhysicalDevice PhysicalDevice,
	uint32_t OutputWidth,
	uint32_t OutputHeight)
	: m_LogicalDeviceVK(LogicalDevice),
	  m_PhysicalDeviceVK(PhysicalDevice),
	  m_SwapchainWidth(OutputWidth),
	  m_SwapchainHeight(OutputHeight)
{
	Create();
}

FFFrameInterpolator::~FFFrameInterpolator()
{
	Destroy();

	if (m_LogicalDeviceDX)
		m_LogicalDeviceDX->Release();
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

		QueryHDRLuminanceRange(NGXParameters);

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
		auto status = m_DilationContext->Dispatch(fsrDilationDesc);
		FFX_RETURN_ON_FAIL(status);

		status = ffxOpticalflowContextDispatch(&m_OpticalFlowContext.value(), &fsrOfDispatchDesc);
		FFX_RETURN_ON_FAIL(status);

		status = m_FrameInterpolatorContext->Dispatch(fsrFiDispatchDesc);
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
	return m_LogicalDeviceDX == nullptr;
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
		m_PostUpscaleRenderWidth = m_SwapchainWidth;
		m_PostUpscaleRenderHeight = m_SwapchainHeight;
	}

	if (m_PostUpscaleRenderWidth <= 32 || m_PostUpscaleRenderHeight <= 32)
		return false;

	// I've no better place to put this. Dying Light 2 is beyond screwed up.
	//
	// 1. They take a D24 depth buffer and explicitly convert it to RGBA8. The GBA channels are unused. This
	//    is passed to Streamline as the "depth" buffer.
	//
	// 2. They take the same RGBA8 "depth" buffer above and pass it to Streamline as the "HUD-less" color
	//    buffer.
	//
	// How's it possible to be this incompetent? Have these people used a debugger? The debug visualizer? For
	// all I know the native DLSS-G implementation doesn't even work. It could just be duplicating frames.
	const static auto isDyingLight2 = GetModuleHandleW(L"DyingLightGame_x64_rwdi.exe") != nullptr;

	if (isDyingLight2)
	{
		NGXParameters->SetVoidPointer("DLSSG.HUDLess", nullptr);
		m_PostUpscaleRenderWidth = m_SwapchainWidth;
		m_PostUpscaleRenderHeight = m_SwapchainHeight;
	}

	return true;
}

void FFFrameInterpolator::QueryHDRLuminanceRange(NGXInstanceParameters *NGXParameters)
{
	if (NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) == 0)
		return;

	if (m_HDRLuminanceRangeSet)
		return;

	// Microsoft DirectX 12 HDR sample
	// https://github.com/microsoft/DirectX-Graphics-Samples/blob/b5f92e2251ee83db4d4c795b3cba5d470c52eaf8/Samples/Desktop/D3D12HDR/src/D3D12HDR.cpp#L1064
	const auto currentAdapterLuid = m_LogicalDeviceDX->GetAdapterLuid();

	IDXGIFactory1 *factory = nullptr;
	IDXGIAdapter1 *adapter = nullptr;
	IDXGIOutput *output = nullptr;

	if (CreateDXGIFactory1(IID_PPV_ARGS(&factory)) == S_OK)
	{
		// Match the active DXGI adapter
		for (uint32_t i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++)
		{
			DXGI_ADAPTER_DESC desc = {};
			adapter->GetDesc(&desc);

			if (desc.AdapterLuid.LowPart == currentAdapterLuid.LowPart && desc.AdapterLuid.HighPart == currentAdapterLuid.HighPart)
			{
				// Then check the first HDR-capable output
				for (uint32_t j = 0; adapter->EnumOutputs(j, &output) != DXGI_ERROR_NOT_FOUND; j++)
				{
					if (IDXGIOutput6 *output6; output->QueryInterface(IID_PPV_ARGS(&output6)) == S_OK)
					{
						DXGI_OUTPUT_DESC1 outputDesc = {};
						output6->GetDesc1(&outputDesc);
						output6->Release();

						if (outputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 && !m_HDRLuminanceRangeSet)
						{
							m_HDRLuminanceRange = { outputDesc.MinLuminance, outputDesc.MaxLuminance };
							m_HDRLuminanceRangeSet = true;
						}
					}

					output->Release();
				}
			}

			adapter->Release();
		}

		factory->Release();
	}

	// Keep using hardcoded defaults even if we didn't find a valid output
	m_HDRLuminanceRangeSet = true;

	spdlog::info("Using assumed HDR luminance range: {} to {} nits", m_HDRLuminanceRange.x, m_HDRLuminanceRange.y);
}

bool FFFrameInterpolator::BuildDilationParameters(FFDilatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.CommandList = m_ActiveCommandList;

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.Depth", &desc.InputDepth, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	if (!LoadResourceFromNGXParameters(NGXParameters, "DLSSG.MVecs", &desc.InputMotionVectors, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	desc.OutputDilatedDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedDilatedDepth);
	desc.OutputDilatedMotionVectors = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedDilatedMotionVectors);
	desc.OutputReconstructedPrevNearestDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedPreviousNearestDepth);

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

	desc.opticalFlowVector = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowVector);
	desc.opticalFlowSCD = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowSCD);

	desc.reset = NGXParameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;

	if (NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) == 0)
		desc.backbufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
	else
		desc.backbufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ;

	desc.minMaxLuminance = m_HDRLuminanceRange;

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

	desc.InputDilatedDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedDilatedDepth);
	desc.InputDilatedMotionVectors = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedDilatedMotionVectors);
	desc.InputReconstructedPreviousNearDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedPreviousNearestDepth);
	desc.InputOpticalFlowVector = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowVector);
	desc.InputOpticalFlowSceneChangeDetection = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowSCD);

	desc.RenderSize = { m_PreUpscaleRenderWidth, m_PreUpscaleRenderHeight };
	desc.OutputSize = { m_SwapchainWidth, m_SwapchainHeight };

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
	desc.MinMaxLuminance = m_HDRLuminanceRange;

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

	m_FrameInterpolatorContext = std::make_unique<FFInterpolator>(m_FrameInterpolationBackendInterface, m_SwapchainWidth, m_SwapchainHeight);
}

void FFFrameInterpolator::Destroy()
{
	m_FrameInterpolatorContext.reset();
	DestroyOpticalFlowContext();
	DestroyDilationContext();
	DestroyBackend();
}

FfxErrorCode FFFrameInterpolator::CreateBackend()
{
	if (IsVulkanBackend())
	{
		VkDeviceContext vkContext = {
			.vkDevice = m_LogicalDeviceVK,
			.vkPhysicalDevice = m_PhysicalDeviceVK,
			.vkDeviceProcAddr = nullptr,
		};

		const uint32_t maxContexts = 6; // One interface, six contexts
		const auto fsrDevice = ffxGetDeviceVK(&vkContext);
		const auto scratchSize = ffxGetScratchMemorySizeVK(vkContext.vkPhysicalDevice, maxContexts);

		auto& buffer1 = m_ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceVK(&m_SharedBackendInterface, fsrDevice, buffer1.get(), scratchSize, maxContexts));

		m_FrameInterpolationBackendInterface = m_SharedBackendInterface;
	}
	else
	{
		const uint32_t maxContexts = 3; // Assume 3 contexts per interface
		const auto fsrDevice = ffxGetDeviceDX12(m_LogicalDeviceDX);
		const auto scratchSize = ffxGetScratchMemorySizeDX12(maxContexts);

		auto& buffer1 = m_ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceDX12(&m_SharedBackendInterface, fsrDevice, buffer1.get(), scratchSize, maxContexts));

		auto& buffer2 = m_ScratchMemoryBuffers.emplace_back(std::make_unique<uint8_t[]>(scratchSize));
		FFX_RETURN_ON_FAIL(ffxGetInterfaceDX12(&m_FrameInterpolationBackendInterface, fsrDevice, buffer2.get(), scratchSize, maxContexts));
	}

	const auto status = m_SharedBackendInterface.fpCreateBackendContext(&m_SharedBackendInterface, &m_SharedEffectContextId.emplace());

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
		m_SharedBackendInterface.fpDestroyBackendContext(&m_SharedBackendInterface, *m_SharedEffectContextId);
}

FfxErrorCode FFFrameInterpolator::CreateDilationContext()
{
	m_DilationContext = std::make_unique<FFDilator>(m_SharedBackendInterface, m_SwapchainWidth, m_SwapchainHeight);
	const auto resourceDescs = m_DilationContext->GetSharedResourceDescriptions();

	auto status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&resourceDescs.dilatedDepth,
		*m_SharedEffectContextId,
		&m_TexSharedDilatedDepth.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedDilatedDepth.reset();
		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&resourceDescs.dilatedMotionVectors,
		*m_SharedEffectContextId,
		&m_TexSharedDilatedMotionVectors.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedDilatedMotionVectors.reset();
		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
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
	m_DilationContext.reset();

	if (m_TexSharedDilatedDepth)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedDilatedDepth, *m_SharedEffectContextId);

	if (m_TexSharedDilatedMotionVectors)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedDilatedMotionVectors, *m_SharedEffectContextId);

	if (m_TexSharedPreviousNearestDepth)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedPreviousNearestDepth, *m_SharedEffectContextId);
}

FfxErrorCode FFFrameInterpolator::CreateOpticalFlowContext()
{
	// Set up configuration for optical flow
	FfxOpticalflowContextDescription fsrOfDescription = {};
	fsrOfDescription.backendInterface = m_FrameInterpolationBackendInterface;
	fsrOfDescription.flags = 0;
	fsrOfDescription.resolution = { m_SwapchainWidth, m_SwapchainHeight };
	auto status = ffxOpticalflowContextCreate(&m_OpticalFlowContext.emplace(), &fsrOfDescription);

	if (status != FFX_OK)
	{
		m_OpticalFlowContext.reset();
		return status;
	}

	FfxOpticalflowSharedResourceDescriptions fsrOfSharedDescriptions = {};
	FFX_RETURN_ON_FAIL(ffxOpticalflowGetSharedResourceDescriptions(&m_OpticalFlowContext.value(), &fsrOfSharedDescriptions));

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&fsrOfSharedDescriptions.opticalFlowVector,
		*m_SharedEffectContextId,
		&m_TexSharedOpticalFlowVector.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedOpticalFlowVector.reset();
		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
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
	if (m_OpticalFlowContext)
		ffxOpticalflowContextDestroy(&m_OpticalFlowContext.value());

	if (m_TexSharedOpticalFlowVector)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowVector, *m_SharedEffectContextId);

	if (m_TexSharedOpticalFlowSCD)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowSCD, *m_SharedEffectContextId);
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
