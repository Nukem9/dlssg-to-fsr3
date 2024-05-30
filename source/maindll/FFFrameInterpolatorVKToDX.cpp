#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <d3d12.h>
#include <dxgi1_3.h>
#include <DXProgrammableCapture.h>
#include "FFFrameInterpolatorVKToDX.h"

DXGI_FORMAT ffxGetDX12FormatFromSurfaceFormat(FfxSurfaceFormat surfaceFormat);
VkAccessFlags getVKAccessFlagsFromResourceState(FfxResourceStates state);
VkImageLayout getVKImageLayoutFromResourceState(FfxResourceStates state);

void FFFrameInterpolatorVKToDX::CachedSharedImageData::Reset(VkDevice DeviceVK)
{
	if (m_ResourceVK != VK_NULL_HANDLE)
		vkDestroyImage(DeviceVK, m_ResourceVK, nullptr);

	if (m_MemoryVK != VK_NULL_HANDLE)
		vkFreeMemory(DeviceVK, m_MemoryVK, nullptr);

	if (m_ResourceDX)
		m_ResourceDX->Release();

	*this = {};
}

FFFrameInterpolatorVKToDX::FFFrameInterpolatorVKToDX(
	VkDevice LogicalDevice,
	VkPhysicalDevice PhysicalDevice,
	uint32_t OutputWidth,
	uint32_t OutputHeight,
	NGXInstanceParameters *NGXParameters)
	: m_DeviceVK(LogicalDevice),
	  m_PhysicalDeviceVK(PhysicalDevice)
{
	// Query the Vulkan device LUID property for an equivalent DXGI adapter interface
	IDXGIAdapter1 *adapter = nullptr;
	uint32_t nodeMask = 0;

	if (!FindEquivalentDXGIAdapter(PhysicalDevice, adapter, nodeMask))
		throw std::runtime_error("Failed to find corresponding DXGI adapter for Vulkan device");

	// Then create both Vulkan and D3D12 resources. D3D12 creates the shared handles. Vulkan imports them.
	InitializeVulkanBackend(PhysicalDevice);
	InitializeD3D12Backend(adapter, nodeMask);
	adapter->Release();

	// D3D12 frame interpolator
	m_FrameInterpolator.emplace(m_DeviceDX, OutputWidth, OutputHeight, nullptr);

	// NGX-specific state
	NGXParameters->GetVoidPointer("DLSSG.CreateTimelineSyncObjectsCallback", reinterpret_cast<void **>(&m_AppCreateTimelineSyncObjects));
	NGXParameters->GetVoidPointer("DLSSG.SyncSignalCallback", reinterpret_cast<void **>(&m_AppSyncSignal));
	NGXParameters->GetVoidPointer("DLSSG.SyncWaitCallback", reinterpret_cast<void **>(&m_AppSyncWait));
	NGXParameters->GetVoidPointer("DLSSG.SyncFlushCallback", reinterpret_cast<void **>(&m_AppSyncFlush));

	NGXParameters->GetVoidPointer("DLSSG.CreateTimelineSyncObjectsCallbackData", &m_AppCreateTimelineSyncObjectsData);
	NGXParameters->GetVoidPointer("DLSSG.SyncSignalCallbackData", &m_AppSyncSignalData);
	NGXParameters->GetVoidPointer("DLSSG.SyncWaitCallbackData", &m_AppSyncWaitData);
	NGXParameters->GetVoidPointer("DLSSG.SyncFlushCallbackData", &m_AppSyncFlushData);

	if (!m_AppSyncSignal || !m_AppSyncWait)
		throw std::runtime_error("DLSSG synchronization callbacks are missing");

	if (m_AppCreateTimelineSyncObjects)
		m_AppCreateTimelineSyncObjects(
			m_AppCreateTimelineSyncObjectsData,
			reinterpret_cast<void **>(&m_SharedSemaphoreS1VK),
			m_SharedSemaphoreS1Counter,
			reinterpret_cast<void **>(&m_SharedSemaphoreS4VK),
			m_SharedSemaphoreS4Counter);

	if (!CreateOrImportSharedSemaphore(m_SharedSemaphoreS1Counter, m_SharedSemaphoreS1VK, m_SharedSemaphoreS1DX) ||
		!CreateOrImportSharedSemaphore(m_SharedSemaphoreS4Counter, m_SharedSemaphoreS4VK, m_SharedSemaphoreS4DX))
		throw std::runtime_error("Failed to create shared fences");
}

FFFrameInterpolatorVKToDX::~FFFrameInterpolatorVKToDX()
{
	m_CachedMVecsImage.Reset(m_DeviceVK);
	m_CachedDepthImage.Reset(m_DeviceVK);
	m_CachedBackbufferImage.Reset(m_DeviceVK);
	m_CachedOutputImage.Reset(m_DeviceVK);

	if (m_CommandListDX)
		m_CommandListDX->Release();

	for (auto& allocator : m_CommandAllocatorsDX)
	{
		if (allocator)
			allocator->Release();
	}

	if (m_CommandQueueDX)
		m_CommandQueueDX->Release();

	if (m_SharedSemaphoreS1DX)
		m_SharedSemaphoreS1DX->Release();

	if (m_SharedSemaphoreS4DX)
		m_SharedSemaphoreS4DX->Release();

	if (!m_AppCreateTimelineSyncObjects)
	{
		if (m_SharedSemaphoreS1VK != VK_NULL_HANDLE)
			vkDestroySemaphore(m_DeviceVK, m_SharedSemaphoreS1VK, nullptr);

		if (m_SharedSemaphoreS4VK != VK_NULL_HANDLE)
			vkDestroySemaphore(m_DeviceVK, m_SharedSemaphoreS4VK, nullptr);
	}

	if (m_DeviceDX)
		m_DeviceDX->Release();

	if (m_GraphicsAnalysisInterface)
		m_GraphicsAnalysisInterface->Release();
}

FfxErrorCode FFFrameInterpolatorVKToDX::Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters)
{
	const bool isRecordingCommands = NGXParameters->GetUIntOrDefault("DLSSG.IsRecording", 0) != 0;

	NGXParameters->Set4("DLSSG.FlushRequired", 0);

	if (m_ResourceFlushRequested)
	{
		m_ResourceFlushRequested = false;
		m_CachedMVecsImage.Reset(m_DeviceVK);
		m_CachedDepthImage.Reset(m_DeviceVK);
		m_CachedBackbufferImage.Reset(m_DeviceVK);
		m_CachedOutputImage.Reset(m_DeviceVK);
	}

	auto setupCachedSharedResource = [&](const char *Name, CachedSharedImageData& CachedData, VkImage& CachedImage)
	{
		if (m_ResourceFlushRequested)
			return FFX_OK;

		VkImageCreateInfo newCreateInfo = {};
		if (!LoadVulkanResourceNGXInfo(NGXParameters, Name, newCreateInfo, CachedImage))
			return FFX_ERROR_INVALID_ARGUMENT;

		if ((memcmp(&newCreateInfo.extent, &CachedData.m_CreateInfo.extent, sizeof(VkExtent3D)) != 0) ||
			newCreateInfo.format != CachedData.m_CreateInfo.format ||
			newCreateInfo.mipLevels != CachedData.m_CreateInfo.mipLevels ||
			newCreateInfo.arrayLayers != CachedData.m_CreateInfo.arrayLayers)
		{
			// Resolution probably changed; request a vkWaitForIdle and clear shared resources on the next evaluate call
			if (CachedData.m_ResourceDX)
			{
				m_ResourceFlushRequested = true;
				return FFX_OK;
			}

			// The image didn't exist previously; just create it
			CachedData.m_CreateInfo = newCreateInfo;

			if (!CreateSharedTexture(CachedData.m_CreateInfo, CachedData.m_ResourceVK, CachedData.m_MemoryVK, CachedData.m_ResourceDX))
				return FFX_ERROR_OUT_OF_MEMORY;
		}

		NGXParameters->SetVoidPointer(Name, CachedData.m_ResourceDX);
		return FFX_OK;
	};

	auto copyResource = [&](VkCommandBuffer CommandList,
							VkImage InputImage,
							FfxResourceStates InputResourceState,
							CachedSharedImageData& OutputCachedImageData,
							FfxResourceStates OutputResourceState,
							bool ReverseCopyOrder = false)
	{
		if (ReverseCopyOrder)
		{
			CopyVulkanTexture(
				CommandList,
				OutputCachedImageData.m_ResourceVK,
				InputImage,
				OutputResourceState,
				InputResourceState,
				OutputCachedImageData.m_CreateInfo.extent,
				false);
		}
		else
		{
			CopyVulkanTexture(
				CommandList,
				InputImage,
				OutputCachedImageData.m_ResourceVK,
				InputResourceState,
				OutputResourceState,
				OutputCachedImageData.m_CreateInfo.extent,
				false);
		}
	};

	// Acquire/create interop shared resources and swap Vulkan NGX parameters with D3D12 equivalents
	FfxErrorCode result = FFX_OK;
	VkImage inMVecs = {};
	VkImage inDepth = {};
	VkImage inBackbuffer = {};
	VkImage outInterp = {};

	if ((result = setupCachedSharedResource("DLSSG.MVecs", m_CachedMVecsImage, inMVecs)) != FFX_OK)
		return result;

	if ((result = setupCachedSharedResource("DLSSG.Depth", m_CachedDepthImage, inDepth)) != FFX_OK)
		return result;

	if ((result = setupCachedSharedResource("DLSSG.Backbuffer", m_CachedBackbufferImage, inBackbuffer)) != FFX_OK)
		return result;

	if ((result = setupCachedSharedResource("DLSSG.OutputInterpolated", m_CachedOutputImage, outInterp)) != FFX_OK)
		return result;

	if (m_ResourceFlushRequested)
	{
		NGXParameters->Set4("DLSSG.FlushRequired", 1);
		return FFX_OK;
	}

	// Capture Vulkan-side inputs
	{
		// CL1: m_currentCommandList = cmdList = m_dlfgEvalCommandLists
		auto cmdList = reinterpret_cast<VkCommandBuffer>(CommandList);

		copyResource(cmdList, inMVecs, FFX_RESOURCE_STATE_COMPUTE_READ, m_CachedMVecsImage, FFX_RESOURCE_STATE_COPY_DEST);
		copyResource(cmdList, inDepth, FFX_RESOURCE_STATE_COMPUTE_READ, m_CachedDepthImage, FFX_RESOURCE_STATE_COPY_DEST);
		copyResource(cmdList, inBackbuffer, FFX_RESOURCE_STATE_COMPUTE_READ, m_CachedBackbufferImage, FFX_RESOURCE_STATE_COMPUTE_READ);

		// CL1 -> CL2: m_currentCommandList = cmdList = m_dlfgInternalAsyncOFACommandLists
		m_SharedSemaphoreS1Counter++;
		m_AppSyncSignal(m_AppSyncSignalData, &CommandList, m_SharedSemaphoreS1VK, m_SharedSemaphoreS1Counter);
	}

	// Submit D3D12 commands
	{
		const bool triggerPixCapture = m_GraphicsAnalysisInterface && (GetAsyncKeyState(VK_F11) & 1);

		if (triggerPixCapture)
			m_GraphicsAnalysisInterface->BeginCapture();

		m_CommandQueueDX->Wait(m_SharedSemaphoreS1DX, m_SharedSemaphoreS1Counter);

		const auto allocator = m_CommandAllocatorsDX[m_NextCommandAllocatorIndexDX++ % m_CommandAllocatorsDX.size()];
		allocator->Reset();

		NGXParameters->SetVoidPointer("DLSSG.CmdQueue", m_CommandQueueDX);
		NGXParameters->SetVoidPointer("DLSSG.CmdAlloc", allocator);
		NGXParameters->Set4("DLSSG.IsRecording", 0);
		result = m_FrameInterpolator->Dispatch(m_CommandListDX, NGXParameters);

		if (result != FFX_OK)
			return result;

		m_CommandQueueDX->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList *const *>(&m_CommandListDX));
		m_SharedSemaphoreS4Counter++;
		m_CommandQueueDX->Signal(m_SharedSemaphoreS4DX, m_SharedSemaphoreS4Counter);

		if (triggerPixCapture)
			m_GraphicsAnalysisInterface->EndCapture();
	}

	// Then back to the Vulkan side again
	{
		// CL2 -> CL3: m_currentCommandList = cmdList = m_dlfgInternalPostOFACommandLists
		m_AppSyncWait(m_AppSyncWaitData, &CommandList, m_SharedSemaphoreS4VK, m_SharedSemaphoreS4Counter, 0, nullptr, 0);

		copyResource(
			reinterpret_cast<VkCommandBuffer>(CommandList),
			outInterp,
			FFX_RESOURCE_STATE_UNORDERED_ACCESS,
			m_CachedOutputImage,
			FFX_RESOURCE_STATE_UNORDERED_ACCESS,
			true);
	}

	NGXParameters->Set4("DLSSG.IsRecording", isRecordingCommands);
	return FFX_OK;
}

void FFFrameInterpolatorVKToDX::InitializeVulkanBackend(VkPhysicalDevice PhysicalDevice)
{
	m_vkGetMemoryWin32HandlePropertiesKHR = reinterpret_cast<PFN_vkGetMemoryWin32HandlePropertiesKHR>(
		vkGetDeviceProcAddr(m_DeviceVK, "vkGetMemoryWin32HandlePropertiesKHR"));
	m_vkImportSemaphoreWin32HandleKHR = reinterpret_cast<PFN_vkImportSemaphoreWin32HandleKHR>(
		vkGetDeviceProcAddr(m_DeviceVK, "vkImportSemaphoreWin32HandleKHR"));

	if (!m_vkGetMemoryWin32HandlePropertiesKHR || !m_vkImportSemaphoreWin32HandleKHR)
		throw std::runtime_error("Vulkan handle import extensions are unavailable");

	const VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = 0,
	};

	const VkPhysicalDeviceExternalSemaphoreInfo externalSemaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO,
		.pNext = &semaphoreTypeCreateInfo,
		.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
	};

	// We only need to know if semaphores can be imported
	VkExternalSemaphoreProperties externalSemaphoreProperties = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES,
	};

	vkGetPhysicalDeviceExternalSemaphoreProperties(PhysicalDevice, &externalSemaphoreInfo, &externalSemaphoreProperties);

	if ((externalSemaphoreProperties.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) == 0 ||
		(externalSemaphoreProperties.exportFromImportedHandleTypes & externalSemaphoreInfo.handleType) == 0)
		throw std::runtime_error("Vulkan instance doesn't support importing timeline semaphores");
}

void FFFrameInterpolatorVKToDX::InitializeD3D12Backend(IDXGIAdapter1 *Adapter, uint32_t NodeMask)
{
#if 0 // PIX debugging
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&m_GraphicsAnalysisInterface));
#endif

	auto hr = D3D12CreateDevice(Adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_DeviceDX));

	if (FAILED(hr))
		throw std::runtime_error("Failed to create D3D12 device");

	const D3D12_COMMAND_QUEUE_DESC queueDesc = {
		.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE,
		.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH, // TODO: Unsure. Need to check Streamline.
		.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
		.NodeMask = NodeMask,
	};

	hr = m_DeviceDX->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueueDX));

	if (FAILED(hr))
		throw std::runtime_error("Failed to create D3D12 command queue");

	// One queued frame is probably enough. Four is for safety.
	m_CommandAllocatorsDX.resize(4);

	for (auto& allocator : m_CommandAllocatorsDX)
	{
		hr = m_DeviceDX->CreateCommandAllocator(queueDesc.Type, IID_PPV_ARGS(&allocator));

		if (FAILED(hr))
			throw std::runtime_error("Failed to create D3D12 command allocator");
	}

	hr = m_DeviceDX->CreateCommandList(NodeMask, queueDesc.Type, m_CommandAllocatorsDX[0], nullptr, IID_PPV_ARGS(&m_CommandListDX));

	if (FAILED(hr))
		throw std::runtime_error("Failed to create D3D12 command list");

	m_CommandListDX->Close();
}

bool FFFrameInterpolatorVKToDX::CreateOrImportSharedSemaphore(uint64_t InitialValue, VkSemaphore& VulkanSemaphore, ID3D12Fence*& D3D12Semaphore)
{
	ID3D12Fence *createdSemaphoreDX = nullptr;
	auto hr = m_DeviceDX->CreateFence(InitialValue, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&createdSemaphoreDX));

	if (FAILED(hr))
		return false;

	HANDLE win32Handle = nullptr;
	hr = m_DeviceDX->CreateSharedHandle(createdSemaphoreDX, nullptr, GENERIC_ALL, nullptr, &win32Handle);

	if (FAILED(hr))
	{
		createdSemaphoreDX->Release();
		return false;
	}

	VkSemaphore createdSemaphoreVK = VulkanSemaphore;

	if (createdSemaphoreVK == VK_NULL_HANDLE)
	{
		const VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = InitialValue,
		};

		const VkSemaphoreCreateInfo semaphoreCreateInfo = {
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &semaphoreTypeCreateInfo,
		};

		if (vkCreateSemaphore(m_DeviceVK, &semaphoreCreateInfo, nullptr, &createdSemaphoreVK) != VK_SUCCESS)
		{
			CloseHandle(win32Handle);
			createdSemaphoreDX->Release();
			return false;
		}
	}

	const VkImportSemaphoreWin32HandleInfoKHR importSemaphoreInfo = {
		.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR,
		.semaphore = createdSemaphoreVK,
		.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT,
		.handle = win32Handle,
	};

	if (m_vkImportSemaphoreWin32HandleKHR(m_DeviceVK, &importSemaphoreInfo) != VK_SUCCESS)
	{
		if (createdSemaphoreVK != VulkanSemaphore)
			vkDestroySemaphore(m_DeviceVK, createdSemaphoreVK, nullptr);

		CloseHandle(win32Handle);
		createdSemaphoreDX->Release();
		return false;
	}

	VulkanSemaphore = createdSemaphoreVK;
	D3D12Semaphore = createdSemaphoreDX;

	CloseHandle(win32Handle);
	return true;
}

bool FFFrameInterpolatorVKToDX::CreateSharedTexture(
	const VkImageCreateInfo& ImageInfo,
	VkImage& VulkanResource,
	VkDeviceMemory& VulkanMemory,
	ID3D12Resource *& D3D12Resource)
{
	const D3D12_HEAP_PROPERTIES d3d12HeapProperties = {
		.Type = D3D12_HEAP_TYPE_DEFAULT,
		.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
		.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	};

	const D3D12_RESOURCE_DESC d3d12ResourceDesc = {
		.Dimension = (ImageInfo.imageType == VK_IMAGE_TYPE_3D) ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		.Width = ImageInfo.extent.width,
		.Height = ImageInfo.extent.height,
		.DepthOrArraySize = static_cast<uint16_t>(ImageInfo.extent.depth),
		.MipLevels = static_cast<uint16_t>(ImageInfo.mipLevels),
		.Format = ffxGetDX12FormatFromSurfaceFormat(ffxGetSurfaceFormatVK(ImageInfo.format)),
		.SampleDesc = { .Count = 1, .Quality = 0 },
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS,
	};

	ID3D12Resource *createdResourceDX = nullptr;
	auto hr = m_DeviceDX->CreateCommittedResource(
		&d3d12HeapProperties,
		D3D12_HEAP_FLAG_SHARED,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&createdResourceDX));

	if (FAILED(hr))
		return false;

	HANDLE win32Handle = nullptr;
	hr = m_DeviceDX->CreateSharedHandle(createdResourceDX, nullptr, GENERIC_ALL, nullptr, &win32Handle);

	if (FAILED(hr))
	{
		createdResourceDX->Release();
		return false;
	}

	// Vulkan makes us create an image and allocate its backing memory by hand...
	//
	// "A VkExternalMemoryImageCreateInfo structure with a non-zero handleTypes field must
	// be included in the creation parameters for an image that will be bound to memory that
	// is either exported or imported."
	const VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO,
		.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
	};

	auto imageCreateInfoCopy = ImageInfo;
	imageCreateInfoCopy.pNext = &externalMemoryImageCreateInfo;

	VkImage createdResourceVK = VK_NULL_HANDLE;
	if (vkCreateImage(m_DeviceVK, &imageCreateInfoCopy, nullptr, &createdResourceVK) != VK_SUCCESS)
	{
		CloseHandle(win32Handle);
		createdResourceDX->Release();
		return false;
	}

	VkMemoryRequirements memoryRequirements = {};
	vkGetImageMemoryRequirements(m_DeviceVK, createdResourceVK, &memoryRequirements);

	VkMemoryWin32HandlePropertiesKHR memoryWin32HandleProperties = {};
	memoryWin32HandleProperties.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;

	m_vkGetMemoryWin32HandlePropertiesKHR(
		m_DeviceVK,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
		win32Handle,
		&memoryWin32HandleProperties);

	// "To import memory from a Windows handle, add a VkImportMemoryWin32HandleInfoKHR structure
	// to the pNext chain of the VkMemoryAllocateInfo structure."
	const VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO,
		.image = createdResourceVK,
		.buffer = VK_NULL_HANDLE,
	};

	const VkImportMemoryWin32HandleInfoKHR importMemoryWin32HandleInfo = {
		.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR,
		.pNext = &dedicatedAllocInfo,
		.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
		.handle = win32Handle,
	};

	const VkMemoryAllocateInfo memoryAllocInfo = {
		.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.pNext = &importMemoryWin32HandleInfo,
		.allocationSize = memoryRequirements.size,
		.memoryTypeIndex = FindVulkanMemoryTypeIndex(memoryWin32HandleProperties.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};

	VkDeviceMemory createdMemory = VK_NULL_HANDLE;
	if (vkAllocateMemory(m_DeviceVK, &memoryAllocInfo, nullptr, &createdMemory) != VK_SUCCESS)
	{
		vkDestroyImage(m_DeviceVK, createdResourceVK, nullptr);

		CloseHandle(win32Handle);
		createdResourceDX->Release();
		return false;
	}

	if (vkBindImageMemory(m_DeviceVK, createdResourceVK, createdMemory, 0) != VK_SUCCESS)
	{
		vkDestroyImage(m_DeviceVK, createdResourceVK, nullptr);
		vkFreeMemory(m_DeviceVK, createdMemory, nullptr);

		CloseHandle(win32Handle);
		createdResourceDX->Release();
		return false;
	}

	VulkanResource = createdResourceVK;
	VulkanMemory = createdMemory;
	D3D12Resource = createdResourceDX;

	CloseHandle(win32Handle);
	return true;
}

void FFFrameInterpolatorVKToDX::CopyVulkanTexture(
	VkCommandBuffer CommandList,
	VkImage SourceResource,
	VkImage DestinationResource,
	FfxResourceStates SourceState,
	FfxResourceStates DestinationState,
	VkExtent3D Extent,
	bool IsDepthAspect)
{
	std::array<VkImageMemoryBarrier, 2> barriers = {
		MakeVulkanBarrier(SourceResource, SourceState, FFX_RESOURCE_STATE_COPY_SRC, IsDepthAspect),
		MakeVulkanBarrier(DestinationResource, DestinationState, FFX_RESOURCE_STATE_COPY_DEST, IsDepthAspect),
	};

	vkCmdPipelineBarrier(
		CommandList,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		static_cast<uint32_t>(barriers.size()),
		barriers.data());

	VkImageCopy copyRegion = {};
	copyRegion.extent = Extent;
	copyRegion.dstSubresource.aspectMask = barriers[0].subresourceRange.aspectMask;
	copyRegion.dstSubresource.mipLevel = barriers[0].subresourceRange.baseMipLevel;
	copyRegion.dstSubresource.baseArrayLayer = barriers[0].subresourceRange.baseArrayLayer;
	copyRegion.dstSubresource.layerCount = barriers[0].subresourceRange.layerCount;
	copyRegion.srcSubresource = copyRegion.dstSubresource;

	vkCmdCopyImage(CommandList, barriers[0].image, barriers[0].newLayout, barriers[1].image, barriers[1].newLayout, 1, &copyRegion);

	std::swap(barriers[0].srcAccessMask, barriers[0].dstAccessMask);
	std::swap(barriers[0].oldLayout, barriers[0].newLayout);
	std::swap(barriers[1].srcAccessMask, barriers[1].dstAccessMask);
	std::swap(barriers[1].oldLayout, barriers[1].newLayout);

	vkCmdPipelineBarrier(
		CommandList,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		static_cast<uint32_t>(barriers.size()),
		barriers.data());
}

VkImageMemoryBarrier FFFrameInterpolatorVKToDX::MakeVulkanBarrier(
	VkImage Resource,
	FfxResourceStates SourceState,
	FfxResourceStates DestinationState,
	bool IsDepthAspect)
{
	return VkImageMemoryBarrier {
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask = getVKAccessFlagsFromResourceState(SourceState),
		.dstAccessMask = getVKAccessFlagsFromResourceState(DestinationState),
		.oldLayout = getVKImageLayoutFromResourceState(SourceState),
		.newLayout = getVKImageLayoutFromResourceState(DestinationState),
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = Resource,
		.subresourceRange = {
			.aspectMask = static_cast<VkImageAspectFlags>(IsDepthAspect ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
			.baseMipLevel = 0,
			.levelCount = VK_REMAINING_MIP_LEVELS,
			.baseArrayLayer = 0,
			.layerCount = VK_REMAINING_ARRAY_LAYERS,
		},
	};
}

bool FFFrameInterpolatorVKToDX::LoadVulkanResourceNGXInfo(
	NGXInstanceParameters *NGXParameters,
	const char *Name,
	VkImageCreateInfo& ImageInfo,
	VkImage& Resource)
{
	NGXVulkanResourceHandle *resourceHandle = nullptr;
	NGXParameters->GetVoidPointer(Name, reinterpret_cast<void **>(&resourceHandle));

	if (!resourceHandle)
		return false;

	if (resourceHandle->Type != 0) // TODO: Figure out where this is defined
		return false;

	ImageInfo = VkImageCreateInfo {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = resourceHandle->ImageMetadata.Format,
		.extent = { resourceHandle->ImageMetadata.Width, resourceHandle->ImageMetadata.Height, 1 },
		.mipLevels = resourceHandle->ImageMetadata.Subresource.levelCount,
		.arrayLayers = resourceHandle->ImageMetadata.Subresource.layerCount,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
	};

	Resource = resourceHandle->ImageMetadata.Image;
	return true;
}

bool FFFrameInterpolatorVKToDX::FindEquivalentDXGIAdapter(VkPhysicalDevice PhysicalDevice, IDXGIAdapter1 *& Adapter, uint32_t& NodeMask)
{
	VkPhysicalDeviceIDProperties idProperties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES,
	};

	VkPhysicalDeviceProperties2 properties = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
		.pNext = &idProperties,
	};

	vkGetPhysicalDeviceProperties2(PhysicalDevice, &properties);

	IDXGIAdapter1 *adapter = nullptr;

	if (idProperties.deviceLUIDValid == VK_TRUE)
	{
		IDXGIFactory1 *factory = nullptr;

		if (CreateDXGIFactory1(IID_PPV_ARGS(&factory)) == S_OK)
		{
			for (uint32_t i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; i++)
			{
				DXGI_ADAPTER_DESC desc = {};
				adapter->GetDesc(&desc);

				static_assert(sizeof(desc.AdapterLuid) == sizeof(idProperties.deviceLUID));

				if (memcmp(&desc.AdapterLuid, &idProperties.deviceLUID, sizeof(desc.AdapterLuid)) == 0)
					break;

				adapter->Release();
				adapter = nullptr;
			}

			factory->Release();
		}
	}

	if (!adapter)
		return false;

	Adapter = adapter;
	NodeMask = idProperties.deviceNodeMask;

	return true;
}

uint32_t FFFrameInterpolatorVKToDX::FindVulkanMemoryTypeIndex(uint32_t MemoryTypeBits, VkMemoryPropertyFlags PropertyFlags)
{
	VkPhysicalDeviceMemoryProperties memoryProperties = {};
	vkGetPhysicalDeviceMemoryProperties(m_PhysicalDeviceVK, &memoryProperties);

	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if (((1u << i) & MemoryTypeBits) == 0)
			continue;

		if ((memoryProperties.memoryTypes[i].propertyFlags & PropertyFlags) != PropertyFlags)
			continue;

		return i;
	}

	return 0xFFFFFFFF;
}
