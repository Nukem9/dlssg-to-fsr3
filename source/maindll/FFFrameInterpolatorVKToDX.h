#pragma once

#include <vector>
#include <Windows.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>
#include "NGX/NvNGX.h"
#include "FFFrameInterpolatorDX.h"

struct IDXGIAdapter1;
struct ID3D12CommandQueue;
struct ID3D12CommandAllocator;
struct ID3D12GraphicsCommandList;
struct ID3D12Fence;

class FFFrameInterpolatorVKToDX
{
	// https://github.com/NVIDIAGameWorks/dxvk-remix/blob/69bf38e89be5b185564516a8fa4e7bdda9b2ddfa/src/dxvk/rtx_render/rtx_ngx_wrapper.h#L167
	using PfnAppCreateTimelineSyncObjectsCallback = void(
		void *UserData,
		void **SignalObj,
		uint64_t SignalValue,
		void **WaitObj,
		uint64_t WaitValue);

	using PfnAppSyncSignalCallback = void(void *UserData, void **CommandList, void *SignalObj, uint64_t SignalValue);

	using PfnAppSyncWaitCallback = void(
		void *UserData,
		void **CommandList,
		void *WaitObj,
		uint64_t WaitValue,
		int WaitCPU,
		void * /*SignalObj*/,
		uint64_t /*SignalValue*/);

	using PfnAppSyncFlushCallback = void(
		void *UserData,
		void ** /*CommandList*/,
		void * /*SignalObj*/,
		uint64_t /*SignalValue*/,
		int /*WaitCPU*/);

private:
	const VkDevice m_DeviceVK;
	const VkPhysicalDevice m_PhysicalDeviceVK;

	ID3D12Device *m_DeviceDX = nullptr;
	ID3D12CommandQueue *m_CommandQueueDX = nullptr;
	ID3D12GraphicsCommandList *m_CommandListDX = nullptr;
	std::vector<ID3D12CommandAllocator *> m_TransientCommandAllocatorsDX;
	size_t m_NextTransientCommandAllocatorIndex = 0;

	PfnAppCreateTimelineSyncObjectsCallback *m_AppCreateTimelineSyncObjects = nullptr; // Create S1 and S4
	PfnAppSyncSignalCallback *m_AppSyncSignal = nullptr;							   // Wait on S0, submit CL1, signal S1, return CL2
	PfnAppSyncWaitCallback *m_AppSyncWait = nullptr;								   // Wait on S1, submit CL2, signal S2, wait on S4, return CL3
	PfnAppSyncFlushCallback *m_AppSyncFlush = nullptr;								   // Drain DLFG queue

	void *m_AppCreateTimelineSyncObjectsData = nullptr;
	void *m_AppSyncSignalData = nullptr;
	void *m_AppSyncWaitData = nullptr;
	void *m_AppSyncFlushData = nullptr;

	VkSemaphore m_SharedSemaphoreS1VK = VK_NULL_HANDLE;
	ID3D12Fence *m_SharedSemaphoreS1DX = nullptr;
	uint64_t m_SharedSemaphoreS1Counter = 0;

	VkSemaphore m_SharedSemaphoreS4VK = VK_NULL_HANDLE;
	ID3D12Fence *m_SharedSemaphoreS4DX = nullptr;
	uint64_t m_SharedSemaphoreS4Counter = 0;

	PFN_vkGetMemoryWin32HandlePropertiesKHR m_vkGetMemoryWin32HandlePropertiesKHR = nullptr;
	PFN_vkImportSemaphoreWin32HandleKHR m_vkImportSemaphoreWin32HandleKHR = nullptr;

	std::optional<FFFrameInterpolatorDX> m_FrameInterpolator;

public:
	FFFrameInterpolatorVKToDX(
		VkDevice LogicalDevice,
		VkPhysicalDevice PhysicalDevice,
		uint32_t OutputWidth,
		uint32_t OutputHeight,
		NGXInstanceParameters *NGXParameters);
	FFFrameInterpolatorVKToDX(const FFFrameInterpolatorVKToDX&) = delete;
	FFFrameInterpolatorVKToDX& operator=(const FFFrameInterpolatorVKToDX&) = delete;
	~FFFrameInterpolatorVKToDX();

	FfxErrorCode Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters);

private:
	void InitializeVulkanBackend(VkPhysicalDevice PhysicalDevice);
	void InitializeD3D12Backend(IDXGIAdapter1 *Adapter, uint32_t NodeMask);

	void CreateVulkanToDXGIAdapter(VkPhysicalDevice PhysicalDevice, IDXGIAdapter1 *& Adapter, uint32_t& NodeMask);
	bool CreateVulkanToD3D12SharedFence(uint64_t InitialValue, VkSemaphore& VulkanFence, ID3D12Fence *& D3D12Fence);
	bool CreateVulkanToD3D12SharedTexture(const VkImageCreateInfo& ImageInfo, VkImage& VulkanImage, ID3D12Resource *& D3D12Resource);

	ID3D12CommandAllocator *AllocateTransientCommandAllocator();
	uint32_t FindMemoryTypeIndex(uint32_t MemoryTypeBits, VkMemoryPropertyFlags PropertyFlags);

	static VkImageMemoryBarrier VkBarrier(VkImage Image, FfxResourceStates SourceState, FfxResourceStates DestinationState);

	static bool LoadVulkanResourceNGXInfo(
		NGXInstanceParameters *NGXParameters,
		const char *Name,
		VkImage& Image,
		VkImageCreateInfo& ImageInfo);
};
