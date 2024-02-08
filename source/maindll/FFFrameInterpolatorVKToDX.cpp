#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <dxgi.h>
#include <d3d12.h>
#include "FFFrameInterpolatorVKToDX.h"

DXGI_FORMAT ffxGetDX12FormatFromSurfaceFormat(FfxSurfaceFormat surfaceFormat);
VkAccessFlags getVKAccessFlagsFromResourceState(FfxResourceStates state);
VkImageLayout getVKImageLayoutFromResourceState(FfxResourceStates state);

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
	CreateVulkanToDXGIAdapter(PhysicalDevice, adapter, nodeMask);

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

	if (m_AppCreateTimelineSyncObjects)
		m_AppCreateTimelineSyncObjects(
			m_AppCreateTimelineSyncObjectsData,
			reinterpret_cast<void **>(&m_SharedSemaphoreS1VK),
			m_SharedSemaphoreS1Counter,
			reinterpret_cast<void **>(&m_SharedSemaphoreS4VK),
			m_SharedSemaphoreS4Counter);

	if (!CreateVulkanToD3D12SharedFence(m_SharedSemaphoreS1Counter, m_SharedSemaphoreS1VK, m_SharedSemaphoreS1DX) ||
		!CreateVulkanToD3D12SharedFence(m_SharedSemaphoreS4Counter, m_SharedSemaphoreS4VK, m_SharedSemaphoreS4DX))
		throw std::runtime_error("Failed to create shared fences");
}

FFFrameInterpolatorVKToDX::~FFFrameInterpolatorVKToDX()
{
	if (m_CommandListDX)
		m_CommandListDX->Release();

	for (auto& allocator : m_TransientCommandAllocatorsDX)
	{
		if (allocator)
			allocator->Release();
	}

	if (m_CommandQueueDX)
		m_CommandQueueDX->Release();

	if (m_DeviceDX)
		m_DeviceDX->Release();

	if (m_SharedSemaphoreS1VK != VK_NULL_HANDLE)
		vkDestroySemaphore(m_DeviceVK, m_SharedSemaphoreS1VK, nullptr);

	if (m_SharedSemaphoreS1DX)
		m_SharedSemaphoreS1DX->Release();

	if (m_SharedSemaphoreS4VK != VK_NULL_HANDLE)
		vkDestroySemaphore(m_DeviceVK, m_SharedSemaphoreS4VK, nullptr);

	if (m_SharedSemaphoreS4DX)
		m_SharedSemaphoreS4DX->Release();
}

FfxErrorCode FFFrameInterpolatorVKToDX::Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters)
{
	const bool isRecordingCommands = NGXParameters->GetUIntOrDefault("DLSSG.IsRecording", 0) != 0;

	void *recordingQueue = nullptr;
	NGXParameters->GetVoidPointer("DLSSG.CmdQueue", &recordingQueue);

	void *commandAllocator = nullptr;
	NGXParameters->GetVoidPointer("DLSSG.CmdAlloc", &commandAllocator);

	auto transferSharedNGX = [&](VkCommandBuffer CommandList,
								 const char *ResourceName,
								 FfxResourceStates ResourceState,
								 bool CopyRequired = true,
								 bool ReverseCopyOrder = false)
	{
		ID3D12Resource *sharedDX12Texture = nullptr;

		VkImageCreateInfo sharedCreateInfo = {};
		VkImage sharedVulkanImage = {};
		VkImage sourceVulkanImage = {};

		if (!LoadVulkanResourceNGXInfo(NGXParameters, ResourceName, sourceVulkanImage, sharedCreateInfo))
			return;

		if (!CreateVulkanToD3D12SharedTexture(sharedCreateInfo, sharedVulkanImage, sharedDX12Texture))
			__debugbreak();

		NGXParameters->SetVoidPointer(ResourceName, sharedDX12Texture);

		VkImageMemoryBarrier barriers[2] = {};
		barriers[0] = VkBarrier(sourceVulkanImage, ResourceState, FFX_RESOURCE_STATE_COPY_SRC);
		barriers[1] = VkBarrier(sharedVulkanImage, ResourceState, FFX_RESOURCE_STATE_COPY_DEST);

		if (ReverseCopyOrder)
			std::swap(barriers[0], barriers[1]);

		vkCmdPipelineBarrier(
			CommandList,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			barriers);

		VkImageCopy copyRegion = {};
		copyRegion.extent = sharedCreateInfo.extent;
		copyRegion.dstSubresource.aspectMask = barriers[0].subresourceRange.aspectMask;
		copyRegion.dstSubresource.mipLevel = barriers[0].subresourceRange.baseMipLevel;
		copyRegion.dstSubresource.baseArrayLayer = barriers[0].subresourceRange.baseArrayLayer;
		copyRegion.dstSubresource.layerCount = barriers[0].subresourceRange.layerCount;
		copyRegion.srcSubresource = copyRegion.dstSubresource;

		if (CopyRequired)
			vkCmdCopyImage(CommandList, barriers[1].image, barriers[1].newLayout, barriers[0].image, barriers[0].newLayout, 1, &copyRegion);

		std::swap(barriers[0].srcAccessMask, barriers[0].dstAccessMask);
		std::swap(barriers[0].oldLayout, barriers[0].newLayout);
		std::swap(barriers[1].srcAccessMask, barriers[1].dstAccessMask);
		std::swap(barriers[1].oldLayout, barriers[1].newLayout);

		vkCmdPipelineBarrier(
			CommandList,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
			0,
			0,
			nullptr,
			0,
			nullptr,
			1,
			barriers);
	};

	auto cmdList = reinterpret_cast<VkCommandBuffer>(CommandList); // CL1

	m_SharedSemaphoreS1Counter++;
	m_AppSyncSignal(
		m_AppSyncSignalData,
		reinterpret_cast<void **>(&cmdList),
		m_SharedSemaphoreS1VK,
		m_SharedSemaphoreS1Counter); // CL1 -> CL2

	m_SharedSemaphoreS4Counter++;
	m_AppSyncWait(m_AppSyncWaitData, reinterpret_cast<void **>(&cmdList), m_SharedSemaphoreS4VK, m_SharedSemaphoreS4Counter, 0, nullptr, 0); // CL2 -> CL3

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
		throw std::runtime_error("Handle import extensions are unavailable");

	VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {};
	semaphoreTypeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	semaphoreTypeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	semaphoreTypeCreateInfo.initialValue = 0;

	VkPhysicalDeviceExternalSemaphoreInfo externalSemaphoreInfo = {};
	externalSemaphoreInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
	externalSemaphoreInfo.pNext = &semaphoreTypeCreateInfo;
	externalSemaphoreInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;

	// We only need to know if semaphores can be imported
	VkExternalSemaphoreProperties externalSemaphoreProperties = {};
	externalSemaphoreProperties.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;

	vkGetPhysicalDeviceExternalSemaphoreProperties(PhysicalDevice, &externalSemaphoreInfo, &externalSemaphoreProperties);

	if ((externalSemaphoreProperties.externalSemaphoreFeatures & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT) == 0 ||
		(externalSemaphoreProperties.exportFromImportedHandleTypes & externalSemaphoreInfo.handleType) == 0)
		throw std::runtime_error("Vulkan instance doesn't support importing timeline semaphores");
}

void FFFrameInterpolatorVKToDX::InitializeD3D12Backend(IDXGIAdapter1 *Adapter, uint32_t NodeMask)
{
	auto hr = D3D12CreateDevice(Adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_DeviceDX));

	if (FAILED(hr))
		throw std::runtime_error("D3D12 device creation failed");

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH; // TODO: Unsure about this. Need to check Streamline.
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	hr = m_DeviceDX->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_CommandQueueDX));

	if (FAILED(hr))
		throw std::runtime_error("Failed to create D3D12 command queue");

	m_TransientCommandAllocatorsDX.resize(8);

	for (auto& allocator : m_TransientCommandAllocatorsDX)
	{
		hr = m_DeviceDX->CreateCommandAllocator(queueDesc.Type, IID_PPV_ARGS(&allocator));

		if (FAILED(hr))
			throw std::runtime_error("Failed to create D3D12 command allocator");
	}

	hr = m_DeviceDX->CreateCommandList(
		NodeMask,
		queueDesc.Type,
		m_TransientCommandAllocatorsDX[0],
		nullptr,
		IID_PPV_ARGS(&m_CommandListDX));

	if (FAILED(hr))
		throw std::runtime_error("Failed to create D3D12 command list");

	m_CommandListDX->Close();
}

void FFFrameInterpolatorVKToDX::CreateVulkanToDXGIAdapter(VkPhysicalDevice PhysicalDevice, IDXGIAdapter1*& Adapter, uint32_t& NodeMask)
{
	VkPhysicalDeviceIDProperties idProperties = {};
	idProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

	VkPhysicalDeviceProperties2 properties = {};
	properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	properties.pNext = &idProperties;

	vkGetPhysicalDeviceProperties2(PhysicalDevice, &properties);

	if (!idProperties.deviceLUIDValid)
		throw std::runtime_error("Vulkan device LUID isn't valid");

	IDXGIFactory1 *factory = nullptr;
	IDXGIAdapter1 *adapter = nullptr;

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

	if (!adapter)
		throw std::runtime_error("Failed to look up corresponding DXGI adapter for Vulkan device");

	Adapter = adapter;
	NodeMask = idProperties.deviceNodeMask;
}

bool FFFrameInterpolatorVKToDX::CreateVulkanToD3D12SharedFence(uint64_t InitialValue, VkSemaphore& VulkanFence, ID3D12Fence*& D3D12Fence)
{
	ID3D12Fence *createdFence = nullptr;
	auto hr = m_DeviceDX->CreateFence(InitialValue, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&createdFence));

	if (FAILED(hr))
		return false;

	HANDLE win32Handle = nullptr;
	hr = m_DeviceDX->CreateSharedHandle(createdFence, nullptr, GENERIC_ALL, nullptr, &win32Handle);

	if (FAILED(hr))
	{
		createdFence->Release();
		return false;
	}

	VkSemaphore createdSemaphore = VulkanFence;

	if (createdSemaphore == VK_NULL_HANDLE)
	{
		VkSemaphoreTypeCreateInfo semaphoreTypeCreateInfo = {};
		semaphoreTypeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
		semaphoreTypeCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
		semaphoreTypeCreateInfo.initialValue = InitialValue;

		VkSemaphoreCreateInfo semaphoreCreateInfo = {};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		semaphoreCreateInfo.pNext = &semaphoreTypeCreateInfo;

		if (vkCreateSemaphore(m_DeviceVK, &semaphoreCreateInfo, nullptr, &createdSemaphore) != VK_SUCCESS)
		{
			CloseHandle(win32Handle);
			createdFence->Release();
			return false;
		}
	}

	VkImportSemaphoreWin32HandleInfoKHR importSemaphoreInfo = {};
	importSemaphoreInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR;
	importSemaphoreInfo.semaphore = createdSemaphore;
	importSemaphoreInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_D3D12_FENCE_BIT;
	importSemaphoreInfo.handle = win32Handle;

	if (m_vkImportSemaphoreWin32HandleKHR(m_DeviceVK, &importSemaphoreInfo) != VK_SUCCESS)
	{
		if (createdSemaphore != VulkanFence)
			vkDestroySemaphore(m_DeviceVK, createdSemaphore, nullptr);

		CloseHandle(win32Handle);
		createdFence->Release();
		return false;
	}

	VulkanFence = createdSemaphore;
	D3D12Fence = createdFence;

	CloseHandle(win32Handle);
	return true;
}

bool FFFrameInterpolatorVKToDX::CreateVulkanToD3D12SharedTexture(const VkImageCreateInfo& ImageInfo, VkImage& VulkanImage, ID3D12Resource*& D3D12Resource)
{
	D3D12_HEAP_PROPERTIES d3d12HeapProperties = {};
	d3d12HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3d12HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3d12HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC d3d12ResourceDesc = {};
	d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3d12ResourceDesc.Width = ImageInfo.extent.width;
	d3d12ResourceDesc.Height = ImageInfo.extent.height;
	d3d12ResourceDesc.DepthOrArraySize = static_cast<uint16_t>(ImageInfo.extent.depth);
	d3d12ResourceDesc.MipLevels = static_cast<uint16_t>(ImageInfo.mipLevels);
	d3d12ResourceDesc.Format = ffxGetDX12FormatFromSurfaceFormat(ffxGetSurfaceFormatVK(ImageInfo.format));
	d3d12ResourceDesc.SampleDesc.Count = 1;
	d3d12ResourceDesc.SampleDesc.Quality = 0;
	d3d12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3d12ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS |
							  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	auto hr = m_DeviceDX->CreateCommittedResource(
		&d3d12HeapProperties,
		D3D12_HEAP_FLAG_SHARED,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&D3D12Resource));

	if (FAILED(hr))
		return false;

	HANDLE win32Handle = nullptr;
	hr = m_DeviceDX->CreateSharedHandle(D3D12Resource, nullptr, GENERIC_ALL, nullptr, &win32Handle);

	if (FAILED(hr))
	{
		D3D12Resource->Release();
		return false;
	}

	// Vulkan makes us create an image and allocate its backing memory by hand...
	//
	// "A VkExternalMemoryImageCreateInfo structure with a non-zero handleTypes field must
	// be included in the creation parameters for an image that will be bound to memory that
	// is either exported or imported."
	VkExternalMemoryImageCreateInfo externalMemoryImageCreateInfo = {};
	externalMemoryImageCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	externalMemoryImageCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;

	auto imageCreateInfoCopy = ImageInfo;
	imageCreateInfoCopy.pNext = &externalMemoryImageCreateInfo;

	if (vkCreateImage(m_DeviceVK, &imageCreateInfoCopy, nullptr, &VulkanImage) != VK_SUCCESS)
	{
		CloseHandle(win32Handle);
		D3D12Resource->Release();
		return false;
	}

	VkMemoryRequirements memoryRequirements = {};
	vkGetImageMemoryRequirements(m_DeviceVK, VulkanImage, &memoryRequirements);

	VkMemoryWin32HandlePropertiesKHR memoryWin32HandleProperties = {};
	memoryWin32HandleProperties.sType = VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR;

	m_vkGetMemoryWin32HandlePropertiesKHR(
		m_DeviceVK,
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT,
		win32Handle,
		&memoryWin32HandleProperties);

	// "To import memory from a Windows handle, add a VkImportMemoryWin32HandleInfoKHR structure
	// to the pNext chain of the VkMemoryAllocateInfo structure."
	VkDeviceMemory memory = {};

	VkMemoryDedicatedAllocateInfo dedicatedAllocInfo = {};
	dedicatedAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	dedicatedAllocInfo.image = VulkanImage;
	dedicatedAllocInfo.buffer = VK_NULL_HANDLE;

	VkImportMemoryWin32HandleInfoKHR importMemoryWin32HandleInfo = {};
	importMemoryWin32HandleInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR;
	importMemoryWin32HandleInfo.pNext = &dedicatedAllocInfo;
	importMemoryWin32HandleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE_BIT;
	importMemoryWin32HandleInfo.handle = win32Handle;

	VkMemoryAllocateInfo memoryAllocInfo = {};
	memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocInfo.pNext = &importMemoryWin32HandleInfo;
	memoryAllocInfo.allocationSize = memoryRequirements.size;
	memoryAllocInfo.memoryTypeIndex = FindMemoryTypeIndex(memoryWin32HandleProperties.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	if (vkAllocateMemory(m_DeviceVK, &memoryAllocInfo, nullptr, &memory) != VK_SUCCESS)
	{
		vkDestroyImage(m_DeviceVK, VulkanImage, nullptr);

		CloseHandle(win32Handle);
		D3D12Resource->Release();
		return false;
	}

	if (vkBindImageMemory(m_DeviceVK, VulkanImage, memory, 0) != VK_SUCCESS)
	{
		vkDestroyImage(m_DeviceVK, VulkanImage, nullptr);
		vkFreeMemory(m_DeviceVK, memory, nullptr);

		CloseHandle(win32Handle);
		D3D12Resource->Release();
		return false;
	}

	CloseHandle(win32Handle);
	return true;
}

ID3D12CommandAllocator *FFFrameInterpolatorVKToDX::AllocateTransientCommandAllocator()
{
	return m_TransientCommandAllocatorsDX[m_NextTransientCommandAllocatorIndex++ % m_TransientCommandAllocatorsDX.size()];
}

uint32_t FFFrameInterpolatorVKToDX::FindMemoryTypeIndex(uint32_t MemoryTypeBits, VkMemoryPropertyFlags PropertyFlags)
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

VkImageMemoryBarrier FFFrameInterpolatorVKToDX::VkBarrier(VkImage Image, FfxResourceStates SourceState, FfxResourceStates DestinationState)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = getVKAccessFlagsFromResourceState(SourceState);
	barrier.dstAccessMask = getVKAccessFlagsFromResourceState(DestinationState);
	barrier.oldLayout = getVKImageLayoutFromResourceState(SourceState);
	barrier.newLayout = getVKImageLayoutFromResourceState(DestinationState);
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = Image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	return barrier;
}

bool FFFrameInterpolatorVKToDX::LoadVulkanResourceNGXInfo(NGXInstanceParameters *NGXParameters, const char *Name, VkImage& Image, VkImageCreateInfo& ImageInfo)
{
	void *resource = nullptr;
	NGXParameters->GetVoidPointer(Name, &resource);

	if (!resource)
		return false;

	auto resourceHandle = static_cast<const NGXVulkanResourceHandle *>(resource);

	if (resourceHandle->Type != 0)
		__debugbreak();

	Image = resourceHandle->ImageMetadata.Image;

	ImageInfo = {};
	ImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ImageInfo.imageType = VK_IMAGE_TYPE_2D;
	ImageInfo.format = resourceHandle->ImageMetadata.Format;
	ImageInfo.extent = { resourceHandle->ImageMetadata.Width, resourceHandle->ImageMetadata.Height, 1 };
	ImageInfo.mipLevels = resourceHandle->ImageMetadata.Subresource.levelCount;
	ImageInfo.arrayLayers = resourceHandle->ImageMetadata.Subresource.layerCount;
	ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ImageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	return true;
}
