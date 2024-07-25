// This file is part of the FidelityFX SDK.
//
// Copyright (C) 2024 Advanced Micro Devices, Inc.
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <FidelityFX/host/ffx_interface.h>
#include <FidelityFX/host/ffx_util.h>
#include <FidelityFX/host/ffx_assert.h>
#include <FidelityFX/host/backends/vk/ffx_vk.h>
#include <ffx_shader_blobs.h>
#include <ffx_breadcrumbs_list.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <codecvt>  // this is deprecated so it's just a fallback solution
#endif  // _WIN32

#include <vulkan/vulkan.h>

// prototypes for functions in the interface
FfxVersionNumber       GetSDKVersionVK(FfxInterface* backendInterface);
FfxErrorCode           GetEffectGpuMemoryUsageVK(FfxInterface* backendInterface, FfxUInt32 effectContextId, FfxEffectMemoryUsage* outVramUsage);
FfxErrorCode           CreateBackendContextVK(FfxInterface* backendInterface, FfxEffectBindlessConfig* bindlessConfig, FfxUInt32* effectContextId);
FfxErrorCode           GetDeviceCapabilitiesVK(FfxInterface* backendInterface, FfxDeviceCapabilities* deviceCapabilities);
FfxErrorCode           DestroyBackendContextVK(FfxInterface* backendInterface, FfxUInt32 effectContextId);
FfxErrorCode           CreateResourceVK(FfxInterface* backendInterface, const FfxCreateResourceDescription* desc, FfxUInt32 effectContextId, FfxResourceInternal* outTexture);
FfxErrorCode           DestroyResourceVK(FfxInterface* backendInterface, FfxResourceInternal resource, FfxUInt32 effectContextId);
FfxErrorCode           MapResourceVK(FfxInterface* backendInterface, FfxResourceInternal resource, void** ptr);
FfxErrorCode           UnmapResourceVK(FfxInterface* backendInterface, FfxResourceInternal resource);
FfxErrorCode           RegisterResourceVK(FfxInterface* backendInterface, const FfxResource* inResource, FfxUInt32 effectContextId, FfxResourceInternal* outResourceInternal);
FfxResource            GetResourceVK(FfxInterface* backendInterface, FfxResourceInternal resource);
FfxErrorCode           UnregisterResourcesVK(FfxInterface* backendInterface, FfxCommandList commandList, FfxUInt32 effectContextId);
FfxErrorCode           RegisterStaticResourceVK(FfxInterface* backendInterface, const FfxStaticResourceDescription* desc, FfxUInt32 effectContextId);
FfxResourceDescription GetResourceDescriptionVK(FfxInterface* backendInterface, FfxResourceInternal resource);
FfxErrorCode           StageConstantBufferDataVK(FfxInterface* backendInterface, void* data, FfxUInt32 size, FfxConstantBuffer* constantBuffer);
FfxErrorCode           CreatePipelineVK(FfxInterface* backendInterface, FfxEffect effect, FfxPass passId, uint32_t permutationOptions, const FfxPipelineDescription* desc, FfxUInt32 effectContextId, FfxPipelineState* outPass);
FfxErrorCode           DestroyPipelineVK(FfxInterface* backendInterface, FfxPipelineState* pipeline, FfxUInt32 effectContextId);
FfxErrorCode           ScheduleGpuJobVK(FfxInterface* backendInterface, const FfxGpuJobDescription* job);
FfxErrorCode           ExecuteGpuJobsVK(FfxInterface* backendInterface, FfxCommandList commandList, FfxUInt32 effectContextId);
FfxErrorCode           BreadcrumbsAllocBlockVK(FfxInterface* backendInterface, uint64_t blockBytes, FfxBreadcrumbsBlockData* blockData);
void                   BreadcrumbsFreeBlockVK(FfxInterface* backendInterface, FfxBreadcrumbsBlockData* blockData);
void                   BreadcrumbsWriteVK(FfxInterface* backendInterface, FfxCommandList commandList, uint32_t value, uint64_t gpuLocation, void* gpuBuffer, bool isBegin);
void                   BreadcrumbsPrintDeviceInfoVK(FfxInterface* backendInterface, FfxAllocationCallbacks* allocs, bool extendedInfo, char** printBuffer, size_t* printSize);
void                   RegisterConstantBufferAllocatorVK(FfxInterface* backendInterface, FfxConstantBufferAllocator fpConstantAllocator);


static VkDeviceContext sVkDeviceContext = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE };

#define MAX_PIPELINE_USAGE_PER_FRAME      (10) // Required to make sure passes that are called more than once per-frame don't have their descriptors overwritten.
#define MAX_DESCRIPTOR_SET_LAYOUTS        (64)
#define FFX_MAX_BINDLESS_DESCRIPTOR_COUNT (65536)

// Constant buffer allocation callback
static FfxConstantBufferAllocator s_fpConstantAllocator = nullptr;

// Redefine offsets for compilation purposes
#define BINDING_SHIFT(name, shift)                       \
constexpr uint32_t name##_BINDING_SHIFT     = shift; \
constexpr wchar_t* name##_BINDING_SHIFT_STR = L#shift;

// put it there for now
BINDING_SHIFT(TEXTURE, 0);
BINDING_SHIFT(SAMPLER, 1000);
BINDING_SHIFT(UNORDERED_ACCESS_VIEW, 2000);
BINDING_SHIFT(CONSTANT_BUFFER, 3000);

typedef struct BackendContext_VK {

    // store for resources and resourceViews
    typedef struct Resource
    {
#ifdef _DEBUG
        char                    resourceName[64] = {};
#endif
        union {
            VkImage             imageResource;
            VkBuffer            bufferResource;
        };

        FfxResourceDescription  resourceDescription;
        FfxResourceStates       initialState;
        FfxResourceStates       currentState;
        int32_t                 srvViewIndex;
        int32_t                 uavViewIndex;
        uint32_t                uavViewCount;

        VkDeviceMemory          deviceMemory;
        VkMemoryPropertyFlags   memoryProperties;

        bool                    undefined;
        bool                    dynamic;

    } Resource;

    typedef struct PipelineLayout {

        VkSampler               samplers[FFX_MAX_SAMPLERS];
        VkDescriptorSetLayout   descriptorSetLayout;
        VkDescriptorSet         descriptorSets[FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME];
        uint32_t                descriptorSetIndex;
        VkPipelineLayout        pipelineLayout;
        int32_t                 staticTextureSrvSet;
        int32_t                 staticBufferSrvSet;
        int32_t                 staticTextureUavSet;
        int32_t                 staticBufferUavSet;
    } PipelineLayout;

    typedef struct VKFunctionTable
    {
        PFN_vkGetDeviceProcAddr                 vkGetDeviceProcAddr = 0;
        PFN_vkSetDebugUtilsObjectNameEXT        vkSetDebugUtilsObjectNameEXT = 0;
        PFN_vkCreateDescriptorPool              vkCreateDescriptorPool = 0;
        PFN_vkCreateSampler                     vkCreateSampler = 0;
        PFN_vkCreateDescriptorSetLayout         vkCreateDescriptorSetLayout = 0;
        PFN_vkCreateBuffer                      vkCreateBuffer = 0;
        PFN_vkCreateBufferView                  vkCreateBufferView = 0;
        PFN_vkCreateImage                       vkCreateImage = 0;
        PFN_vkCreateImageView                   vkCreateImageView = 0;
        PFN_vkCreateShaderModule                vkCreateShaderModule = 0;
        PFN_vkCreatePipelineLayout              vkCreatePipelineLayout = 0;
        PFN_vkCreateComputePipelines            vkCreateComputePipelines = 0;
        PFN_vkDestroyPipelineLayout             vkDestroyPipelineLayout = 0;
        PFN_vkDestroyPipeline                   vkDestroyPipeline = 0;
        PFN_vkDestroyImage                      vkDestroyImage = 0;
        PFN_vkDestroyImageView                  vkDestroyImageView = 0;
        PFN_vkDestroyBuffer                     vkDestroyBuffer = 0;
        PFN_vkDestroyBufferView                 vkDestroyBufferView = 0;
        PFN_vkDestroyDescriptorSetLayout        vkDestroyDescriptorSetLayout = 0;
        PFN_vkDestroyDescriptorPool             vkDestroyDescriptorPool = 0;
        PFN_vkDestroySampler                    vkDestroySampler = 0;
        PFN_vkDestroyShaderModule               vkDestroyShaderModule = 0;
        PFN_vkGetBufferMemoryRequirements       vkGetBufferMemoryRequirements = 0;
        PFN_vkGetBufferMemoryRequirements2KHR   vkGetBufferMemoryRequirements2KHR = 0;
        PFN_vkGetImageMemoryRequirements        vkGetImageMemoryRequirements = 0;
        PFN_vkAllocateDescriptorSets            vkAllocateDescriptorSets = 0;
        PFN_vkFreeDescriptorSets                vkFreeDescriptorSets = 0;
        PFN_vkAllocateMemory                    vkAllocateMemory = 0;
        PFN_vkFreeMemory                        vkFreeMemory = 0;
        PFN_vkMapMemory                         vkMapMemory = 0;
        PFN_vkUnmapMemory                       vkUnmapMemory = 0;
        PFN_vkBindBufferMemory                  vkBindBufferMemory = 0;
        PFN_vkBindImageMemory                   vkBindImageMemory = 0;
        PFN_vkUpdateDescriptorSets              vkUpdateDescriptorSets = 0;
        PFN_vkFlushMappedMemoryRanges           vkFlushMappedMemoryRanges = 0;
        PFN_vkCmdPipelineBarrier                vkCmdPipelineBarrier = 0;
        PFN_vkCmdBindPipeline                   vkCmdBindPipeline = 0;
        PFN_vkCmdBindDescriptorSets             vkCmdBindDescriptorSets = 0;
        PFN_vkCmdDispatch                       vkCmdDispatch = 0;
        PFN_vkCmdDispatchIndirect               vkCmdDispatchIndirect = 0;
        PFN_vkCmdCopyBuffer                     vkCmdCopyBuffer = 0;
        PFN_vkCmdCopyImage                      vkCmdCopyImage = 0;
        PFN_vkCmdCopyBufferToImage              vkCmdCopyBufferToImage = 0;
        PFN_vkCmdClearColorImage                vkCmdClearColorImage = 0;
        PFN_vkCmdFillBuffer                     vkCmdFillBuffer = 0;
        PFN_vkCmdWriteBufferMarkerAMD           vkCmdWriteBufferMarkerAMD = 0;
        PFN_vkCmdWriteBufferMarker2AMD          vkCmdWriteBufferMarker2AMD = 0;
        PFN_vkCmdBeginDebugUtilsLabelEXT        vkCmdBeginDebugUtilsLabelEXT = 0;
        PFN_vkCmdEndDebugUtilsLabelEXT          vkCmdEndDebugUtilsLabelEXT = 0;

    } VkFunctionTable;

    uint32_t refCount;
    uint32_t maxEffectContexts;

    VkDevice                device = VK_NULL_HANDLE;
    VkPhysicalDevice        physicalDevice = VK_NULL_HANDLE;
    VkFunctionTable         vkFunctionTable = {};

    FfxGpuJobDescription*   pGpuJobs;
    uint32_t                gpuJobCount = 0;

    typedef struct VkResourceView {
        VkImageView imageView;
    } VkResourceView;
    VkResourceView*         pResourceViews;

    uint8_t*                pStagingRingBuffer;
    uint32_t                stagingRingBufferBase = 0;

    PipelineLayout*         pPipelineLayouts;

    VkDescriptorPool        descriptorPool;
    uint32_t                bindlessBase;

    VkImageMemoryBarrier    imageMemoryBarriers[FFX_MAX_BARRIERS] = {};
    VkBufferMemoryBarrier   bufferMemoryBarriers[FFX_MAX_BARRIERS] = {};
    uint32_t                scheduledImageBarrierCount = 0;
    uint32_t                scheduledBufferBarrierCount = 0;
    VkPipelineStageFlags    srcStageMask = 0;
    VkPipelineStageFlags    dstStageMask = 0;

    typedef struct alignas(32) EffectContext {

        // Resource allocation
        uint32_t              nextStaticResource;
        uint32_t              nextDynamicResource;

        // UAV offsets
        uint32_t              nextStaticResourceView;
        uint32_t              nextDynamicResourceView[FFX_MAX_QUEUED_FRAMES];

        // Bindless descriptors
        uint32_t              bindlessTextureSrvHeapStart;
        uint32_t              bindlessTextureSrvHeapSize;
        uint32_t              bindlessBufferSrvHeapSize;
        uint32_t              bindlessTextureUavHeapStart;
        uint32_t              bindlessTextureUavHeapSize;
        uint32_t              bindlessBufferUavHeapSize;
        VkDescriptorPool      bindlessDescriptorPool;
        VkDescriptorSetLayout bindlessTextureSrvDescriptorSetLayout;
        VkDescriptorSetLayout bindlessBufferSrvDescriptorSetLayout;
        VkDescriptorSetLayout bindlessTextureUavDescriptorSetLayout;
        VkDescriptorSetLayout bindlessBufferUavDescriptorSetLayout;
        VkDescriptorSet       bindlessTextureSrvDescriptorSet;
        VkDescriptorSet       bindlessBufferSrvDescriptorSet;
        VkDescriptorSet       bindlessTextureUavDescriptorSet;
        VkDescriptorSet       bindlessBufferUavDescriptorSet;

        // Pipeline layout
        uint32_t              nextPipelineLayout;

        // the frame index for the context
        uint32_t              frameIndex;

        // Usage
        bool                  active;

    } EffectContext;

    Resource*               pResources;
    EffectContext*          pEffectContexts;

     // Allocation defaults
    FfxConstantAllocation FallbackConstantAllocator(void* data, FfxUInt64 dataSize);
    VkDeviceMemory        uniformBufferMemory = VK_NULL_HANDLE;
    VkMemoryPropertyFlags uniformBufferMemoryProperties;
    VkDeviceSize          uniformBufferAlignment = 0;
    void*                 uniformBufferMem       = nullptr;
    VkBuffer              uniformBuffer          = VK_NULL_HANDLE;
    VkDeviceSize          uniformBufferSize      = 0;
    VkDeviceSize          uniformBufferOffset    = 0;
    std::mutex            uniformBufferMutex;

    uint32_t                numDeviceExtensions = 0;
    VkExtensionProperties*  extensionProperties = nullptr;

    typedef enum BreadcrumbsFlags
    {
        BREADCRUMBS_DEDICATED_MEMORY_ENABLED    = 0x01,
        BREADCRUMBS_BUFFER_MARKER_ENABLED       = 0x02,
        BREADCRUMBS_SYNCHRONIZATION2_ENABLED    = 0x04,
    } BreadcrumbsFlags;
    uint8_t                 breadcrumbsFlags = 0;
    uint32_t                breadcrumbsMemoryIndex = 0;

} BackendContext_VK;

FFX_API size_t ffxGetScratchMemorySizeVK(VkPhysicalDevice physicalDevice, size_t maxContexts)
{
    uint32_t numExtensions = 0;

    if (physicalDevice)
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &numExtensions, nullptr);

    uint32_t extensionPropArraySize = sizeof(VkExtensionProperties) * numExtensions;
    uint32_t gpuJobDescArraySize = FFX_ALIGN_UP(maxContexts * FFX_MAX_GPU_JOBS * sizeof(FfxGpuJobDescription), sizeof(uint32_t));
    uint32_t resourceViewArraySize = FFX_ALIGN_UP(((maxContexts * FFX_MAX_QUEUED_FRAMES * FFX_MAX_RESOURCE_COUNT * 2) + FFX_MAX_BINDLESS_DESCRIPTOR_COUNT) * sizeof(BackendContext_VK::VkResourceView), sizeof(uint32_t));
    uint32_t stagingRingBufferArraySize = FFX_ALIGN_UP(maxContexts * FFX_CONSTANT_BUFFER_RING_BUFFER_SIZE, sizeof(uint32_t));
    uint32_t pipelineArraySize = FFX_ALIGN_UP(maxContexts * FFX_MAX_PASS_COUNT * sizeof(BackendContext_VK::PipelineLayout), sizeof(uint32_t));
    uint32_t resourceArraySize = FFX_ALIGN_UP(maxContexts * FFX_MAX_RESOURCE_COUNT * sizeof(BackendContext_VK::Resource), sizeof(uint32_t));
    uint32_t contextArraySize = FFX_ALIGN_UP(maxContexts * sizeof(BackendContext_VK::EffectContext), sizeof(uint32_t));
    
    return FFX_ALIGN_UP(sizeof(BackendContext_VK) + extensionPropArraySize + gpuJobDescArraySize + resourceViewArraySize + stagingRingBufferArraySize +
                            pipelineArraySize + resourceArraySize + contextArraySize,
                        sizeof(uint64_t));
}

// Create a FfxDevice from a VkDevice
FfxDevice ffxGetDeviceVK(VkDeviceContext* vkDeviceContext)
{
    sVkDeviceContext = *vkDeviceContext;
    return reinterpret_cast<FfxDevice>(&sVkDeviceContext);
}

FfxErrorCode ffxGetInterfaceVK(
    FfxInterface* backendInterface,
    FfxDevice device,
    void* scratchBuffer,
    size_t scratchBufferSize,
    size_t maxContexts)
{
    FFX_RETURN_ON_ERROR(
        backendInterface,
        FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(
        scratchBuffer,
        FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(
        scratchBufferSize >= ffxGetScratchMemorySizeVK(((VkDeviceContext*)device)->vkPhysicalDevice, maxContexts),
        FFX_ERROR_INSUFFICIENT_MEMORY);

    backendInterface->fpGetSDKVersion = GetSDKVersionVK;
    backendInterface->fpGetEffectGpuMemoryUsage = GetEffectGpuMemoryUsageVK;
    backendInterface->fpCreateBackendContext = CreateBackendContextVK;
    backendInterface->fpGetDeviceCapabilities = GetDeviceCapabilitiesVK;
    backendInterface->fpDestroyBackendContext = DestroyBackendContextVK;
    backendInterface->fpCreateResource = CreateResourceVK;
    backendInterface->fpDestroyResource = DestroyResourceVK;
    backendInterface->fpMapResource = MapResourceVK;
    backendInterface->fpUnmapResource = UnmapResourceVK;
    backendInterface->fpRegisterResource = RegisterResourceVK;
    backendInterface->fpGetResource = GetResourceVK;
    backendInterface->fpUnregisterResources = UnregisterResourcesVK;
    backendInterface->fpRegisterStaticResource = RegisterStaticResourceVK;
    backendInterface->fpGetResourceDescription = GetResourceDescriptionVK;
    backendInterface->fpStageConstantBufferDataFunc = StageConstantBufferDataVK;
    backendInterface->fpCreatePipeline = CreatePipelineVK;
    backendInterface->fpDestroyPipeline = DestroyPipelineVK;
    backendInterface->fpGetPermutationBlobByIndex = ffxGetPermutationBlobByIndex;
    backendInterface->fpScheduleGpuJob = ScheduleGpuJobVK;
    backendInterface->fpExecuteGpuJobs = ExecuteGpuJobsVK;
    backendInterface->fpBreadcrumbsAllocBlock = BreadcrumbsAllocBlockVK;
    backendInterface->fpBreadcrumbsFreeBlock = BreadcrumbsFreeBlockVK;
    backendInterface->fpBreadcrumbsWrite = BreadcrumbsWriteVK;
    backendInterface->fpBreadcrumbsPrintDeviceInfo = BreadcrumbsPrintDeviceInfoVK;
    backendInterface->fpRegisterConstantBufferAllocator = RegisterConstantBufferAllocatorVK;
    backendInterface->fpSwapChainConfigureFrameGeneration = ffxSetFrameGenerationConfigToSwapchainVK;

    // Memory assignments
    backendInterface->scratchBuffer = scratchBuffer;
    backendInterface->scratchBufferSize = scratchBufferSize;

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    FFX_RETURN_ON_ERROR(!backendContext->refCount, FFX_ERROR_BACKEND_API_ERROR);

    // Clear everything out
    memset(backendContext, 0, sizeof(*backendContext));

    // Map the device
    backendInterface->device = device;

    // Assign the max number of contexts we'll be using
    backendContext->maxEffectContexts = (uint32_t)maxContexts;

    return FFX_OK;
}

FfxCommandList ffxGetCommandListVK(VkCommandBuffer cmdBuf)
{
    FFX_ASSERT(NULL != cmdBuf);
    return reinterpret_cast<FfxCommandList>(cmdBuf);
}

FfxPipeline ffxGetPipelineVK(VkPipeline pipeline)
{
    FFX_ASSERT(NULL != pipeline);
    return reinterpret_cast<FfxPipeline>(pipeline);
}

FfxResource ffxGetResourceVK(void* vkResource,
    FfxResourceDescription          ffxResDescription,
    const wchar_t* ffxResName,
    FfxResourceStates               state /*=FFX_RESOURCE_STATE_COMPUTE_READ*/)
{
    FfxResource resource = {};
    resource.resource = vkResource;
    resource.state = state;
    resource.description = ffxResDescription;

#ifdef _DEBUG
    if (ffxResName) {
        wcscpy_s(resource.name, ffxResName);
    }
#endif

    return resource;
}

uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, VkMemoryRequirements memRequirements, VkMemoryPropertyFlags requestedProperties, VkMemoryPropertyFlags& outProperties)
{
    FFX_ASSERT(NULL != physicalDevice);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    uint32_t bestCandidate = UINT32_MAX;

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & requestedProperties)) {

            // if just device-local memory is requested, make sure this is the invisible heap to prevent over-subscribing the local heap
            if (requestedProperties == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
                continue;

            bestCandidate = i;
            outProperties = memProperties.memoryTypes[i].propertyFlags;

            // if host-visible memory is requested, check for host coherency as well and if available, return immediately
            if ((requestedProperties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) && (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
                return bestCandidate;
        }
    }

    return bestCandidate;
}

VkBufferUsageFlags ffxGetVKBufferUsageFlagsFromResourceUsage(FfxResourceUsage flags)
{
    VkBufferUsageFlags indirectBit = 0;

    if (FFX_CONTAINS_FLAG(flags, FFX_RESOURCE_USAGE_INDIRECT))
        indirectBit = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    if (FFX_CONTAINS_FLAG(flags, FFX_RESOURCE_USAGE_UAV))
        return indirectBit | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    else
        return indirectBit | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
}

VkImageType ffxGetVKImageTypeFromResourceType(FfxResourceType type)
{
    switch (type) {
    case(FFX_RESOURCE_TYPE_TEXTURE1D):
        return VK_IMAGE_TYPE_1D;
    case(FFX_RESOURCE_TYPE_TEXTURE2D):
        return VK_IMAGE_TYPE_2D;
    case(FFX_RESOURCE_TYPE_TEXTURE_CUBE):
    case(FFX_RESOURCE_TYPE_TEXTURE3D):
        return VK_IMAGE_TYPE_3D;
    default:
        return VK_IMAGE_TYPE_MAX_ENUM;
    }
}

bool ffxIsSurfaceFormatSRGB(FfxSurfaceFormat fmt)
{
    switch (fmt)
    {
    case (FFX_SURFACE_FORMAT_R8G8B8A8_SRGB):
    case (FFX_SURFACE_FORMAT_B8G8R8A8_SRGB):
        return true;
    case (FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS):
    case (FFX_SURFACE_FORMAT_R32G32B32A32_UINT):
    case (FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT):
    case (FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT):
    case (FFX_SURFACE_FORMAT_R32G32B32_FLOAT):
    case (FFX_SURFACE_FORMAT_R32G32_FLOAT):
    case (FFX_SURFACE_FORMAT_R8_UINT):
    case (FFX_SURFACE_FORMAT_R32_UINT):
    case (FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS):
    case (FFX_SURFACE_FORMAT_R8G8B8A8_UNORM):
    case (FFX_SURFACE_FORMAT_R8G8B8A8_SNORM):
    case (FFX_SURFACE_FORMAT_B8G8R8A8_TYPELESS):
    case (FFX_SURFACE_FORMAT_B8G8R8A8_UNORM):
    case (FFX_SURFACE_FORMAT_R11G11B10_FLOAT):
    case (FFX_SURFACE_FORMAT_R10G10B10A2_UNORM):
    case (FFX_SURFACE_FORMAT_R16G16_FLOAT):
    case (FFX_SURFACE_FORMAT_R16G16_UINT):
    case (FFX_SURFACE_FORMAT_R16G16_SINT):
    case (FFX_SURFACE_FORMAT_R16_FLOAT):
    case (FFX_SURFACE_FORMAT_R16_UINT):
    case (FFX_SURFACE_FORMAT_R16_UNORM):
    case (FFX_SURFACE_FORMAT_R16_SNORM):
    case (FFX_SURFACE_FORMAT_R8_UNORM):
    case (FFX_SURFACE_FORMAT_R8G8_UNORM):
    case (FFX_SURFACE_FORMAT_R8G8_UINT):
    case (FFX_SURFACE_FORMAT_R32_FLOAT):
    case (FFX_SURFACE_FORMAT_UNKNOWN):
        return false;
    default:
        FFX_ASSERT_MESSAGE(false, "Format not yet supported");
        return false;
    }
}

FfxSurfaceFormat ffxGetSurfaceFormatFromGamma(FfxSurfaceFormat fmt)
{
    switch (fmt)
    {
    case (FFX_SURFACE_FORMAT_R8G8B8A8_SRGB):
        return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
    case (FFX_SURFACE_FORMAT_B8G8R8A8_SRGB):
        return FFX_SURFACE_FORMAT_B8G8R8A8_UNORM;
    default:
        return fmt;
    }
}

FfxSurfaceFormat ffxGetSurfaceFormatToGamma(FfxSurfaceFormat fmt)
{
    switch (fmt)
    {
    case (FFX_SURFACE_FORMAT_R8G8B8A8_UNORM):
        return FFX_SURFACE_FORMAT_R8G8B8A8_SRGB;
    case (FFX_SURFACE_FORMAT_B8G8R8A8_UNORM):
        return FFX_SURFACE_FORMAT_B8G8R8A8_SRGB;
    default:
        return fmt;
    }
}

VkFormat ffxGetVkFormatFromSurfaceFormat(FfxSurfaceFormat fmt)
{
    switch (fmt)
    {
    case(FFX_SURFACE_FORMAT_UNKNOWN):
        return VK_FORMAT_UNDEFINED;
    case(FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS):
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case(FFX_SURFACE_FORMAT_R32G32B32A32_UINT):
        return VK_FORMAT_R32G32B32A32_UINT;
    case(FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT):
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case(FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT):
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case (FFX_SURFACE_FORMAT_R32G32B32_FLOAT):
        return VK_FORMAT_R32G32B32_SFLOAT;
    case(FFX_SURFACE_FORMAT_R32G32_FLOAT):
        return VK_FORMAT_R32G32_SFLOAT;
    case(FFX_SURFACE_FORMAT_R8_UINT):
        return VK_FORMAT_R8_UINT;
    case(FFX_SURFACE_FORMAT_R32_UINT):
        return VK_FORMAT_R32_UINT;
    case(FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS):
        return VK_FORMAT_R8G8B8A8_UNORM;
    case(FFX_SURFACE_FORMAT_R8G8B8A8_UNORM):
        return VK_FORMAT_R8G8B8A8_UNORM;
    case (FFX_SURFACE_FORMAT_R8G8B8A8_SNORM):
        return VK_FORMAT_R8G8B8A8_SNORM;
    case(FFX_SURFACE_FORMAT_R8G8B8A8_SRGB):
        return VK_FORMAT_R8G8B8A8_SRGB;
    case(FFX_SURFACE_FORMAT_B8G8R8A8_TYPELESS):
        return VK_FORMAT_B8G8R8A8_UNORM;
    case(FFX_SURFACE_FORMAT_B8G8R8A8_UNORM):
        return VK_FORMAT_B8G8R8A8_UNORM;
    case (FFX_SURFACE_FORMAT_B8G8R8A8_SRGB):
        return VK_FORMAT_B8G8R8A8_SRGB;
    case(FFX_SURFACE_FORMAT_R11G11B10_FLOAT):
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case (FFX_SURFACE_FORMAT_R10G10B10A2_UNORM):
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case(FFX_SURFACE_FORMAT_R16G16_FLOAT):
        return VK_FORMAT_R16G16_SFLOAT;
    case(FFX_SURFACE_FORMAT_R16G16_UINT):
        return VK_FORMAT_R16G16_UINT;
    case(FFX_SURFACE_FORMAT_R16G16_SINT):
        return VK_FORMAT_R16G16_SINT;
    case(FFX_SURFACE_FORMAT_R16_FLOAT):
        return VK_FORMAT_R16_SFLOAT;
    case(FFX_SURFACE_FORMAT_R16_UINT):
        return VK_FORMAT_R16_UINT;
    case(FFX_SURFACE_FORMAT_R16_UNORM):
        return VK_FORMAT_R16_UNORM;
    case(FFX_SURFACE_FORMAT_R16_SNORM):
        return VK_FORMAT_R16_SNORM;
    case(FFX_SURFACE_FORMAT_R8_UNORM):
        return VK_FORMAT_R8_UNORM;
    case(FFX_SURFACE_FORMAT_R8G8_UNORM):
        return VK_FORMAT_R8G8_UNORM;
    case(FFX_SURFACE_FORMAT_R8G8_UINT):
        return VK_FORMAT_R8G8_UINT;
    case(FFX_SURFACE_FORMAT_R32_FLOAT):
        return VK_FORMAT_R32_SFLOAT;

    default:
        FFX_ASSERT_MESSAGE(false, "Format not yet supported");
        return VK_FORMAT_UNDEFINED;
    }
}

VkFormat ffxGetVKUAVFormatFromSurfaceFormat(FfxSurfaceFormat fmt)
{
    switch (fmt)
    {
    case(FFX_SURFACE_FORMAT_UNKNOWN):
        return VK_FORMAT_UNDEFINED;
    case(FFX_SURFACE_FORMAT_R32G32B32A32_TYPELESS):
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case(FFX_SURFACE_FORMAT_R32G32B32A32_UINT):
        return VK_FORMAT_R32G32B32A32_UINT;
    case(FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT):
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case(FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT):
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case(FFX_SURFACE_FORMAT_R32G32B32_FLOAT):
        return VK_FORMAT_R32G32B32_SFLOAT;
    case(FFX_SURFACE_FORMAT_R32G32_FLOAT):
        return VK_FORMAT_R32G32_SFLOAT;
    case(FFX_SURFACE_FORMAT_R8_UINT):
        return VK_FORMAT_R8_UINT;
    case(FFX_SURFACE_FORMAT_R32_UINT):
        return VK_FORMAT_R32_UINT;
    case(FFX_SURFACE_FORMAT_R8G8B8A8_TYPELESS):
    case(FFX_SURFACE_FORMAT_R8G8B8A8_UNORM):
    case(FFX_SURFACE_FORMAT_R8G8B8A8_SRGB):
        return VK_FORMAT_R8G8B8A8_UNORM;
    case(FFX_SURFACE_FORMAT_R8G8B8A8_SNORM):
        return VK_FORMAT_R8G8B8A8_SNORM;
    case(FFX_SURFACE_FORMAT_B8G8R8A8_TYPELESS):
    case(FFX_SURFACE_FORMAT_B8G8R8A8_UNORM):
    case (FFX_SURFACE_FORMAT_B8G8R8A8_SRGB):
        return VK_FORMAT_B8G8R8A8_UNORM;
    case(FFX_SURFACE_FORMAT_R11G11B10_FLOAT):
        return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
    case (FFX_SURFACE_FORMAT_R10G10B10A2_UNORM):
        return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case(FFX_SURFACE_FORMAT_R16G16_FLOAT):
        return VK_FORMAT_R16G16_SFLOAT;
    case(FFX_SURFACE_FORMAT_R16G16_UINT):
        return VK_FORMAT_R16G16_UINT;
    case(FFX_SURFACE_FORMAT_R16G16_SINT):
        return VK_FORMAT_R16G16_SINT;
    case(FFX_SURFACE_FORMAT_R16_FLOAT):
        return VK_FORMAT_R16_SFLOAT;
    case(FFX_SURFACE_FORMAT_R16_UINT):
        return VK_FORMAT_R16_UINT;
    case(FFX_SURFACE_FORMAT_R16_UNORM):
        return VK_FORMAT_R16_UNORM;
    case(FFX_SURFACE_FORMAT_R16_SNORM):
        return VK_FORMAT_R16_SNORM;
    case(FFX_SURFACE_FORMAT_R8_UNORM):
        return VK_FORMAT_R8_UNORM;
    case(FFX_SURFACE_FORMAT_R8G8_UNORM):
        return VK_FORMAT_R8G8_UNORM;
    case(FFX_SURFACE_FORMAT_R8G8_UINT):
        return VK_FORMAT_R8G8_UINT;
    case(FFX_SURFACE_FORMAT_R32_FLOAT):
        return VK_FORMAT_R32_SFLOAT;

    default:
        FFX_ASSERT_MESSAGE(false, "Format not yet supported");
        return VK_FORMAT_UNDEFINED;
    }
}

FfxSurfaceFormat ffxGetSurfaceFormatVK(VkFormat fmt)
{
    switch (fmt)
    {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        return FFX_SURFACE_FORMAT_R32G32B32A32_FLOAT;
    case VK_FORMAT_R32G32B32A32_UINT:
        return FFX_SURFACE_FORMAT_R32G32B32A32_UINT;
    case VK_FORMAT_R16G16B16A16_SFLOAT:
        return FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    case VK_FORMAT_R32G32_SFLOAT:
        return FFX_SURFACE_FORMAT_R32G32_FLOAT;
    case VK_FORMAT_R32_UINT:
        return FFX_SURFACE_FORMAT_R32_UINT;
    case VK_FORMAT_R8G8B8A8_UNORM:
        return FFX_SURFACE_FORMAT_R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SNORM:
        return FFX_SURFACE_FORMAT_R8G8B8A8_SNORM;
    case VK_FORMAT_R8G8B8A8_SRGB:
        return FFX_SURFACE_FORMAT_R8G8B8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_UNORM:
        return FFX_SURFACE_FORMAT_B8G8R8A8_UNORM;
    case VK_FORMAT_B8G8R8A8_SRGB:
        return FFX_SURFACE_FORMAT_B8G8R8A8_SRGB;
    case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        return FFX_SURFACE_FORMAT_R11G11B10_FLOAT;
    case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        return FFX_SURFACE_FORMAT_R10G10B10A2_UNORM;
    case VK_FORMAT_R16G16_SFLOAT:
        return FFX_SURFACE_FORMAT_R16G16_FLOAT;
    case VK_FORMAT_R16G16_UINT:
        return FFX_SURFACE_FORMAT_R16G16_UINT;
    case VK_FORMAT_R16G16_SINT:
        return FFX_SURFACE_FORMAT_R16G16_SINT;
    case VK_FORMAT_R16_SFLOAT:
        return FFX_SURFACE_FORMAT_R16_FLOAT;
    case VK_FORMAT_R16_UINT:
        return FFX_SURFACE_FORMAT_R16_UINT;
    case VK_FORMAT_R16_UNORM:
        return FFX_SURFACE_FORMAT_R16_UNORM;
    case VK_FORMAT_R16_SNORM:
        return FFX_SURFACE_FORMAT_R16_SNORM;
    case VK_FORMAT_R8_UNORM:
        return FFX_SURFACE_FORMAT_R8_UNORM;
    case VK_FORMAT_R8_UINT:
        return FFX_SURFACE_FORMAT_R8_UINT;
    case VK_FORMAT_R8G8_UNORM:
        return FFX_SURFACE_FORMAT_R8G8_UNORM;
    case VK_FORMAT_R8G8_UINT:
        return FFX_SURFACE_FORMAT_R8G8_UINT;
    case VK_FORMAT_R32_SFLOAT:
        return FFX_SURFACE_FORMAT_R32_FLOAT;
    case VK_FORMAT_D32_SFLOAT:
        return FFX_SURFACE_FORMAT_R32_FLOAT;
    case VK_FORMAT_UNDEFINED:
        return FFX_SURFACE_FORMAT_UNKNOWN;

    default:
        // NOTE: we do not support typeless formats here
        FFX_ASSERT_MESSAGE(false, "Format not yet supported");
        return FFX_SURFACE_FORMAT_UNKNOWN;
    }
}

VkImageUsageFlags getVKImageUsageFlagsFromResourceUsage(FfxResourceUsage flags)
{
    VkImageUsageFlags ret = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (FFX_CONTAINS_FLAG(flags, FFX_RESOURCE_USAGE_RENDERTARGET)) ret |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (FFX_CONTAINS_FLAG(flags, FFX_RESOURCE_USAGE_UAV)) ret |= (VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    return ret;
}

FfxResourceDescription ffxGetBufferResourceDescriptionVK(const VkBuffer buffer, const VkBufferCreateInfo createInfo, FfxResourceUsage additionalUsages /*=FFX_RESOURCE_USAGE_READ_ONLY*/)
{
    FfxResourceDescription resourceDescription = {};

    // This is valid
    if (buffer == VK_NULL_HANDLE)
        return resourceDescription;

    resourceDescription.flags  = FFX_RESOURCE_FLAGS_NONE;
    resourceDescription.usage  = additionalUsages;
    resourceDescription.size   = (uint32_t)createInfo.size;
    resourceDescription.stride = 0;
    resourceDescription.format = FFX_SURFACE_FORMAT_UNKNOWN;

    if ((createInfo.usage & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) != 0)
        resourceDescription.usage = (FfxResourceUsage)(resourceDescription.usage | FFX_RESOURCE_USAGE_UAV);
    if ((createInfo.usage & VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT) != 0)
        resourceDescription.usage = (FfxResourceUsage)(resourceDescription.usage | FFX_RESOURCE_USAGE_INDIRECT);

    // What should we initialize this to?? No case for this yet
    resourceDescription.depth    = 0;
    resourceDescription.mipCount = 0;

    // Set the type
    resourceDescription.type = FFX_RESOURCE_TYPE_BUFFER;
    
    return resourceDescription;
}

FfxResourceDescription ffxGetImageResourceDescriptionVK(const VkImage           image,
                                                        const VkImageCreateInfo createInfo,
                                                        FfxResourceUsage        additionalUsages /*=FFX_RESOURCE_USAGE_READ_ONLY*/)
{
    FfxResourceDescription resourceDescription = {};

    // This is valid
    if (image == VK_NULL_HANDLE)
        return resourceDescription;

    // Set flags properly for resource registration
    resourceDescription.flags = FFX_RESOURCE_FLAGS_NONE;

    // Check for depth use
    if ((createInfo.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        resourceDescription.usage = FFX_RESOURCE_USAGE_DEPTHTARGET;
    else
        resourceDescription.usage = FFX_RESOURCE_USAGE_READ_ONLY;
    
    // Unordered access use
    if ((createInfo.usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0)
        resourceDescription.usage = (FfxResourceUsage)(resourceDescription.usage | FFX_RESOURCE_USAGE_UAV);

    // Resource-specific supplemental use flags
    resourceDescription.usage = (FfxResourceUsage)(resourceDescription.usage | additionalUsages);

    resourceDescription.width    = createInfo.extent.width;
    resourceDescription.height   = createInfo.extent.height;
    resourceDescription.mipCount = createInfo.mipLevels;
    resourceDescription.format   = ffxGetSurfaceFormatVK(createInfo.format);

    // if the mutable flag is present, assume that the real format is sRGB
    if (FFX_CONTAINS_FLAG(createInfo.flags, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT))
        resourceDescription.format = ffxGetSurfaceFormatToGamma(resourceDescription.format);
    
    switch (createInfo.imageType)
    {
    case VK_IMAGE_TYPE_1D:
        resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE1D;
        break;
    case VK_IMAGE_TYPE_2D:
        resourceDescription.depth = createInfo.arrayLayers;
        if (FFX_CONTAINS_FLAG(additionalUsages, FFX_RESOURCE_USAGE_ARRAYVIEW))
            resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE2D;
        else if ((createInfo.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) != 0)
            resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE_CUBE;
        else
            resourceDescription.type  = FFX_RESOURCE_TYPE_TEXTURE2D;
        break;
    case VK_IMAGE_TYPE_3D:
        resourceDescription.depth = createInfo.extent.depth;
        resourceDescription.type = FFX_RESOURCE_TYPE_TEXTURE3D;
        break;
    default:
        FFX_ASSERT_MESSAGE(false, "FFXInterface: VK: Unsupported texture dimension requested. Please implement.");
        break;
    }

    return resourceDescription;
}

FfxErrorCode allocateDeviceMemory(BackendContext_VK* backendContext, VkMemoryRequirements memRequirements, VkMemoryPropertyFlags requiredMemoryProperties, BackendContext_VK::Resource* backendResource)
{
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryTypeIndex(backendContext->physicalDevice, memRequirements, requiredMemoryProperties, backendResource->memoryProperties);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        return FFX_ERROR_BACKEND_API_ERROR;
    }

    VkResult result = backendContext->vkFunctionTable.vkAllocateMemory(backendContext->device, &allocInfo, nullptr, &backendResource->deviceMemory);

    if (result != VK_SUCCESS) {
        switch (result) {
        case(VK_ERROR_OUT_OF_HOST_MEMORY):
        case(VK_ERROR_OUT_OF_DEVICE_MEMORY):
            return FFX_ERROR_OUT_OF_MEMORY;
        default:
            return FFX_ERROR_BACKEND_API_ERROR;
        }
    }

    return FFX_OK;
}

void setVKObjectName(BackendContext_VK::VKFunctionTable& vkFunctionTable, VkDevice device, VkObjectType objectType, uint64_t object, char* name)
{
    VkDebugUtilsObjectNameInfoEXT s{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr, objectType, object, name };

    if (vkFunctionTable.vkSetDebugUtilsObjectNameEXT)
        vkFunctionTable.vkSetDebugUtilsObjectNameEXT(device, &s);
}

uint32_t getDynamicResourcesStartIndex(uint32_t effectContextId)
{
    // dynamic resources are tracked from the max index
    return (effectContextId * FFX_MAX_RESOURCE_COUNT) + FFX_MAX_RESOURCE_COUNT - 1;
}
uint32_t getDynamicResourceViewsStartIndex(uint32_t effectContextId, uint32_t frameIndex)
{
    // dynamic resource views are tracked from the max index
    return (effectContextId * FFX_MAX_QUEUED_FRAMES * FFX_MAX_RESOURCE_COUNT * 2) + (frameIndex * FFX_MAX_RESOURCE_COUNT * 2) + (FFX_MAX_RESOURCE_COUNT * 2) - 1;
}

void destroyDynamicViews(BackendContext_VK* backendContext, uint32_t effectContextId, uint32_t frameIndex)
{
    BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];

    // Release image views for dynamic resources
    const uint32_t dynamicResourceViewIndexStart = getDynamicResourceViewsStartIndex(effectContextId, frameIndex);
    for (uint32_t dynamicViewIndex = effectContext.nextDynamicResourceView[frameIndex] + 1; dynamicViewIndex <= dynamicResourceViewIndexStart;
         ++dynamicViewIndex)
    {
        backendContext->vkFunctionTable.vkDestroyImageView(backendContext->device, backendContext->pResourceViews[dynamicViewIndex].imageView, VK_NULL_HANDLE);
        backendContext->pResourceViews[dynamicViewIndex].imageView = VK_NULL_HANDLE;
    }
    effectContext.nextDynamicResourceView[frameIndex] = dynamicResourceViewIndexStart;
}

VkAccessFlags getVKAccessFlagsFromResourceState(FfxResourceStates state)
{
    switch (state) {

    case(FFX_RESOURCE_STATE_COMMON):
        return VK_ACCESS_NONE;
    case(FFX_RESOURCE_STATE_GENERIC_READ):
        return VK_ACCESS_SHADER_READ_BIT;
    case(FFX_RESOURCE_STATE_UNORDERED_ACCESS):
        return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case(FFX_RESOURCE_STATE_COMPUTE_READ):
    case(FFX_RESOURCE_STATE_PIXEL_READ):
    case(FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ):
        return VK_ACCESS_SHADER_READ_BIT;
    case FFX_RESOURCE_STATE_COPY_SRC:
        return VK_ACCESS_TRANSFER_READ_BIT;
    case FFX_RESOURCE_STATE_COPY_DEST:
        return VK_ACCESS_TRANSFER_WRITE_BIT;
    case FFX_RESOURCE_STATE_INDIRECT_ARGUMENT:
        return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    case FFX_RESOURCE_STATE_PRESENT:
        return VK_ACCESS_NONE;
    case FFX_RESOURCE_STATE_RENDER_TARGET:
        return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    default:
        FFX_ASSERT_MESSAGE(false, "State flag not yet supported");
        return VK_ACCESS_SHADER_READ_BIT;
    }
}

VkPipelineStageFlags getVKPipelineStageFlagsFromResourceState(FfxResourceStates state)
{
    switch (state) {

    case(FFX_RESOURCE_STATE_COMMON):
    case(FFX_RESOURCE_STATE_GENERIC_READ):
    case(FFX_RESOURCE_STATE_UNORDERED_ACCESS):
    case(FFX_RESOURCE_STATE_COMPUTE_READ):
    case(FFX_RESOURCE_STATE_PIXEL_READ):
    case(FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ):
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    case(FFX_RESOURCE_STATE_INDIRECT_ARGUMENT):
        return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    case FFX_RESOURCE_STATE_COPY_SRC:
    case FFX_RESOURCE_STATE_COPY_DEST:
        return VK_PIPELINE_STAGE_TRANSFER_BIT;
    case FFX_RESOURCE_STATE_PRESENT:
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    case FFX_RESOURCE_STATE_RENDER_TARGET:
        return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    default:
        FFX_ASSERT_MESSAGE(false, "Pipeline stage flag not yet supported");
        return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
}

VkImageLayout getVKImageLayoutFromResourceState(FfxResourceStates state)
{
    switch (state) {

    case(FFX_RESOURCE_STATE_COMMON):
        return VK_IMAGE_LAYOUT_GENERAL;
    case(FFX_RESOURCE_STATE_GENERIC_READ):
        return VK_IMAGE_LAYOUT_GENERAL;
    case(FFX_RESOURCE_STATE_UNORDERED_ACCESS):
        return VK_IMAGE_LAYOUT_GENERAL;
    case(FFX_RESOURCE_STATE_COMPUTE_READ):
    case(FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ):
    case(FFX_RESOURCE_STATE_PIXEL_READ):
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case FFX_RESOURCE_STATE_COPY_SRC:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case FFX_RESOURCE_STATE_COPY_DEST:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case FFX_RESOURCE_STATE_PRESENT:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case FFX_RESOURCE_STATE_RENDER_TARGET:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case FFX_RESOURCE_STATE_INDIRECT_ARGUMENT:
        // this case is for buffers
    default:
        FFX_ASSERT_MESSAGE(false, "Image layout flag not yet supported");
        return VK_IMAGE_LAYOUT_GENERAL;
    }
}

void addMutableViewForSRV(VkImageViewCreateInfo& imageViewCreateInfo, VkImageViewUsageCreateInfo& imageViewUsageCreateInfo, FfxResourceDescription resourceDescription)
{
    if (ffxIsSurfaceFormatSRGB(resourceDescription.format) && FFX_CONTAINS_FLAG(resourceDescription.usage, FFX_RESOURCE_USAGE_UAV))
    {
        // mutable is only for sRGB textures that will need a storage
        imageViewUsageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO;
        imageViewUsageCreateInfo.pNext = nullptr;
        imageViewUsageCreateInfo.usage = getVKImageUsageFlagsFromResourceUsage(FFX_RESOURCE_USAGE_READ_ONLY);  // we can assume that SRV is enough
        imageViewCreateInfo.pNext      = &imageViewUsageCreateInfo;
    }
}

void copyResourceState(BackendContext_VK::Resource* backendResource, const FfxResource* inFfxResource)
{
    FfxResourceStates state = inFfxResource->state;

    // copy the new states
    backendResource->initialState = state;
    backendResource->currentState = state;
    backendResource->undefined    = false;
    backendResource->dynamic      = true;

    // If the internal resource state is undefined, that means we are importing a resource that
    // has not yet been initialized, so tag the resource as undefined so we can transition it accordingly.
    if (backendResource->resourceDescription.flags & FFX_RESOURCE_FLAGS_UNDEFINED)
    {
        backendResource->undefined                 = true;
        backendResource->resourceDescription.flags = (FfxResourceFlags)((int)backendResource->resourceDescription.flags & ~FFX_RESOURCE_FLAGS_UNDEFINED);
    }
}

#ifdef _WIN32
void ConvertUTF8ToUTF16(const char* inputName, wchar_t* outputBuffer, size_t outputLen)
{
    if (MultiByteToWideChar(CP_UTF8, 0, inputName, -1, outputBuffer, static_cast<int>(outputLen)) == 0)
    {
        memset(outputBuffer, 0, outputLen * sizeof(wchar_t));
    }
}
void ConvertUTF16ToUTF8(const wchar_t* inputName, char* outputBuffer, size_t outputLen)
{
    if (WideCharToMultiByte(CP_UTF8, 0, inputName, -1, outputBuffer, static_cast<int>(outputLen * sizeof(char)), NULL, NULL) == 0)
    {
        memset(outputBuffer, 0, outputLen * sizeof(char));
    }
}
#else
void ConvertUTF8ToUTF16(const char* inputName, wchar_t* outputBuffer, size_t outputLen)
{
    memset(outputBuffer, 0, outputLen * sizeof(wchar_t));
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    wcscpy_s(outputBuffer, outputLen, converter.from_bytes(inputName).c_str());
}
void ConvertUTF16ToUTF8(const wchar_t* inputName, char* outputBuffer, size_t outputLen)
{
    memset(outputBuffer, 0, outputLen * sizeof(char));
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    strcpy_s(outputBuffer, outputLen, converter.to_bytes(inputName).c_str());
}
#endif  // _WIN32

void beginMarkerVK(BackendContext_VK* backendContext, VkCommandBuffer commandBuffer, const wchar_t* label)
{
    if (!backendContext->vkFunctionTable.vkCmdBeginDebugUtilsLabelEXT || !backendContext->vkFunctionTable.vkCmdEndDebugUtilsLabelEXT)
        return;

    constexpr size_t strLen = 64;
    char strLabel[strLen];
    ConvertUTF16ToUTF8(label, strLabel, strLen);

    VkDebugUtilsLabelEXT debugLabel = {};
    debugLabel.sType                = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    debugLabel.pNext                = nullptr;
    debugLabel.pLabelName           = strLabel;

    // not to saturated red
    debugLabel.color[0] = 1.0f;
    debugLabel.color[1] = 0.14f;
    debugLabel.color[2] = 0.14f;
    debugLabel.color[3] = 1.0f;
    backendContext->vkFunctionTable.vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
}

void endMarkerVK(BackendContext_VK* backendContext, VkCommandBuffer commandBuffer)
{
    if (!backendContext->vkFunctionTable.vkCmdBeginDebugUtilsLabelEXT || !backendContext->vkFunctionTable.vkCmdEndDebugUtilsLabelEXT)
        return;

    backendContext->vkFunctionTable.vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}

void addBarrier(BackendContext_VK* backendContext, FfxResourceInternal* resource, FfxResourceStates newState)
{
    FFX_ASSERT(NULL != backendContext);
    FFX_ASSERT(NULL != resource);

    BackendContext_VK::Resource& ffxResource = backendContext->pResources[resource->internalIndex];

    if (ffxResource.resourceDescription.type == FFX_RESOURCE_TYPE_BUFFER)
    {
        VkBuffer vkResource = ffxResource.bufferResource;
        VkBufferMemoryBarrier* barrier = &backendContext->bufferMemoryBarriers[backendContext->scheduledBufferBarrierCount];

        FfxResourceStates& curState = backendContext->pResources[resource->internalIndex].currentState;

        barrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier->pNext = nullptr;
        barrier->srcAccessMask = getVKAccessFlagsFromResourceState(curState);
        barrier->dstAccessMask = getVKAccessFlagsFromResourceState(newState);
        barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->buffer = vkResource;
        barrier->offset = 0;
        barrier->size = VK_WHOLE_SIZE;

        backendContext->srcStageMask |= getVKPipelineStageFlagsFromResourceState(curState);
        backendContext->dstStageMask |= getVKPipelineStageFlagsFromResourceState(newState);

        curState = newState;

        ++backendContext->scheduledBufferBarrierCount;
    }
    else
    {
        VkImage vkResource = ffxResource.imageResource;
        VkImageMemoryBarrier* barrier = &backendContext->imageMemoryBarriers[backendContext->scheduledImageBarrierCount];

        FfxResourceStates& curState = backendContext->pResources[resource->internalIndex].currentState;

        VkImageSubresourceRange range;
        range.aspectMask = FFX_CONTAINS_FLAG(ffxResource.resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer = 0;
        range.layerCount = VK_REMAINING_ARRAY_LAYERS;

        barrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier->pNext = nullptr;
        barrier->srcAccessMask = getVKAccessFlagsFromResourceState(curState);
        barrier->dstAccessMask = getVKAccessFlagsFromResourceState(newState);
        barrier->oldLayout = ffxResource.undefined ? VK_IMAGE_LAYOUT_UNDEFINED : getVKImageLayoutFromResourceState(curState);
        barrier->newLayout = getVKImageLayoutFromResourceState(newState);
        barrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier->image = vkResource;
        barrier->subresourceRange = range;

        backendContext->srcStageMask |= getVKPipelineStageFlagsFromResourceState(curState);
        backendContext->dstStageMask |= getVKPipelineStageFlagsFromResourceState(newState);

        curState = newState;

        ++backendContext->scheduledImageBarrierCount;
    }

    if (ffxResource.undefined)
        ffxResource.undefined = false;
}

void flushBarriers(BackendContext_VK* backendContext, VkCommandBuffer vkCommandBuffer)
{
    FFX_ASSERT(NULL != backendContext);
    FFX_ASSERT(NULL != vkCommandBuffer);

    if (backendContext->scheduledImageBarrierCount > 0 || backendContext->scheduledBufferBarrierCount > 0)
    {
        backendContext->vkFunctionTable.vkCmdPipelineBarrier(vkCommandBuffer, backendContext->srcStageMask, backendContext->dstStageMask, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, backendContext->scheduledBufferBarrierCount, backendContext->bufferMemoryBarriers, backendContext->scheduledImageBarrierCount, backendContext->imageMemoryBarriers);
        backendContext->scheduledImageBarrierCount = 0;
        backendContext->scheduledBufferBarrierCount = 0;
        backendContext->srcStageMask = 0;
        backendContext->dstStageMask = 0;
    }
}

FfxConstantAllocation BackendContext_VK::FallbackConstantAllocator(void* data, FfxUInt64 dataSize)
{
    FfxConstantAllocation       allocation;
    std::lock_guard<std::mutex> cbLock{uniformBufferMutex};

    if (!uniformBufferMem)
    {
        // allocate dynamic uniform buffer

        // get alignment
        VkPhysicalDeviceProperties physicalDeviceProperties = {};
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
        uniformBufferAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;

        uniformBufferSize = FFX_ALIGN_UP(FFX_BUFFER_SIZE, uniformBufferAlignment) * maxEffectContexts * FFX_MAX_PASS_COUNT * FFX_MAX_QUEUED_FRAMES;

        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size               = static_cast<VkDeviceSize>(uniformBufferSize);
        bufferInfo.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

        VkResult res = vkFunctionTable.vkCreateBuffer(device, &bufferInfo, NULL, &uniformBuffer);
        FFX_ASSERT(res == VK_SUCCESS);

        VkMemoryAllocateInfo allocInfo = {};
        if (res == VK_SUCCESS)
        {
            // allocate memory block for all uniform buffers
            VkMemoryRequirements memRequirements = {};
            vkFunctionTable.vkGetBufferMemoryRequirements(device, uniformBuffer, &memRequirements);

            // this is the real alignment
            uniformBufferAlignment = memRequirements.alignment;

            VkMemoryPropertyFlags requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize  = uniformBufferSize;
            allocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memRequirements, requiredMemoryProperties, uniformBufferMemoryProperties);

            if (allocInfo.memoryTypeIndex == UINT32_MAX)
            {
                requiredMemoryProperties  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                allocInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memRequirements, requiredMemoryProperties, uniformBufferMemoryProperties);

                if (allocInfo.memoryTypeIndex == UINT32_MAX)
                    res = VK_ERROR_INITIALIZATION_FAILED;
            }
        }

        if (res == VK_SUCCESS)
            res = vkFunctionTable.vkAllocateMemory(device, &allocInfo, nullptr, &uniformBufferMemory);
        FFX_ASSERT(res == VK_SUCCESS);

        if (res == VK_SUCCESS)
            res = vkFunctionTable.vkBindBufferMemory(device, uniformBuffer, uniformBufferMemory, 0);
        FFX_ASSERT(res == VK_SUCCESS);

        // map the memory block
        if (res == VK_SUCCESS)
            res = vkFunctionTable.vkMapMemory(device, uniformBufferMemory, 0, uniformBufferSize, 0, &uniformBufferMem);
        FFX_ASSERT(res == VK_SUCCESS);
    }

    FFX_ASSERT(uniformBufferMem);

    allocation.resource.resource = uniformBuffer;
    allocation.handle            = 0;

    if (data)
    {
        if (uniformBufferOffset + dataSize >= uniformBufferSize)
            uniformBufferOffset = 0;

        allocation.handle = static_cast<FfxUInt64>(uniformBufferOffset);

        void* pBuffer = (void*)((uint8_t*)(uniformBufferMem) + uniformBufferOffset);
        memcpy(pBuffer, data, dataSize);

        // TODO: ensure that we aren't writing on some used memory

        // flush mapped range if memory type is not coherent
        if ((uniformBufferMemoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
        {
            VkMappedMemoryRange memoryRange;
            memset(&memoryRange, 0, sizeof(memoryRange));

            memoryRange.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            memoryRange.memory = uniformBufferMemory;
            memoryRange.offset = uniformBufferOffset;
            memoryRange.size   = dataSize;

            vkFunctionTable.vkFlushMappedMemoryRanges(device, 1, &memoryRange);
        }

        uniformBufferOffset += FFX_ALIGN_UP(dataSize, uniformBufferAlignment);
        if (uniformBufferOffset > uniformBufferSize)
            uniformBufferOffset = 0;
    }

    return allocation;
}

void resetBackendContext(BackendContext_VK* backendContext)
{
    // reset the context except the maxEffectContexts in case the memory is reused for a new context
    uint32_t maxEffectContexts = backendContext->maxEffectContexts;

    memset(backendContext, 0, sizeof(BackendContext_VK));

    // restore the maxEffectContexts
    backendContext->maxEffectContexts = maxEffectContexts;
}

//////////////////////////////////////////////////////////////////////////
// VK back end implementation

FfxVersionNumber GetSDKVersionVK(FfxInterface* backendInterface)
{
    return FFX_SDK_MAKE_VERSION(FFX_SDK_VERSION_MAJOR, FFX_SDK_VERSION_MINOR, FFX_SDK_VERSION_PATCH);
}

FfxErrorCode GetEffectGpuMemoryUsageVK(FfxInterface* backendInterface, FfxUInt32 effectContextId, FfxEffectMemoryUsage* outVramUsage)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != outVramUsage);

    *outVramUsage = {};

    return FFX_OK;
}

FfxErrorCode CreateBackendContextVK(FfxInterface* backendInterface, FfxEffectBindlessConfig* bindlessConfig, FfxUInt32* effectContextId)
{
    VkDeviceContext* vkDeviceContext = reinterpret_cast<VkDeviceContext*>(backendInterface->device);

    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != vkDeviceContext);
    FFX_ASSERT(VK_NULL_HANDLE != vkDeviceContext->vkDevice);
    FFX_ASSERT(VK_NULL_HANDLE != vkDeviceContext->vkPhysicalDevice);

    // set up some internal resources we need (space for resource views and constant buffers)
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    // Set things up if this is the first invocation
    if (!backendContext->refCount) {

        resetBackendContext(backendContext);

        new (&backendContext->uniformBufferMutex) std::mutex();

        // Map all of our pointers
        uint32_t gpuJobDescArraySize   = FFX_ALIGN_UP(backendContext->maxEffectContexts * FFX_MAX_GPU_JOBS * sizeof(FfxGpuJobDescription), sizeof(uint32_t));
        uint32_t resourceViewArraySize = FFX_ALIGN_UP(((backendContext->maxEffectContexts * FFX_MAX_QUEUED_FRAMES * FFX_MAX_RESOURCE_COUNT * 2) + FFX_MAX_BINDLESS_DESCRIPTOR_COUNT) * sizeof(BackendContext_VK::VkResourceView), sizeof(uint32_t));
        uint32_t stagingRingBufferArraySize = FFX_ALIGN_UP(backendContext->maxEffectContexts * FFX_CONSTANT_BUFFER_RING_BUFFER_SIZE, sizeof(uint32_t));
        uint32_t pipelineArraySize = FFX_ALIGN_UP(backendContext->maxEffectContexts * FFX_MAX_PASS_COUNT * sizeof(BackendContext_VK::PipelineLayout), sizeof(uint32_t));
        uint32_t resourceArraySize = FFX_ALIGN_UP(backendContext->maxEffectContexts * FFX_MAX_RESOURCE_COUNT * sizeof(BackendContext_VK::Resource), sizeof(uint32_t));
        uint32_t contextArraySize = FFX_ALIGN_UP(backendContext->maxEffectContexts * sizeof(BackendContext_VK::EffectContext), sizeof(uint32_t));
        uint8_t* pMem = (uint8_t*)((BackendContext_VK*)(backendContext + 1));

        // Map gpu job array
        backendContext->pGpuJobs = (FfxGpuJobDescription*)pMem;
        memset(backendContext->pGpuJobs, 0, gpuJobDescArraySize);
        pMem += gpuJobDescArraySize;

        // Map the resource view array
        backendContext->pResourceViews = (BackendContext_VK::VkResourceView*)(pMem);
        memset(backendContext->pResourceViews, 0, resourceViewArraySize);
        pMem += resourceViewArraySize;

        // Map the staging ring buffer array
        backendContext->pStagingRingBuffer = (uint8_t*)pMem;
        memset(backendContext->pStagingRingBuffer, 0, stagingRingBufferArraySize);
        pMem += stagingRingBufferArraySize;

        // Map pipeline array
        backendContext->pPipelineLayouts = (BackendContext_VK::PipelineLayout*)pMem;
        memset(backendContext->pPipelineLayouts, 0, pipelineArraySize);
        pMem += pipelineArraySize;

        // Map resource array
        backendContext->pResources = (BackendContext_VK::Resource*)pMem;
        memset(backendContext->pResources, 0, resourceArraySize);
        pMem += resourceArraySize;

        // Clear out all resource mappings
        for (uint32_t i = 0; i < backendContext->maxEffectContexts * FFX_MAX_RESOURCE_COUNT; ++i) {
            backendContext->pResources[i].uavViewIndex = backendContext->pResources[i].srvViewIndex = -1;
        }

        // Map context array
        backendContext->pEffectContexts = (BackendContext_VK::EffectContext*)pMem;
        memset(backendContext->pEffectContexts, 0, contextArraySize);
        pMem += contextArraySize;

        // Map extension array
        backendContext->extensionProperties = (VkExtensionProperties*)pMem;

        // if vkGetDeviceProcAddr is NULL, use the one from the vulkan header
        if (vkDeviceContext->vkDeviceProcAddr == NULL)
            vkDeviceContext->vkDeviceProcAddr = vkGetDeviceProcAddr;

        if (vkDeviceContext->vkDevice != VK_NULL_HANDLE) {
            backendContext->device = vkDeviceContext->vkDevice;
        }

        if (vkDeviceContext->vkPhysicalDevice != VK_NULL_HANDLE) {
            backendContext->physicalDevice = vkDeviceContext->vkPhysicalDevice;
        }

        // load vulkan functions
        backendContext->vkFunctionTable.vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkSetDebugUtilsObjectNameEXT");
        backendContext->vkFunctionTable.vkFlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkFlushMappedMemoryRanges");
        backendContext->vkFunctionTable.vkCreateDescriptorPool = (PFN_vkCreateDescriptorPool)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateDescriptorPool");
        backendContext->vkFunctionTable.vkCreateSampler = (PFN_vkCreateSampler)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateSampler");
        backendContext->vkFunctionTable.vkCreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateDescriptorSetLayout");
        backendContext->vkFunctionTable.vkCreateBuffer = (PFN_vkCreateBuffer)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateBuffer");
        backendContext->vkFunctionTable.vkCreateBufferView = (PFN_vkCreateBufferView)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateBufferView");
        backendContext->vkFunctionTable.vkCreateImage = (PFN_vkCreateImage)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateImage");
        backendContext->vkFunctionTable.vkCreateImageView = (PFN_vkCreateImageView)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateImageView");
        backendContext->vkFunctionTable.vkCreateShaderModule = (PFN_vkCreateShaderModule)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateShaderModule");
        backendContext->vkFunctionTable.vkCreatePipelineLayout = (PFN_vkCreatePipelineLayout)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreatePipelineLayout");
        backendContext->vkFunctionTable.vkCreateComputePipelines = (PFN_vkCreateComputePipelines)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCreateComputePipelines");
        backendContext->vkFunctionTable.vkDestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyPipelineLayout");
        backendContext->vkFunctionTable.vkDestroyPipeline = (PFN_vkDestroyPipeline)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyPipeline");
        backendContext->vkFunctionTable.vkDestroyImage = (PFN_vkDestroyImage)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyImage");
        backendContext->vkFunctionTable.vkDestroyImageView = (PFN_vkDestroyImageView)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyImageView");
        backendContext->vkFunctionTable.vkDestroyBuffer = (PFN_vkDestroyBuffer)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyBuffer");
        backendContext->vkFunctionTable.vkDestroyBufferView = (PFN_vkDestroyBufferView)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyBufferView");
        backendContext->vkFunctionTable.vkDestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyDescriptorSetLayout");
        backendContext->vkFunctionTable.vkDestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyDescriptorPool");
        backendContext->vkFunctionTable.vkDestroySampler = (PFN_vkDestroySampler)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroySampler");
        backendContext->vkFunctionTable.vkDestroyShaderModule = (PFN_vkDestroyShaderModule)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkDestroyShaderModule");
        backendContext->vkFunctionTable.vkGetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkGetBufferMemoryRequirements");
        backendContext->vkFunctionTable.vkGetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2KHR)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkGetBufferMemoryRequirements2KHR");
        if (!backendContext->vkFunctionTable.vkGetBufferMemoryRequirements2KHR) backendContext->vkFunctionTable.vkGetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2KHR)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkGetBufferMemoryRequirements2");
        backendContext->vkFunctionTable.vkGetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkGetImageMemoryRequirements");
        backendContext->vkFunctionTable.vkAllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkAllocateDescriptorSets");
        backendContext->vkFunctionTable.vkFreeDescriptorSets = (PFN_vkFreeDescriptorSets)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkFreeDescriptorSets");
        backendContext->vkFunctionTable.vkAllocateMemory = (PFN_vkAllocateMemory)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkAllocateMemory");
        backendContext->vkFunctionTable.vkFreeMemory = (PFN_vkFreeMemory)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkFreeMemory");
        backendContext->vkFunctionTable.vkMapMemory = (PFN_vkMapMemory)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkMapMemory");
        backendContext->vkFunctionTable.vkUnmapMemory = (PFN_vkUnmapMemory)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkUnmapMemory");
        backendContext->vkFunctionTable.vkBindBufferMemory = (PFN_vkBindBufferMemory)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkBindBufferMemory");
        backendContext->vkFunctionTable.vkBindImageMemory = (PFN_vkBindImageMemory)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkBindImageMemory");
        backendContext->vkFunctionTable.vkUpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkUpdateDescriptorSets");
        backendContext->vkFunctionTable.vkCmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdPipelineBarrier");
        backendContext->vkFunctionTable.vkCmdBindPipeline = (PFN_vkCmdBindPipeline)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdBindPipeline");
        backendContext->vkFunctionTable.vkCmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdBindDescriptorSets");
        backendContext->vkFunctionTable.vkCmdDispatch = (PFN_vkCmdDispatch)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdDispatch");
        backendContext->vkFunctionTable.vkCmdDispatchIndirect = (PFN_vkCmdDispatchIndirect)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdDispatchIndirect");
        backendContext->vkFunctionTable.vkCmdCopyBuffer = (PFN_vkCmdCopyBuffer)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdCopyBuffer");
        backendContext->vkFunctionTable.vkCmdCopyImage = (PFN_vkCmdCopyImage)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdCopyImage");
        backendContext->vkFunctionTable.vkCmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdCopyBufferToImage");
        backendContext->vkFunctionTable.vkCmdClearColorImage = (PFN_vkCmdClearColorImage)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdClearColorImage");
        backendContext->vkFunctionTable.vkCmdFillBuffer = (PFN_vkCmdFillBuffer)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdFillBuffer");
        backendContext->vkFunctionTable.vkCmdWriteBufferMarkerAMD = (PFN_vkCmdWriteBufferMarkerAMD)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdWriteBufferMarkerAMD");
        backendContext->vkFunctionTable.vkCmdWriteBufferMarker2AMD = (PFN_vkCmdWriteBufferMarker2AMD)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdWriteBufferMarker2AMD");
        backendContext->vkFunctionTable.vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdBeginDebugUtilsLabelEXT");
        backendContext->vkFunctionTable.vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkDeviceContext->vkDeviceProcAddr(backendContext->device, "vkCmdEndDebugUtilsLabelEXT");

        // enumerate all the device extensions
        backendContext->numDeviceExtensions = 0;
        vkEnumerateDeviceExtensionProperties(backendContext->physicalDevice, nullptr, &backendContext->numDeviceExtensions, nullptr);
        vkEnumerateDeviceExtensionProperties(backendContext->physicalDevice, nullptr, &backendContext->numDeviceExtensions, backendContext->extensionProperties);

        // create a global descriptor pool to hold all descriptors we'll need
        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
        VkDescriptorPoolSize poolSizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, backendContext->maxEffectContexts * FFX_MAX_RESOURCE_COUNT * FFX_MAX_PASS_COUNT * FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, backendContext->maxEffectContexts * FFX_MAX_RESOURCE_COUNT * FFX_MAX_PASS_COUNT * FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, backendContext->maxEffectContexts * FFX_MAX_RESOURCE_COUNT * FFX_MAX_PASS_COUNT * FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME },
            { VK_DESCRIPTOR_TYPE_SAMPLER, backendContext->maxEffectContexts * FFX_MAX_RESOURCE_COUNT * FFX_MAX_PASS_COUNT * FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, backendContext->maxEffectContexts * FFX_MAX_RESOURCE_COUNT * FFX_MAX_PASS_COUNT * FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, backendContext->maxEffectContexts * FFX_MAX_RESOURCE_COUNT * FFX_MAX_PASS_COUNT * FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME },
        };

        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolCreateInfo.pNext = nullptr;
        descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descriptorPoolCreateInfo.poolSizeCount = 5;
        descriptorPoolCreateInfo.pPoolSizes = poolSizes;
        descriptorPoolCreateInfo.maxSets = backendContext->maxEffectContexts * FFX_MAX_PASS_COUNT * MAX_PIPELINE_USAGE_PER_FRAME * FFX_MAX_QUEUED_FRAMES;

        if (backendContext->vkFunctionTable.vkCreateDescriptorPool(backendContext->device, &descriptorPoolCreateInfo, nullptr, &backendContext->descriptorPool) != VK_SUCCESS) {
            return FFX_ERROR_BACKEND_API_ERROR;
        }

        // set bindless resource view to base
        backendContext->bindlessBase = (backendContext->maxEffectContexts * FFX_MAX_QUEUED_FRAMES * FFX_MAX_RESOURCE_COUNT * 2);

        // allocate dynamic uniform buffer
        {
            // get alignment
            VkPhysicalDeviceProperties physicalDeviceProperties = {};
            vkGetPhysicalDeviceProperties(backendContext->physicalDevice, &physicalDeviceProperties);
            backendContext->uniformBufferAlignment = physicalDeviceProperties.limits.minUniformBufferOffsetAlignment;

            backendContext->uniformBufferSize =
                FFX_ALIGN_UP(FFX_BUFFER_SIZE, backendContext->uniformBufferAlignment) * backendContext->maxEffectContexts * FFX_MAX_PASS_COUNT * FFX_MAX_QUEUED_FRAMES;

            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size               = static_cast<VkDeviceSize>(backendContext->uniformBufferSize);
            bufferInfo.usage              = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferInfo.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;

            if (backendContext->vkFunctionTable.vkCreateBuffer(backendContext->device, &bufferInfo, NULL, &backendContext->uniformBuffer) != VK_SUCCESS)
            {
                return FFX_ERROR_BACKEND_API_ERROR;
            }

            // allocate memory block for all uniform buffers
            VkMemoryRequirements memRequirements = {};
            backendContext->vkFunctionTable.vkGetBufferMemoryRequirements(backendContext->device, backendContext->uniformBuffer, &memRequirements);

            // this is the real alignment
            backendContext->uniformBufferAlignment = memRequirements.alignment;

            VkMemoryPropertyFlags requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType          = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize = backendContext->uniformBufferSize;
            allocInfo.memoryTypeIndex =
                findMemoryTypeIndex(backendContext->physicalDevice, memRequirements, requiredMemoryProperties, backendContext->uniformBufferMemoryProperties);

            if (allocInfo.memoryTypeIndex == UINT32_MAX)
            {
                requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
                allocInfo.memoryTypeIndex = findMemoryTypeIndex(
                    backendContext->physicalDevice, memRequirements, requiredMemoryProperties, backendContext->uniformBufferMemoryProperties);

                if (allocInfo.memoryTypeIndex == UINT32_MAX)
                {
                    return FFX_ERROR_BACKEND_API_ERROR;
                }
            }

            VkResult result = backendContext->vkFunctionTable.vkAllocateMemory(backendContext->device, &allocInfo, nullptr, &backendContext->uniformBufferMemory);

            if (result != VK_SUCCESS)
            {
                switch (result)
                {
                case (VK_ERROR_OUT_OF_HOST_MEMORY):
                case (VK_ERROR_OUT_OF_DEVICE_MEMORY):
                    return FFX_ERROR_OUT_OF_MEMORY;
                default:
                    return FFX_ERROR_BACKEND_API_ERROR;
                }
            }

            // map the memory block
            if (backendContext->vkFunctionTable.vkMapMemory(
                    backendContext->device, backendContext->uniformBufferMemory, 0, backendContext->uniformBufferSize, 0, &backendContext->uniformBufferMem) !=
                VK_SUCCESS)
            {
                return FFX_ERROR_BACKEND_API_ERROR;
            }

            if (backendContext->vkFunctionTable.vkBindBufferMemory(
                    backendContext->device, backendContext->uniformBuffer, backendContext->uniformBufferMemory, 0) != VK_SUCCESS)
            {
                return FFX_ERROR_BACKEND_API_ERROR;
            }
        }

        // Setup Breadcrumbs data
        {
            FfxDeviceCapabilities devCaps = {};
            if (GetDeviceCapabilitiesVK(backendInterface, &devCaps) != FFX_OK)
                return FFX_ERROR_BACKEND_API_ERROR;

            // Get info for memory used as Breadcrumbs buffer
            VkBufferCreateInfo bufferInfo = {};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.pNext = nullptr;
            bufferInfo.flags = 0;
            bufferInfo.size = 256;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            bufferInfo.queueFamilyIndexCount = 0;
            bufferInfo.pQueueFamilyIndices = nullptr;

            VkBuffer testBuffer = VK_NULL_HANDLE;
            if (vkCreateBuffer(backendContext->device, &bufferInfo, nullptr, &testBuffer) != VK_SUCCESS)
            {
                FFX_ASSERT_FAIL("Cannot create test Breadcrumbs buffer to find memory requirements!");
                return FFX_ERROR_BACKEND_API_ERROR;
            }

            uint32_t memoryTypeBits = 0;
            /// Enable usage of dedicated memory for Breadcrumbs buffers only when is required by the implementation
            if (devCaps.dedicatedAllocationSupported)
            {
                // Decide whether use dedicated memory or not
                VkBufferMemoryRequirementsInfo2 bufferReq = {};
                bufferReq.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
                bufferReq.pNext = nullptr;
                bufferReq.buffer = testBuffer;

                VkMemoryDedicatedRequirements dedicatedMemoryReq = {};
                dedicatedMemoryReq.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;
                dedicatedMemoryReq.pNext = nullptr;
                dedicatedMemoryReq.requiresDedicatedAllocation = VK_FALSE;

                VkMemoryRequirements2 memoryReq2 = {};
                memoryReq2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
                memoryReq2.pNext = &dedicatedMemoryReq;

                backendContext->vkFunctionTable.vkGetBufferMemoryRequirements2KHR(backendContext->device, &bufferReq, &memoryReq2);
                if (dedicatedMemoryReq.requiresDedicatedAllocation)
                    backendContext->breadcrumbsFlags |= BackendContext_VK::BREADCRUMBS_DEDICATED_MEMORY_ENABLED;
                memoryTypeBits = memoryReq2.memoryRequirements.memoryTypeBits;
            }
            else
            {
                VkMemoryRequirements memoryReq = {};
                backendContext->vkFunctionTable.vkGetBufferMemoryRequirements(backendContext->device, testBuffer, &memoryReq);
                memoryTypeBits = memoryReq.memoryTypeBits;
            }
            backendContext->vkFunctionTable.vkDestroyBuffer(backendContext->device, testBuffer, nullptr);

            // Find proper memory index for created buffers
            VkPhysicalDeviceMemoryProperties memoryProps = {};
            vkGetPhysicalDeviceMemoryProperties(backendContext->physicalDevice, &memoryProps);

            const VkMemoryPropertyFlags requiredMemoryFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            VkMemoryPropertyFlags preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            // When choosing between HOST_CACHED and AMD specific memory, AMD will take precedence as better guarantee of visible writes
            if (devCaps.deviceCoherentMemorySupported)
                preferredFlags |= VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;

            backendContext->breadcrumbsMemoryIndex = UINT32_MAX;
            uint32_t memoryCost = UINT32_MAX;
            for (uint32_t i = 0, memoryBit = 1; i < memoryProps.memoryTypeCount; ++i, memoryBit <<= 1)
            {
                if (memoryTypeBits & memoryBit)
                {
                    const VkMemoryPropertyFlags memFlags = memoryProps.memoryTypes[i].propertyFlags;
                    if ((memFlags & requiredMemoryFlags) == requiredMemoryFlags)
                    {
                        const uint32_t cost = ffxCountBitsSet(preferredFlags & ~memFlags);
                        if (cost < memoryCost)
                        {
                            backendContext->breadcrumbsMemoryIndex = i;
                            if (cost == 0)
                                break;
                            memoryCost = cost;
                        }
                    }
                }
            }

            if (backendContext->breadcrumbsMemoryIndex == UINT32_MAX)
            {
                FFX_ASSERT_FAIL("No memory that satisfies requirements requested by Breadcrumbs buffer type!");
                return FFX_ERROR_BACKEND_API_ERROR;
            }

            // Will switch to use vkCmdWriteBufferMarkerAMD() to write breadcrumbs into the buffer instead of vkCmdFillBuffer() for ensuring proper ordering of writes
            if (devCaps.bufferMarkerSupported)
                backendContext->breadcrumbsFlags |= BackendContext_VK::BREADCRUMBS_BUFFER_MARKER_ENABLED;

            // Together with BREADCRUMBS_BUFFER_MARKER_ENABLED flag will switch to vkCmdWriteBufferMarker2AMD() to use new synchronization facilities
            if (devCaps.extendedSynchronizationSupported)
                backendContext->breadcrumbsFlags |= BackendContext_VK::BREADCRUMBS_SYNCHRONIZATION2_ENABLED;
        }
    }

    // Increment the ref count
    ++backendContext->refCount;

    // Get an available context id
    for (uint32_t i = 0; i < backendContext->maxEffectContexts; ++i) {
        if (!backendContext->pEffectContexts[i].active) {
            *effectContextId = i;

            // Reset everything accordingly
            BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[i];
            effectContext.active = true;
            effectContext.nextStaticResource = (i * FFX_MAX_RESOURCE_COUNT) + 1;
            effectContext.nextDynamicResource = getDynamicResourcesStartIndex(i);
            effectContext.nextStaticResourceView = (i * FFX_MAX_QUEUED_FRAMES * FFX_MAX_RESOURCE_COUNT * 2);
            for (uint32_t frameIndex = 0; frameIndex < FFX_MAX_QUEUED_FRAMES; ++frameIndex)
            {
                effectContext.nextDynamicResourceView[frameIndex] = getDynamicResourceViewsStartIndex(i, frameIndex);
            }
            effectContext.nextPipelineLayout = (i * FFX_MAX_PASS_COUNT);
            effectContext.frameIndex = 0;

            if (bindlessConfig)
            {
                effectContext.bindlessTextureSrvHeapStart = backendContext->bindlessBase;
                effectContext.bindlessTextureSrvHeapSize  = bindlessConfig->maxTextureSrvs;

                backendContext->bindlessBase += bindlessConfig->maxTextureSrvs;

                effectContext.bindlessBufferSrvHeapSize  = bindlessConfig->maxBufferSrvs;

                effectContext.bindlessTextureUavHeapStart = backendContext->bindlessBase;
                effectContext.bindlessTextureUavHeapSize  = bindlessConfig->maxTextureUavs;

                backendContext->bindlessBase += bindlessConfig->maxTextureUavs;

                effectContext.bindlessBufferUavHeapSize  = bindlessConfig->maxBufferUavs;

                
                // create a bindless descriptor pool local to the current effect
                VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
                VkDescriptorPoolSize       poolSizes[3];

                uint32_t poolSizeCount = 0;

                if (bindlessConfig->maxTextureSrvs > 0)
                {
                    VkDescriptorPoolSize& poolSize = poolSizes[poolSizeCount++];

                    poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                    poolSize.descriptorCount = bindlessConfig->maxTextureSrvs;
                }

                if (bindlessConfig->maxTextureUavs > 0)
                {
                    VkDescriptorPoolSize& poolSize = poolSizes[poolSizeCount++];

                    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                    poolSize.descriptorCount = bindlessConfig->maxTextureUavs;
                }

                if (bindlessConfig->maxBufferSrvs > 0 || bindlessConfig->maxBufferUavs > 0)
                {
                    VkDescriptorPoolSize& poolSize = poolSizes[poolSizeCount++];

                    poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                    poolSize.descriptorCount = bindlessConfig->maxBufferSrvs + bindlessConfig->maxBufferUavs;
                }

                descriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
                descriptorPoolCreateInfo.pNext         = nullptr;
                descriptorPoolCreateInfo.flags         = 0;
                descriptorPoolCreateInfo.poolSizeCount = poolSizeCount;
                descriptorPoolCreateInfo.pPoolSizes    = poolSizes;
                descriptorPoolCreateInfo.maxSets       = poolSizeCount;

                if (backendContext->vkFunctionTable.vkCreateDescriptorPool(
                        backendContext->device, &descriptorPoolCreateInfo, nullptr, &effectContext.bindlessDescriptorPool) != VK_SUCCESS)
                {
                    return FFX_ERROR_BACKEND_API_ERROR;
                }

                // create the descriptor layout for bindless texture srv buffers
                if (bindlessConfig->maxTextureSrvs > 0)
                {
                    VkDescriptorSetLayoutBinding binding = {
                        0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, bindlessConfig->maxTextureSrvs, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

                    VkDescriptorBindingFlags bindingFlags[] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

                    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags = {};

                    setLayoutBindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                    setLayoutBindingFlags.bindingCount  = 1;
                    setLayoutBindingFlags.pBindingFlags = bindingFlags;

                    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
                    layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    layoutInfo.pNext                           = &setLayoutBindingFlags;
                    layoutInfo.bindingCount                    = 1;
                    layoutInfo.pBindings                       = &binding;

                    if (backendContext->vkFunctionTable.vkCreateDescriptorSetLayout(
                            backendContext->device, &layoutInfo, nullptr, &effectContext.bindlessTextureSrvDescriptorSetLayout) != VK_SUCCESS)
                    {
                     
                        return FFX_ERROR_BACKEND_API_ERROR;
                    }

                    // allocate descriptor set
                    VkDescriptorSetAllocateInfo setAllocateInfo = {};
                    setAllocateInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    setAllocateInfo.descriptorPool              = effectContext.bindlessDescriptorPool;
                    setAllocateInfo.descriptorSetCount          = 1;
                    setAllocateInfo.pSetLayouts                 = &effectContext.bindlessTextureSrvDescriptorSetLayout;

                    backendContext->vkFunctionTable.vkAllocateDescriptorSets(
                        backendContext->device, &setAllocateInfo, &effectContext.bindlessTextureSrvDescriptorSet);
                }

                // create the descriptor layout for bindless buffer srv buffers
                if (bindlessConfig->maxBufferSrvs > 0)
                {
                    VkDescriptorSetLayoutBinding binding = {
                        0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindlessConfig->maxBufferSrvs, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

                    VkDescriptorBindingFlags bindingFlags[] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

                    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags = {};

                    setLayoutBindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                    setLayoutBindingFlags.bindingCount  = 1;
                    setLayoutBindingFlags.pBindingFlags = bindingFlags;

                    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
                    layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    layoutInfo.pNext                           = &setLayoutBindingFlags;
                    layoutInfo.bindingCount                    = 1;
                    layoutInfo.pBindings                       = &binding;

                    if (backendContext->vkFunctionTable.vkCreateDescriptorSetLayout(
                            backendContext->device, &layoutInfo, nullptr, &effectContext.bindlessBufferSrvDescriptorSetLayout) != VK_SUCCESS)
                    {
                        return FFX_ERROR_BACKEND_API_ERROR;
                    }

                    // allocate descriptor set
                    VkDescriptorSetAllocateInfo setAllocateInfo = {};
                    setAllocateInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    setAllocateInfo.descriptorPool              = effectContext.bindlessDescriptorPool;
                    setAllocateInfo.descriptorSetCount          = 1;
                    setAllocateInfo.pSetLayouts                 = &effectContext.bindlessBufferSrvDescriptorSetLayout;

                    backendContext->vkFunctionTable.vkAllocateDescriptorSets(
                        backendContext->device, &setAllocateInfo, &effectContext.bindlessBufferSrvDescriptorSet);
                }

                // create the descriptor layout for bindless texture uav buffers
                if (bindlessConfig->maxTextureUavs > 0)
                {
                    VkDescriptorSetLayoutBinding binding = {
                        0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, bindlessConfig->maxTextureUavs, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

                    VkDescriptorBindingFlags bindingFlags[] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

                    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags = {};

                    setLayoutBindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                    setLayoutBindingFlags.bindingCount  = 1;
                    setLayoutBindingFlags.pBindingFlags = bindingFlags;

                    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
                    layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    layoutInfo.pNext                           = &setLayoutBindingFlags;
                    layoutInfo.bindingCount                    = 1;
                    layoutInfo.pBindings                       = &binding;

                    if (backendContext->vkFunctionTable.vkCreateDescriptorSetLayout(
                            backendContext->device, &layoutInfo, nullptr, &effectContext.bindlessTextureUavDescriptorSetLayout) != VK_SUCCESS)
                    {
                        return FFX_ERROR_BACKEND_API_ERROR;
                    }

                    // allocate descriptor set
                    VkDescriptorSetAllocateInfo setAllocateInfo = {};
                    setAllocateInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    setAllocateInfo.descriptorPool              = effectContext.bindlessDescriptorPool;
                    setAllocateInfo.descriptorSetCount          = 1;
                    setAllocateInfo.pSetLayouts                 = &effectContext.bindlessTextureUavDescriptorSetLayout;

                    backendContext->vkFunctionTable.vkAllocateDescriptorSets(
                        backendContext->device, &setAllocateInfo, &effectContext.bindlessTextureUavDescriptorSet);
                }

                // create the descriptor layout for bindless buffer uav buffers
                if (bindlessConfig->maxBufferUavs > 0)
                {
                    VkDescriptorSetLayoutBinding binding = {
                        0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, bindlessConfig->maxBufferUavs, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};

                    VkDescriptorBindingFlags bindingFlags[] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};

                    VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags = {};

                    setLayoutBindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
                    setLayoutBindingFlags.bindingCount  = 1;
                    setLayoutBindingFlags.pBindingFlags = bindingFlags;

                    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
                    layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
                    layoutInfo.pNext                           = &setLayoutBindingFlags;
                    layoutInfo.bindingCount                    = 1;
                    layoutInfo.pBindings                       = &binding;

                    if (backendContext->vkFunctionTable.vkCreateDescriptorSetLayout(
                            backendContext->device, &layoutInfo, nullptr, &effectContext.bindlessBufferUavDescriptorSetLayout) != VK_SUCCESS)
                    {
                        return FFX_ERROR_BACKEND_API_ERROR;
                    }

                    // allocate descriptor set
                    VkDescriptorSetAllocateInfo setAllocateInfo = {};
                    setAllocateInfo.sType                       = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                    setAllocateInfo.descriptorPool              = effectContext.bindlessDescriptorPool;
                    setAllocateInfo.descriptorSetCount          = 1;
                    setAllocateInfo.pSetLayouts                 = &effectContext.bindlessBufferUavDescriptorSetLayout;

                    backendContext->vkFunctionTable.vkAllocateDescriptorSets(
                        backendContext->device, &setAllocateInfo, &effectContext.bindlessBufferUavDescriptorSet);
                }
            }
            else
            {
                effectContext.bindlessTextureSrvHeapStart = 0;
                effectContext.bindlessTextureSrvHeapSize  = 0;
                effectContext.bindlessBufferSrvHeapSize   = 0;
                effectContext.bindlessTextureUavHeapStart = 0;
                effectContext.bindlessTextureUavHeapSize  = 0;
                effectContext.bindlessBufferUavHeapSize   = 0;
            }   

            break;
        }
    }

    return FFX_OK;
}

FfxErrorCode GetDeviceCapabilitiesVK(FfxInterface* backendInterface, FfxDeviceCapabilities* deviceCapabilities)
{
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    // no shader model in vulkan so assume the minimum
    deviceCapabilities->maximumSupportedShaderModel = FFX_SHADER_MODEL_5_1;
    deviceCapabilities->waveLaneCountMin = 32;
    deviceCapabilities->waveLaneCountMax = 32;
    deviceCapabilities->fp16Supported = false;
    deviceCapabilities->raytracingSupported = false;
    deviceCapabilities->deviceCoherentMemorySupported = false;
    deviceCapabilities->dedicatedAllocationSupported = false;
    deviceCapabilities->bufferMarkerSupported = false;
    deviceCapabilities->extendedSynchronizationSupported = false;
    deviceCapabilities->shaderStorageBufferArrayNonUniformIndexing = false;

    BackendContext_VK* context = (BackendContext_VK*)backendInterface->scratchBuffer;

    // check if extensions are enabled

    for (uint32_t i = 0; i < backendContext->numDeviceExtensions; i++)
    {
        if (strcmp(backendContext->extensionProperties[i].extensionName, VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME) == 0)
        {
            // check if we the max subgroup size allows us to use wave64
            VkPhysicalDeviceSubgroupSizeControlProperties subgroupSizeControlProperties = {};
            subgroupSizeControlProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_SIZE_CONTROL_PROPERTIES;

            VkPhysicalDeviceProperties2 deviceProperties2 = {};
            deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            deviceProperties2.pNext = &subgroupSizeControlProperties;
            vkGetPhysicalDeviceProperties2(context->physicalDevice, &deviceProperties2);

            deviceCapabilities->waveLaneCountMin = subgroupSizeControlProperties.minSubgroupSize;
            deviceCapabilities->waveLaneCountMax = subgroupSizeControlProperties.maxSubgroupSize;
        }
        else if (strcmp(backendContext->extensionProperties[i].extensionName, VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME) == 0)
        {
            // check for fp16 support
            VkPhysicalDeviceShaderFloat16Int8Features shaderFloat18Int8Features = {};
            shaderFloat18Int8Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;

            VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
            physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physicalDeviceFeatures2.pNext = &shaderFloat18Int8Features;

            vkGetPhysicalDeviceFeatures2(context->physicalDevice, &physicalDeviceFeatures2);

            deviceCapabilities->fp16Supported = (bool)shaderFloat18Int8Features.shaderFloat16;
        }
        else if (strcmp(backendContext->extensionProperties[i].extensionName, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) == 0)
        {
            // check for ray tracing support
            VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
            accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

            VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
            physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physicalDeviceFeatures2.pNext = &accelerationStructureFeatures;

            vkGetPhysicalDeviceFeatures2(context->physicalDevice, &physicalDeviceFeatures2);

            deviceCapabilities->raytracingSupported = (bool)accelerationStructureFeatures.accelerationStructure;
        }
        else if (strcmp(backendContext->extensionProperties[i].extensionName, VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME) == 0)
        {
            // check for coherent memory support
            VkPhysicalDeviceCoherentMemoryFeaturesAMD coherentMemoryFeatures = {};
            coherentMemoryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD;

            VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
            physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physicalDeviceFeatures2.pNext = &coherentMemoryFeatures;

            vkGetPhysicalDeviceFeatures2(context->physicalDevice, &physicalDeviceFeatures2);

            deviceCapabilities->deviceCoherentMemorySupported = (bool)coherentMemoryFeatures.deviceCoherentMemory;
        }
        else if (strcmp(backendContext->extensionProperties[i].extensionName, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
        {
            // no features structure so extension name is enough
            deviceCapabilities->dedicatedAllocationSupported = true;
        }
        else if (strcmp(backendContext->extensionProperties[i].extensionName, VK_AMD_BUFFER_MARKER_EXTENSION_NAME) == 0)
        {
            // no features structure so extension name is enough
            deviceCapabilities->bufferMarkerSupported = true;
        }
        else if (strcmp(backendContext->extensionProperties[i].extensionName, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME) == 0)
        {
            // check for extended synchronization support
            VkPhysicalDeviceSynchronization2FeaturesKHR synchronizationFeatures = {};
            synchronizationFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;

            VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
            physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physicalDeviceFeatures2.pNext = &synchronizationFeatures;

            vkGetPhysicalDeviceFeatures2(context->physicalDevice, &physicalDeviceFeatures2);

            deviceCapabilities->extendedSynchronizationSupported = (bool)synchronizationFeatures.synchronization2;
        }
        else if (strcmp(backendContext->extensionProperties[i].extensionName, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0)
        {
            // check for coherent memory support
            VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {};
            descriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

            VkPhysicalDeviceFeatures2 physicalDeviceFeatures2 = {};
            physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            physicalDeviceFeatures2.pNext = &descriptorIndexingFeatures;

            vkGetPhysicalDeviceFeatures2(context->physicalDevice, &physicalDeviceFeatures2);

            deviceCapabilities->shaderStorageBufferArrayNonUniformIndexing = (bool)descriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing;
        }
    }

    return FFX_OK;
}

FfxErrorCode DestroyBackendContextVK(FfxInterface* backendInterface, FfxUInt32 effectContextId)
{
    FFX_ASSERT(NULL != backendInterface);
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    FFX_ASSERT(backendContext->refCount > 0);

    // Delete any resources allocated by this context
    BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];
    for (uint32_t currentStaticResourceIndex = effectContextId * FFX_MAX_RESOURCE_COUNT; currentStaticResourceIndex < effectContext.nextStaticResource; ++currentStaticResourceIndex) {

        if (backendContext->pResources[currentStaticResourceIndex].imageResource != VK_NULL_HANDLE) {
            FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: SDK Resource was not destroyed prior to destroying the backend context. There is a resource leak.");
            FfxResourceInternal internalResource = { static_cast<int32_t>(currentStaticResourceIndex) };
            DestroyResourceVK(backendInterface, internalResource, effectContextId);
        }
    }

    for (uint32_t frameIndex = 0; frameIndex < FFX_MAX_QUEUED_FRAMES; ++frameIndex)
        destroyDynamicViews(backendContext, effectContextId, frameIndex);

    // clean up descriptor set layouts
    if (effectContext.bindlessTextureSrvDescriptorSetLayout)
    {
        backendContext->vkFunctionTable.vkFreeDescriptorSets(backendContext->device, effectContext.bindlessDescriptorPool, 1, &effectContext.bindlessTextureSrvDescriptorSet);
        backendContext->vkFunctionTable.vkDestroyDescriptorSetLayout(
            backendContext->device, effectContext.bindlessTextureSrvDescriptorSetLayout, VK_NULL_HANDLE);
        effectContext.bindlessTextureSrvDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (effectContext.bindlessBufferSrvDescriptorSetLayout)
    {
        backendContext->vkFunctionTable.vkFreeDescriptorSets(backendContext->device, effectContext.bindlessDescriptorPool, 1, &effectContext.bindlessBufferSrvDescriptorSet);
        backendContext->vkFunctionTable.vkDestroyDescriptorSetLayout(
            backendContext->device, effectContext.bindlessBufferSrvDescriptorSetLayout, VK_NULL_HANDLE);
        effectContext.bindlessBufferSrvDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (effectContext.bindlessTextureUavDescriptorSetLayout)
    {
        backendContext->vkFunctionTable.vkFreeDescriptorSets(backendContext->device, effectContext.bindlessDescriptorPool, 1, &effectContext.bindlessTextureUavDescriptorSet);
        backendContext->vkFunctionTable.vkDestroyDescriptorSetLayout(
            backendContext->device, effectContext.bindlessTextureUavDescriptorSetLayout, VK_NULL_HANDLE);
        effectContext.bindlessTextureUavDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (effectContext.bindlessBufferUavDescriptorSetLayout)
    {
        backendContext->vkFunctionTable.vkFreeDescriptorSets(backendContext->device, effectContext.bindlessDescriptorPool, 1, &effectContext.bindlessBufferUavDescriptorSet);
        backendContext->vkFunctionTable.vkDestroyDescriptorSetLayout(
            backendContext->device, effectContext.bindlessBufferUavDescriptorSetLayout, VK_NULL_HANDLE);
        effectContext.bindlessBufferUavDescriptorSetLayout = VK_NULL_HANDLE;
    }

    if (effectContext.bindlessDescriptorPool)
    {
        backendContext->vkFunctionTable.vkDestroyDescriptorPool(backendContext->device, effectContext.bindlessDescriptorPool, VK_NULL_HANDLE);
        effectContext.bindlessTextureSrvDescriptorSetLayout = VK_NULL_HANDLE;
    }

    // Free up for use by another context
    effectContext.nextStaticResource = 0;
    effectContext.active = false;

    // Decrement ref count
    --backendContext->refCount;

    if (!backendContext->refCount) {

        // clean up descriptor pool
        backendContext->vkFunctionTable.vkDestroyDescriptorPool(backendContext->device, backendContext->descriptorPool, VK_NULL_HANDLE);
        backendContext->descriptorPool = VK_NULL_HANDLE;

        // clean up dynamic uniform buffer & memory
        backendContext->vkFunctionTable.vkUnmapMemory(backendContext->device, backendContext->uniformBufferMemory);
        backendContext->vkFunctionTable.vkFreeMemory(backendContext->device, backendContext->uniformBufferMemory, VK_NULL_HANDLE);
        backendContext->vkFunctionTable.vkDestroyBuffer(backendContext->device, backendContext->uniformBuffer, VK_NULL_HANDLE);

        backendContext->device = VK_NULL_HANDLE;
        backendContext->physicalDevice = VK_NULL_HANDLE;

        resetBackendContext(backendContext);
    }

    return FFX_OK;
}

// create a internal resource that will stay alive until effect gets shut down
FfxErrorCode CreateResourceVK(
    FfxInterface* backendInterface,
    const FfxCreateResourceDescription* createResourceDescription,
    FfxUInt32 effectContextId,
    FfxResourceInternal* outResource)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != createResourceDescription);
    FFX_ASSERT(NULL != outResource);
    FFX_ASSERT_MESSAGE(createResourceDescription->initData.type != FFX_RESOURCE_INIT_DATA_TYPE_INVALID,
                       "InitData type cannot be FFX_RESOURCE_INIT_DATA_TYPE_INVALID. Please explicitly specify the resource initialization type.");


    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];
    VkDevice vkDevice = backendContext->device;

    FFX_ASSERT(VK_NULL_HANDLE != vkDevice);

    VkMemoryPropertyFlags requiredMemoryProperties;

    switch (createResourceDescription->heapType)
    {
    case FFX_HEAP_TYPE_DEFAULT:
        requiredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    case FFX_HEAP_TYPE_UPLOAD:
    case FFX_HEAP_TYPE_READBACK:
        requiredMemoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        break;
    default:
        requiredMemoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
    }

    // Setup the resource description
    FfxResourceDescription resourceDesc = createResourceDescription->resourceDescription;

    if (resourceDesc.mipCount == 0) {
        resourceDesc.mipCount = (uint32_t)(1 + floor(log2(FFX_MAXIMUM(FFX_MAXIMUM(createResourceDescription->resourceDescription.width,
            createResourceDescription->resourceDescription.height), createResourceDescription->resourceDescription.depth))));
    }

    FFX_ASSERT(effectContext.nextStaticResource + 1 < effectContext.nextDynamicResource);
    outResource->internalIndex = effectContext.nextStaticResource++;
    BackendContext_VK::Resource* backendResource = &backendContext->pResources[outResource->internalIndex];
    backendResource->undefined = true;  // A flag to make sure the first barrier for this image resource always uses an src layout of undefined
    backendResource->dynamic = false;   // Not a dynamic resource (need to track them separately for image views)
    backendResource->resourceDescription = resourceDesc;

    const auto& initData = createResourceDescription->initData;

    const FfxResourceStates resourceState =
        ((initData.type != FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED) && (createResourceDescription->heapType != FFX_HEAP_TYPE_UPLOAD))
            ? FFX_RESOURCE_STATE_COPY_DEST
            : createResourceDescription->initialState;
    backendResource->initialState = resourceState;
    backendResource->currentState = resourceState;

#ifdef _DEBUG
    size_t retval = 0;
    wcstombs_s(&retval, backendResource->resourceName, sizeof(backendResource->resourceName), createResourceDescription->name, sizeof(backendResource->resourceName));
    if (retval >= 64) backendResource->resourceName[63] = '\0';
#endif

    VkMemoryRequirements memRequirements = {};

    switch (createResourceDescription->resourceDescription.type)
    {
    case FFX_RESOURCE_TYPE_BUFFER:
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = createResourceDescription->resourceDescription.width;
        bufferInfo.usage = ffxGetVKBufferUsageFlagsFromResourceUsage(resourceDesc.usage);
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (initData.type != FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED)
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (resourceState == FFX_RESOURCE_STATE_COPY_SRC)
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        if (resourceState == FFX_RESOURCE_STATE_COPY_DEST)
            bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        if (backendContext->vkFunctionTable.vkCreateBuffer(backendContext->device, &bufferInfo, NULL, &backendResource->bufferResource) != VK_SUCCESS) {
            return FFX_ERROR_BACKEND_API_ERROR;
        }

#ifdef _DEBUG
        setVKObjectName(backendContext->vkFunctionTable, backendContext->device, VK_OBJECT_TYPE_BUFFER, (uint64_t)backendResource->bufferResource, backendResource->resourceName);
#endif

        backendContext->vkFunctionTable.vkGetBufferMemoryRequirements(backendContext->device, backendResource->bufferResource, &memRequirements);

        // allocate the memory
        FfxErrorCode errorCode = allocateDeviceMemory(backendContext, memRequirements, requiredMemoryProperties, backendResource);
        if (FFX_OK != errorCode)
            return errorCode;

        if (backendContext->vkFunctionTable.vkBindBufferMemory(backendContext->device, backendResource->bufferResource, backendResource->deviceMemory, 0) != VK_SUCCESS) {
            return FFX_ERROR_BACKEND_API_ERROR;
        }

        // if this is an upload buffer (currently only support upload buffers), copy the data and return
        if (createResourceDescription->heapType == FFX_HEAP_TYPE_UPLOAD && initData.size > 0)
        {
            // only allow copies directly into mapped memory for buffer resources since all texture resources are in optimal tiling
            void* data = NULL;

            if (backendContext->vkFunctionTable.vkMapMemory(backendContext->device, backendResource->deviceMemory, 0, initData.size, 0, &data) != VK_SUCCESS) {
                return FFX_ERROR_BACKEND_API_ERROR;
            }

            if (initData.type == FFX_RESOURCE_INIT_DATA_TYPE_BUFFER)
                memcpy(data, initData.buffer, initData.size);
            else if (initData.type == FFX_RESOURCE_INIT_DATA_TYPE_VALUE)
                memset(data, initData.value, initData.size);

            // flush mapped range if memory type is not coherant
            if ((backendResource->memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            {
                VkMappedMemoryRange memoryRange = {};
                memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                memoryRange.memory = backendResource->deviceMemory;
                memoryRange.size = initData.size;

                backendContext->vkFunctionTable.vkFlushMappedMemoryRanges(backendContext->device, 1, &memoryRange);
            }

            backendContext->vkFunctionTable.vkUnmapMemory(backendContext->device, backendResource->deviceMemory);
            return FFX_OK;
        }

        break;
    }
    case FFX_RESOURCE_TYPE_TEXTURE1D:
    case FFX_RESOURCE_TYPE_TEXTURE2D:
    case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
    case FFX_RESOURCE_TYPE_TEXTURE3D:
    {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = ffxGetVKImageTypeFromResourceType(createResourceDescription->resourceDescription.type);
        imageInfo.extent.width = createResourceDescription->resourceDescription.width;
        imageInfo.extent.height = createResourceDescription->resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE1D ? 1 : createResourceDescription->resourceDescription.height;
        imageInfo.extent.depth = ( createResourceDescription->resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE3D || createResourceDescription->resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE_CUBE ) ?
            createResourceDescription->resourceDescription.depth : 1;
        imageInfo.mipLevels = backendResource->resourceDescription.mipCount;
        imageInfo.arrayLayers = (createResourceDescription->resourceDescription.type ==
                                FFX_RESOURCE_TYPE_TEXTURE1D || createResourceDescription->resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE2D)
            ? createResourceDescription->resourceDescription.depth : 1;
        imageInfo.format = FFX_CONTAINS_FLAG(resourceDesc.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_FORMAT_D32_SFLOAT : ffxGetVkFormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = getVKImageUsageFlagsFromResourceUsage(resourceDesc.usage);
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (FFX_CONTAINS_FLAG(resourceDesc.usage, FFX_RESOURCE_USAGE_UAV) && ffxIsSurfaceFormatSRGB(createResourceDescription->resourceDescription.format))
        {
            imageInfo.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
            imageInfo.format = ffxGetVkFormatFromSurfaceFormat(ffxGetSurfaceFormatFromGamma(createResourceDescription->resourceDescription.format));
        }

        if (backendContext->vkFunctionTable.vkCreateImage(backendContext->device, &imageInfo, nullptr, &backendResource->imageResource) != VK_SUCCESS) {
            return FFX_ERROR_BACKEND_API_ERROR;
        }

#ifdef _DEBUG
        setVKObjectName(backendContext->vkFunctionTable, backendContext->device, VK_OBJECT_TYPE_IMAGE, (uint64_t)backendResource->imageResource, backendResource->resourceName);
#endif

        backendContext->vkFunctionTable.vkGetImageMemoryRequirements(backendContext->device, backendResource->imageResource, &memRequirements);

        // allocate the memory
        FfxErrorCode errorCode = allocateDeviceMemory(backendContext, memRequirements, requiredMemoryProperties, backendResource);
        if (FFX_OK != errorCode)
            return errorCode;

        if (backendContext->vkFunctionTable.vkBindImageMemory(backendContext->device, backendResource->imageResource, backendResource->deviceMemory, 0) != VK_SUCCESS) {
            return FFX_ERROR_BACKEND_API_ERROR;
        }

        break;
    }
    default:
        FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: Unsupported resource type creation requested.");
        break;
    }

    // Create SRVs and UAVs
    switch (createResourceDescription->resourceDescription.type)
    {
    case FFX_RESOURCE_TYPE_BUFFER:
        break;
    case FFX_RESOURCE_TYPE_TEXTURE1D:
    case FFX_RESOURCE_TYPE_TEXTURE2D:
    case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
    case FFX_RESOURCE_TYPE_TEXTURE3D:
    {
        FFX_ASSERT_MESSAGE(effectContext.nextStaticResourceView + 1 < effectContext.nextDynamicResourceView[0],
            "FFXInterface: Vulkan: We've run out of resource views. Please increase the size.");
        backendResource->srvViewIndex = effectContext.nextStaticResourceView++;

        FfxResourceType type = createResourceDescription->resourceDescription.type;
        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;

        bool requestArrayView = FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_ARRAYVIEW);

        switch (type)
        {
        case FFX_RESOURCE_TYPE_TEXTURE1D:
            imageViewCreateInfo.viewType = (backendResource->resourceDescription.depth > 1 || requestArrayView) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
            break;
        default:
        case FFX_RESOURCE_TYPE_TEXTURE2D:
            imageViewCreateInfo.viewType = (backendResource->resourceDescription.depth > 1 || requestArrayView) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            break;
        case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        case FFX_RESOURCE_TYPE_TEXTURE3D:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        }

        bool isDepth = FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET);
        imageViewCreateInfo.image = backendResource->imageResource;
        imageViewCreateInfo.format = isDepth ? VK_FORMAT_D32_SFLOAT : ffxGetVkFormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = isDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = backendResource->resourceDescription.mipCount;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        VkImageViewUsageCreateInfo imageViewUsageCreateInfo = {};
        addMutableViewForSRV(imageViewCreateInfo, imageViewUsageCreateInfo, backendResource->resourceDescription);

        // create an image view containing all mip levels for use as an srv
        if (backendContext->vkFunctionTable.vkCreateImageView(backendContext->device, &imageViewCreateInfo, NULL, &backendContext->pResourceViews[backendResource->srvViewIndex].imageView) != VK_SUCCESS) {
            return FFX_ERROR_BACKEND_API_ERROR;
        }
#ifdef _DEBUG
        setVKObjectName(backendContext->vkFunctionTable, backendContext->device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)backendContext->pResourceViews[backendResource->srvViewIndex].imageView, backendResource->resourceName);
#endif

        // create image views of individual mip levels for use as a uav
        if (FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_UAV))
        {
            const int32_t uavResourceViewCount = backendResource->resourceDescription.mipCount;
            FFX_ASSERT(effectContext.nextStaticResourceView + uavResourceViewCount < effectContext.nextDynamicResourceView[0]);

            backendResource->uavViewIndex = effectContext.nextStaticResourceView;
            backendResource->uavViewCount = uavResourceViewCount;

            imageViewCreateInfo.format = FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_FORMAT_D32_SFLOAT : ffxGetVKUAVFormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);

            for (uint32_t mip = 0; mip < backendResource->resourceDescription.mipCount; ++mip)
            {
                imageViewCreateInfo.subresourceRange.levelCount = 1;
                imageViewCreateInfo.subresourceRange.baseMipLevel = mip;

                if (backendContext->vkFunctionTable.vkCreateImageView(backendContext->device, &imageViewCreateInfo, NULL, &backendContext->pResourceViews[backendResource->uavViewIndex + mip].imageView) != VK_SUCCESS) {
                    return FFX_ERROR_BACKEND_API_ERROR;
                }
#ifdef _DEBUG
                setVKObjectName(backendContext->vkFunctionTable, backendContext->device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)backendContext->pResourceViews[backendResource->uavViewIndex + mip].imageView, backendResource->resourceName);
#endif
            }

            effectContext.nextStaticResourceView += uavResourceViewCount;
        }
        break;
    }
    default:
        FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: Unsupported resource view type creation requested.");
        break;
    }

    // create upload resource and upload job if needed
    if (initData.type != FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED)
    {
        FfxResourceInternal copySrc;
        FfxCreateResourceDescription uploadDesc = { *createResourceDescription };
        uploadDesc.heapType = FFX_HEAP_TYPE_UPLOAD;
        uploadDesc.resourceDescription.type = FFX_RESOURCE_TYPE_BUFFER;
        uploadDesc.resourceDescription.width = static_cast<uint32_t>(initData.size);
        uploadDesc.resourceDescription.usage = FFX_RESOURCE_USAGE_READ_ONLY;
        uploadDesc.initialState = FFX_RESOURCE_STATE_GENERIC_READ;
        uploadDesc.initData = createResourceDescription->initData;

        backendInterface->fpCreateResource(backendInterface, &uploadDesc, effectContextId, &copySrc);

        // setup the upload job
        FfxGpuJobDescription copyJob  = { FFX_GPU_JOB_COPY, L"Resource Initialization Copy" };
        copyJob.copyJobDescriptor.src = copySrc;
        copyJob.copyJobDescriptor.dst = *outResource;
        copyJob.copyJobDescriptor.srcOffset = 0;
        copyJob.copyJobDescriptor.dstOffset = 0;
        copyJob.copyJobDescriptor.size      = 0;

        backendInterface->fpScheduleGpuJob(backendInterface, &copyJob);
    }

    return FFX_OK;
}

FfxErrorCode DestroyResourceVK(FfxInterface* backendInterface, FfxResourceInternal resource, FfxUInt32 effectContextId)
{
    FFX_ASSERT(backendInterface != nullptr);
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];

    if ((resource.internalIndex >= int32_t(effectContextId * FFX_MAX_RESOURCE_COUNT)) && (resource.internalIndex < int32_t(effectContext.nextStaticResource)))
    {
        BackendContext_VK::Resource& backgroundResource = backendContext->pResources[resource.internalIndex];

        if (backgroundResource.resourceDescription.type == FFX_RESOURCE_TYPE_BUFFER)
        {
            // Destroy the resource
            if (backgroundResource.bufferResource != VK_NULL_HANDLE)
            {
                backendContext->vkFunctionTable.vkDestroyBuffer(backendContext->device, backgroundResource.bufferResource, nullptr);
                backgroundResource.bufferResource = VK_NULL_HANDLE;
            }
        }
        else
        {
            // Destroy SRV
            if (backgroundResource.srvViewIndex >= 0)
            {
                backendContext->vkFunctionTable.vkDestroyImageView(backendContext->device, backendContext->pResourceViews[backgroundResource.srvViewIndex].imageView, nullptr);
                backendContext->pResourceViews[backgroundResource.srvViewIndex].imageView = VK_NULL_HANDLE;
                backgroundResource.srvViewIndex                                           = 0;
            }

             // And UAVs
             if (FFX_CONTAINS_FLAG(backgroundResource.resourceDescription.usage, FFX_RESOURCE_USAGE_UAV))
             {
                 for (uint32_t i = 0; i < backgroundResource.uavViewCount; ++i)
                 {
                     if (backendContext->pResourceViews[backgroundResource.uavViewIndex + i].imageView != VK_NULL_HANDLE)
                     {
                         backendContext->vkFunctionTable.vkDestroyImageView(backendContext->device, backendContext->pResourceViews[backgroundResource.uavViewIndex + i].imageView, nullptr);
                         backendContext->pResourceViews[backgroundResource.uavViewIndex + i].imageView = VK_NULL_HANDLE;
                     }
                 }
             }

            // Reset indices to resource views
            backgroundResource.uavViewIndex = backgroundResource.srvViewIndex = -1;
            backgroundResource.uavViewCount = 0;

            // Destroy the resource
            if (backgroundResource.imageResource != VK_NULL_HANDLE)
            {
                backendContext->vkFunctionTable.vkDestroyImage(backendContext->device, backgroundResource.imageResource, nullptr);
                backgroundResource.imageResource = VK_NULL_HANDLE;
            }
        }

        if (backgroundResource.deviceMemory)
        {
            backendContext->vkFunctionTable.vkFreeMemory(backendContext->device, backgroundResource.deviceMemory, nullptr);
            backgroundResource.deviceMemory = VK_NULL_HANDLE;
        }
    }

    return FFX_OK;
}

FfxErrorCode MapResourceVK(FfxInterface* backendInterface, FfxResourceInternal resource, void** ptr)
{
    FFX_ASSERT(NULL != backendInterface);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    BackendContext_VK::Resource* res = &backendContext->pResources[resource.internalIndex];

    if ((backendContext->vkFunctionTable.vkMapMemory(backendContext->device, res->deviceMemory, 0, res->resourceDescription.size, 0, ptr) != VK_SUCCESS))
        return FFX_ERROR_BACKEND_API_ERROR;

    return FFX_OK;
}

FfxErrorCode UnmapResourceVK(FfxInterface* backendInterface, FfxResourceInternal resource)
{
    FFX_ASSERT(NULL != backendInterface);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    BackendContext_VK::Resource* res = &backendContext->pResources[resource.internalIndex];

    backendContext->vkFunctionTable.vkUnmapMemory(backendContext->device, res->deviceMemory);

    return FFX_OK;
}

FfxErrorCode RegisterResourceVK(
    FfxInterface* backendInterface,
    const FfxResource* inFfxResource,
    FfxUInt32 effectContextId,
    FfxResourceInternal* outFfxResourceInternal
)
{
    FFX_ASSERT(NULL != backendInterface);
    BackendContext_VK* backendContext = (BackendContext_VK*)(backendInterface->scratchBuffer);
    BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];

    if (inFfxResource->resource == nullptr) {

        outFfxResourceInternal->internalIndex = 0; // Always maps to FFX_<feature>_RESOURCE_IDENTIFIER_NULL;
        return FFX_OK;
    }

    // In vulkan we need to treat dynamic resources a little differently due to needing views to live as long as the GPU needs them.
    // We will treat them more like static resources and use the nextDynamicResource as a "hint" for where it should be.
    // Failure to find the pre-existing resource at the expected location will force a search until the resource is found.
    // If it is not found, a new entry will be created
    FFX_ASSERT(effectContext.nextDynamicResource > effectContext.nextStaticResource);
    outFfxResourceInternal->internalIndex = effectContext.nextDynamicResource--;

    //bool setupDynamicResource = false;
    BackendContext_VK::Resource* backendResource = &backendContext->pResources[outFfxResourceInternal->internalIndex];

    // Start by seeing if this entry is empty, as that triggers an automatic setup of a new dynamic resource
    /*if (backendResource->uavViewIndex < 0 && backendResource->srvViewIndex < 0)
    {
        setupDynamicResource = true;
    }

    // If not a new resource, does it match what's current slotted for this dynamic resource
    if (!setupDynamicResource)
    {
        // If this is us, just return as everything is setup as needed
        if ((backendResource->resourceDescription.type == FFX_RESOURCE_TYPE_BUFFER && backendResource->bufferResource == (VkBuffer)inFfxResource->resource) ||
            (backendResource->resourceDescription.type != FFX_RESOURCE_TYPE_BUFFER && backendResource->imageResource == (VkImage)inFfxResource->resource))
            return FFX_OK;

        // If this isn't us, search until we either find our entry or an empty resource
        outFfxResourceInternal->internalIndex = (effectContextId * FFX_MAX_RESOURCE_COUNT) + FFX_MAX_RESOURCE_COUNT - 1;
        while (!setupDynamicResource)
        {
            FFX_ASSERT(outFfxResourceInternal->internalIndex > effectContext.nextStaticResource); // Safety check while iterating
            backendResource = &backendContext->pResources[outFfxResourceInternal->internalIndex];

            // Is this us?
            if ((backendResource->resourceDescription.type == FFX_RESOURCE_TYPE_BUFFER && backendResource->bufferResource == (VkBuffer)inFfxResource->resource) ||
                (backendResource->resourceDescription.type != FFX_RESOURCE_TYPE_BUFFER && backendResource->imageResource == (VkImage)inFfxResource->resource))
            {
                 copyResourceState(backendResource, inFfxResource);
                 return FFX_OK;
            }

            // Empty?
            if (backendResource->uavViewIndex == -1 && backendResource->srvViewIndex == -1)
            {
                setupDynamicResource = true;
                break;
            }

            --outFfxResourceInternal->internalIndex;
        }
    }*/

    // If we got here, we are setting up a new dynamic entry
    backendResource->resourceDescription = inFfxResource->description;
    if (inFfxResource->description.type == FFX_RESOURCE_TYPE_BUFFER)
        backendResource->bufferResource = reinterpret_cast<VkBuffer>(inFfxResource->resource);
    else
        backendResource->imageResource = reinterpret_cast<VkImage>(inFfxResource->resource);

    copyResourceState(backendResource, inFfxResource);

#ifdef _DEBUG
    size_t retval = 0;
    wcstombs_s(&retval, backendResource->resourceName, sizeof(backendResource->resourceName), inFfxResource->name, sizeof(backendResource->resourceName));
    if (retval >= 64) backendResource->resourceName[63] = '\0';
#endif

    // the first call of RegisterResource can be identified because


    //////////////////////////////////////////////////////////////////////////
    // Create SRVs and UAVs
    switch (backendResource->resourceDescription.type)
    {
    case FFX_RESOURCE_TYPE_BUFFER:
        break;
    case FFX_RESOURCE_TYPE_TEXTURE1D:
    case FFX_RESOURCE_TYPE_TEXTURE2D:
    case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
    case FFX_RESOURCE_TYPE_TEXTURE3D:
    {
        FfxResourceType type = backendResource->resourceDescription.type;
        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = nullptr;

        bool requestArrayView = FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_ARRAYVIEW);

        switch (type)
        {
        case FFX_RESOURCE_TYPE_TEXTURE1D:
            imageViewCreateInfo.viewType = (backendResource->resourceDescription.depth > 1 || requestArrayView) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
            break;
        default:
        case FFX_RESOURCE_TYPE_TEXTURE2D:
            imageViewCreateInfo.viewType = (backendResource->resourceDescription.depth > 1 || requestArrayView) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            break;
        case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        case FFX_RESOURCE_TYPE_TEXTURE3D:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        }

        imageViewCreateInfo.image = backendResource->imageResource;
        imageViewCreateInfo.format = FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_FORMAT_D32_SFLOAT : ffxGetVkFormatFromSurfaceFormat(backendResource->resourceDescription.format);
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = backendResource->resourceDescription.mipCount;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        // create an image view containing all mip levels for use as an srv
        FFX_ASSERT(effectContext.nextDynamicResourceView[effectContext.frameIndex] > ((effectContext.frameIndex == 0) ? effectContext.nextStaticResourceView : getDynamicResourceViewsStartIndex(effectContextId, effectContext.frameIndex - 1)));
        backendResource->srvViewIndex = effectContext.nextDynamicResourceView[effectContext.frameIndex]--;

        VkImageViewUsageCreateInfo imageViewUsageCreateInfo = {};
        addMutableViewForSRV(imageViewCreateInfo, imageViewUsageCreateInfo, backendResource->resourceDescription);

        if (backendContext->vkFunctionTable.vkCreateImageView(backendContext->device, &imageViewCreateInfo, NULL, &backendContext->pResourceViews[backendResource->srvViewIndex].imageView) != VK_SUCCESS) {
            return FFX_ERROR_BACKEND_API_ERROR;
        }
#ifdef _DEBUG
        setVKObjectName(backendContext->vkFunctionTable, backendContext->device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)backendContext->pResourceViews[backendResource->srvViewIndex].imageView, backendResource->resourceName);
#endif

        // create image views of individual mip levels for use as a uav
        if (FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_UAV))
        {
            const int32_t uavResourceViewCount = backendResource->resourceDescription.mipCount;
            FFX_ASSERT(effectContext.nextDynamicResourceView[effectContext.frameIndex] - uavResourceViewCount + 1 > ((effectContext.frameIndex == 0) ? effectContext.nextStaticResourceView : getDynamicResourceViewsStartIndex(effectContextId, effectContext.frameIndex - 1)));
            backendResource->uavViewIndex = effectContext.nextDynamicResourceView[effectContext.frameIndex] - uavResourceViewCount + 1;
            backendResource->uavViewCount = uavResourceViewCount;

            imageViewCreateInfo.format = FFX_CONTAINS_FLAG(backendResource->resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_FORMAT_D32_SFLOAT : ffxGetVKUAVFormatFromSurfaceFormat(backendResource->resourceDescription.format);
            imageViewCreateInfo.pNext  = nullptr;

            for (uint32_t mip = 0; mip < backendResource->resourceDescription.mipCount; ++mip)
            {
                imageViewCreateInfo.subresourceRange.levelCount = 1;
                imageViewCreateInfo.subresourceRange.baseMipLevel = mip;

                if (backendContext->vkFunctionTable.vkCreateImageView(backendContext->device, &imageViewCreateInfo, NULL, &backendContext->pResourceViews[backendResource->uavViewIndex + mip].imageView) != VK_SUCCESS) {
                    return FFX_ERROR_BACKEND_API_ERROR;
                }
#ifdef _DEBUG
                setVKObjectName(backendContext->vkFunctionTable, backendContext->device, VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)backendContext->pResourceViews[backendResource->uavViewIndex + mip].imageView, backendResource->resourceName);
#endif
            }
            effectContext.nextDynamicResourceView[effectContext.frameIndex] -= uavResourceViewCount;
        }
        break;
    }
    default:
        FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: Unsupported resource view type creation requested.");
        break;
    }

    return FFX_OK;
}

FfxResource GetResourceVK(FfxInterface* backendInterface, FfxResourceInternal inResource)
{
    FFX_ASSERT(nullptr != backendInterface);
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    FfxResourceDescription ffxResDescription = backendInterface->fpGetResourceDescription(backendInterface, inResource);

    FfxResource resource = {};
    resource.resource = reinterpret_cast<void*>(backendContext->pResources[inResource.internalIndex].imageResource);
    // If the internal resource state is undefined, that means we are importing a resource that
    // has not yet been initialized, so we will flag it as such to finish initializing it later
    // before it is used.
    if (backendContext->pResources[inResource.internalIndex].undefined) {
        ffxResDescription.flags = (FfxResourceFlags)((int)ffxResDescription.flags | FFX_RESOURCE_FLAGS_UNDEFINED);
        // Flag it as no longer being undefined as it will no longer be after workload
        // execution
        backendContext->pResources[inResource.internalIndex].undefined = false;
    }
    resource.state = backendContext->pResources[inResource.internalIndex].currentState;
    resource.description = ffxResDescription;

#ifdef _DEBUG
    if (backendContext->pResources[inResource.internalIndex].resourceName)
    {
        ConvertUTF8ToUTF16(backendContext->pResources[inResource.internalIndex].resourceName, resource.name, 64);
    }
#endif

    return resource;
}

// dispose dynamic resources: This should be called at the end of the frame
FfxErrorCode UnregisterResourcesVK(FfxInterface* backendInterface, FfxCommandList commandList, FfxUInt32 effectContextId)
{
    FFX_ASSERT(NULL != backendInterface);
    BackendContext_VK* backendContext = (BackendContext_VK*)(backendInterface->scratchBuffer);
    BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];

    // Walk back all the resources that don't belong to us and reset them to their initial state
    const uint32_t dynamicResourceIndexStart = getDynamicResourcesStartIndex(effectContextId);
    for (uint32_t resourceIndex = ++effectContext.nextDynamicResource; resourceIndex <= dynamicResourceIndexStart; ++resourceIndex)
    {
        FfxResourceInternal internalResource;
        internalResource.internalIndex = resourceIndex;

        BackendContext_VK::Resource* backendResource = &backendContext->pResources[resourceIndex];

        // Also clear out their srv/uav indices so they are regenerated each frame
        backendResource->uavViewIndex = -1;
        backendResource->srvViewIndex = -1;

        // Add the barrier
        addBarrier(backendContext, &internalResource, backendResource->initialState);
    }

    FFX_ASSERT(nullptr != commandList);
    VkCommandBuffer pCmdList = reinterpret_cast<VkCommandBuffer>(commandList);

    flushBarriers(backendContext, pCmdList);

    // Just reset the dynamic resource index, but leave the images views.
    // They will be deleted in the first pipeline destroy call as they need to live until then
    effectContext.nextDynamicResource = dynamicResourceIndexStart;

    // destroy the views of the next frame
    effectContext.frameIndex = (effectContext.frameIndex + 1) % FFX_MAX_QUEUED_FRAMES;
    destroyDynamicViews(backendContext, effectContextId, effectContext.frameIndex);

    return FFX_OK;
}

FfxErrorCode registerStaticTextureSrv(BackendContext_VK* backendContext, const FfxResource* inResource, uint32_t index, FfxUInt32 effectContextId)
{            
    BackendContext_VK::EffectContext& effectContext  = backendContext->pEffectContexts[effectContextId];

    if (effectContext.bindlessTextureSrvHeapSize <= index)
    {
        FFX_ASSERT(false);
        return FFX_ERROR_INSUFFICIENT_MEMORY;
    }

    VkImage vkImage = reinterpret_cast<VkImage>(inResource->resource);

    // Create SRVs and UAVs
    switch (inResource->description.type)
    {
    case FFX_RESOURCE_TYPE_BUFFER:
    {
        FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: Attempting to register a Buffer as a Texture SRV.");
        break;
    }
    case FFX_RESOURCE_TYPE_TEXTURE1D:
    case FFX_RESOURCE_TYPE_TEXTURE2D:
    case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
    case FFX_RESOURCE_TYPE_TEXTURE3D:
    {
        FfxResourceType       type                = inResource->description.type;
        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext                 = nullptr;

        bool requestArrayView = FFX_CONTAINS_FLAG(inResource->description.usage, FFX_RESOURCE_USAGE_ARRAYVIEW);

        switch (type)
        {
        case FFX_RESOURCE_TYPE_TEXTURE1D:
            imageViewCreateInfo.viewType = (inResource->description.depth > 1 || requestArrayView) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
            break;
        default:
        case FFX_RESOURCE_TYPE_TEXTURE2D:
            imageViewCreateInfo.viewType = (inResource->description.depth > 1 || requestArrayView) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            break;
        case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        case FFX_RESOURCE_TYPE_TEXTURE3D:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        }

        imageViewCreateInfo.image                           = vkImage;
        imageViewCreateInfo.format                          = FFX_CONTAINS_FLAG(inResource->description.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_FORMAT_D32_SFLOAT : ffxGetVkFormatFromSurfaceFormat(inResource->description.format);
        imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask     = FFX_CONTAINS_FLAG(inResource->description.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
        imageViewCreateInfo.subresourceRange.levelCount     = inResource->description.mipCount;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

        uint32_t imageViewIndex = effectContext.bindlessTextureSrvHeapStart + index;

        if (backendContext->pResourceViews[imageViewIndex].imageView)
            backendContext->vkFunctionTable.vkDestroyImageView(backendContext->device, backendContext->pResourceViews[imageViewIndex].imageView, nullptr);

        if (backendContext->vkFunctionTable.vkCreateImageView(
                backendContext->device, &imageViewCreateInfo, NULL, &backendContext->pResourceViews[imageViewIndex].imageView) != VK_SUCCESS)
        {
            return FFX_ERROR_BACKEND_API_ERROR;
        }

#ifdef _DEBUG
        size_t retval = 0;
        char   resourceName[64];
        wcstombs_s(&retval, resourceName, sizeof(resourceName), inResource->name, sizeof(resourceName));
        if (retval >= 64) resourceName[63] = '\0';

        setVKObjectName(backendContext->vkFunctionTable,
                        backendContext->device,
                        VK_OBJECT_TYPE_IMAGE_VIEW,
                        (uint64_t)backendContext->pResourceViews[imageViewIndex].imageView,
                        resourceName);
#endif

        VkWriteDescriptorSet  writeDescriptorSet  = {};
        VkDescriptorImageInfo imageDescriptorInfo = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};

        writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet          = effectContext.bindlessTextureSrvDescriptorSet;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writeDescriptorSet.pImageInfo      = &imageDescriptorInfo;
        writeDescriptorSet.dstBinding      = 0;
        writeDescriptorSet.dstArrayElement = index;

        imageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageDescriptorInfo.imageView   = backendContext->pResourceViews[imageViewIndex].imageView;

        backendContext->vkFunctionTable.vkUpdateDescriptorSets(backendContext->device, 1, &writeDescriptorSet, 0, nullptr);

        return FFX_OK;

        break;
    }
    default:
        break;
    }

    return FFX_ERROR_INVALID_ARGUMENT;
}            
             
FfxErrorCode registerStaticBufferSrv(BackendContext_VK* backendContext, const FfxResource* inResource, uint32_t offset, uint32_t size, uint32_t stride, uint32_t index, FfxUInt32 effectContextId)
{            
    BackendContext_VK::EffectContext& effectContext  = backendContext->pEffectContexts[effectContextId];

    if (effectContext.bindlessBufferSrvHeapSize <= index)
    {
        FFX_ASSERT(false);
        return FFX_ERROR_INSUFFICIENT_MEMORY;
    }

    if (inResource->description.type == FFX_RESOURCE_TYPE_BUFFER)
    {
        VkBuffer buffer = reinterpret_cast<VkBuffer>(inResource->resource);

        VkWriteDescriptorSet   writeDescriptorSet   = {};
        VkDescriptorBufferInfo bufferDescriptorInfo = {VK_NULL_HANDLE, 0, VK_WHOLE_SIZE};

        writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet          = effectContext.bindlessBufferSrvDescriptorSet;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeDescriptorSet.pBufferInfo     = &bufferDescriptorInfo;
        writeDescriptorSet.dstBinding      = 0;
        writeDescriptorSet.dstArrayElement = index;

        bufferDescriptorInfo.buffer = buffer;
        bufferDescriptorInfo.offset = offset;
        bufferDescriptorInfo.range  = size > 0 ? size : VK_WHOLE_SIZE;

        backendContext->vkFunctionTable.vkUpdateDescriptorSets(backendContext->device, 1, &writeDescriptorSet, 0, nullptr);

        return FFX_OK;
    }
    else
        FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: Attempting to register a Texture as a Buffer SRV.");

    return FFX_ERROR_INVALID_ARGUMENT;
}            
             
FfxErrorCode registerStaticTextureUav(BackendContext_VK* backendContext, const FfxResource* inResource, uint32_t mip, uint32_t index, FfxUInt32 effectContextId)
{            
    BackendContext_VK::EffectContext& effectContext  = backendContext->pEffectContexts[effectContextId];

    if (effectContext.bindlessTextureUavHeapSize <= index)
    {
        FFX_ASSERT(false);
        return FFX_ERROR_INSUFFICIENT_MEMORY;
    }

    VkImage vkImage = reinterpret_cast<VkImage>(inResource->resource);

    // Create SRVs and UAVs
    switch (inResource->description.type)
    {
    case FFX_RESOURCE_TYPE_BUFFER:
    {
        FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: Attempting to register a Buffer as a Texture UAV.");
        break;
    }
    case FFX_RESOURCE_TYPE_TEXTURE1D:
    case FFX_RESOURCE_TYPE_TEXTURE2D:
    case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
    case FFX_RESOURCE_TYPE_TEXTURE3D:
    {
        FfxResourceType       type                = inResource->description.type;
        VkImageViewCreateInfo imageViewCreateInfo = {};
        imageViewCreateInfo.sType                 = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext                 = nullptr;

        bool requestArrayView = FFX_CONTAINS_FLAG(inResource->description.usage, FFX_RESOURCE_USAGE_ARRAYVIEW);

        switch (type)
        {
        case FFX_RESOURCE_TYPE_TEXTURE1D:
            imageViewCreateInfo.viewType = (inResource->description.depth > 1 || requestArrayView) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
            break;
        default:
        case FFX_RESOURCE_TYPE_TEXTURE2D:
            imageViewCreateInfo.viewType = (inResource->description.depth > 1 || requestArrayView) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            break;
        case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        case FFX_RESOURCE_TYPE_TEXTURE3D:
            imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        }

        imageViewCreateInfo.image                           = vkImage;
        imageViewCreateInfo.format                          = FFX_CONTAINS_FLAG(inResource->description.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_FORMAT_D32_SFLOAT : ffxGetVkFormatFromSurfaceFormat(inResource->description.format);
        imageViewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask     = FFX_CONTAINS_FLAG(inResource->description.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
        imageViewCreateInfo.subresourceRange.levelCount     = inResource->description.mipCount;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

        uint32_t imageViewIndex = effectContext.bindlessTextureUavHeapStart + index;

        if (backendContext->pResourceViews[imageViewIndex].imageView)
            backendContext->vkFunctionTable.vkDestroyImageView(backendContext->device, backendContext->pResourceViews[imageViewIndex].imageView, nullptr);

        if (backendContext->vkFunctionTable.vkCreateImageView(
                backendContext->device, &imageViewCreateInfo, NULL, &backendContext->pResourceViews[imageViewIndex].imageView) != VK_SUCCESS)
        {
            return FFX_ERROR_BACKEND_API_ERROR;
        }

#ifdef _DEBUG
        size_t retval = 0;
        char   resourceName[64];
        wcstombs_s(&retval, resourceName, sizeof(resourceName), inResource->name, sizeof(resourceName));
        if (retval >= 64) resourceName[63] = '\0';

        setVKObjectName(backendContext->vkFunctionTable,
                        backendContext->device,
                        VK_OBJECT_TYPE_IMAGE_VIEW,
                        (uint64_t)backendContext->pResourceViews[imageViewIndex].imageView,
                        resourceName);
#endif

        VkWriteDescriptorSet  writeDescriptorSet  = {};
        VkDescriptorImageInfo imageDescriptorInfo = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};

        writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet          = effectContext.bindlessTextureUavDescriptorSet;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeDescriptorSet.pImageInfo      = &imageDescriptorInfo;
        writeDescriptorSet.dstBinding      = 0;
        writeDescriptorSet.dstArrayElement = index;

        imageDescriptorInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageDescriptorInfo.imageView   = backendContext->pResourceViews[imageViewIndex].imageView;

        backendContext->vkFunctionTable.vkUpdateDescriptorSets(backendContext->device, 1, &writeDescriptorSet, 0, nullptr);

        return FFX_OK;

        break;
    }
    default:
        break;
    }

    return FFX_ERROR_INVALID_ARGUMENT;
}            
             
FfxErrorCode registerStaticBufferUav(BackendContext_VK* backendContext, const FfxResource* inResource, uint32_t offset, uint32_t size, uint32_t stride, uint32_t index, FfxUInt32 effectContextId)
{
    BackendContext_VK::EffectContext& effectContext  = backendContext->pEffectContexts[effectContextId];

    if (effectContext.bindlessBufferUavHeapSize <= index)
    {
        FFX_ASSERT(false);
        return FFX_ERROR_INSUFFICIENT_MEMORY;
    }

    if (inResource->description.type == FFX_RESOURCE_TYPE_BUFFER)
    {
        VkBuffer buffer = reinterpret_cast<VkBuffer>(inResource->resource);

        VkWriteDescriptorSet   writeDescriptorSet   = {};
        VkDescriptorBufferInfo bufferDescriptorInfo = {VK_NULL_HANDLE, 0, VK_WHOLE_SIZE};

        writeDescriptorSet.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSet.dstSet          = effectContext.bindlessBufferUavDescriptorSet;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeDescriptorSet.pBufferInfo     = &bufferDescriptorInfo;
        writeDescriptorSet.dstBinding      = 0;
        writeDescriptorSet.dstArrayElement = index;

        bufferDescriptorInfo.buffer = buffer;
        bufferDescriptorInfo.offset = offset;
        bufferDescriptorInfo.range  = size > 0 ? size : VK_WHOLE_SIZE;

        backendContext->vkFunctionTable.vkUpdateDescriptorSets(backendContext->device, 1, &writeDescriptorSet, 0, nullptr);

        return FFX_OK;
    }
    else
        FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: Attempting to register a Texture as a Buffer UAV.");

    return FFX_ERROR_INVALID_ARGUMENT;
}

FfxErrorCode RegisterStaticResourceVK(FfxInterface* backendInterface, const FfxStaticResourceDescription* desc, FfxUInt32 effectContextId)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != desc);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    switch (desc->descriptorType)
    {
    case FFX_DESCRIPTOR_TEXTURE_SRV:
        return registerStaticTextureSrv(backendContext, desc->resource, desc->descriptorIndex, effectContextId);
    case FFX_DESCRIPTOR_BUFFER_SRV:
        return registerStaticBufferSrv(
            backendContext, desc->resource, desc->bufferOffset, desc->bufferSize, desc->bufferStride, desc->descriptorIndex, effectContextId);
    case FFX_DESCRIPTOR_TEXTURE_UAV:
        return registerStaticTextureUav(backendContext, desc->resource, desc->textureUavMip, desc->descriptorIndex, effectContextId);
    case FFX_DESCRIPTOR_BUFFER_UAV:
        return registerStaticBufferUav(
            backendContext, desc->resource, desc->bufferOffset, desc->bufferSize, desc->bufferStride, desc->descriptorIndex, effectContextId);
    default:
        return FFX_ERROR_INVALID_ARGUMENT;
    }
}

FfxResourceDescription GetResourceDescriptionVK(FfxInterface* backendInterface, FfxResourceInternal resource)
{
    FFX_ASSERT(NULL != backendInterface);
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    FfxResourceDescription resourceDescription = backendContext->pResources[resource.internalIndex].resourceDescription;
    return resourceDescription;
}

FfxErrorCode StageConstantBufferDataVK(FfxInterface* backendInterface, void* data, FfxUInt32 size, FfxConstantBuffer* constantBuffer)
{
    FFX_ASSERT(NULL != backendInterface);
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    if (data && constantBuffer)
    {
        if ((backendContext->stagingRingBufferBase + FFX_ALIGN_UP(size, 256)) >= FFX_CONSTANT_BUFFER_RING_BUFFER_SIZE)
            backendContext->stagingRingBufferBase = 0;

        uint32_t* dstPtr = (uint32_t*)(backendContext->pStagingRingBuffer + backendContext->stagingRingBufferBase);

        memcpy(dstPtr, data, size);

        constantBuffer->data            = dstPtr;
        constantBuffer->num32BitEntries = size / sizeof(uint32_t);

        backendContext->stagingRingBufferBase += FFX_ALIGN_UP(size, 256);

        return FFX_OK;
    }
    else
        return FFX_ERROR_INVALID_POINTER;
}

VkSamplerAddressMode FfxGetAddressModeVK(const FfxAddressMode& addressMode)
{
    switch (addressMode)
    {
    case FFX_ADDRESS_MODE_WRAP:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case FFX_ADDRESS_MODE_MIRROR:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case FFX_ADDRESS_MODE_CLAMP:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case FFX_ADDRESS_MODE_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case FFX_ADDRESS_MODE_MIRROR_ONCE:
        return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
    default:
        FFX_ASSERT_MESSAGE(false, "Unsupported addressing mode requested. Please implement");
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        break;
    }
}

FfxErrorCode CreatePipelineVK(FfxInterface* backendInterface,
    FfxEffect effect,
    FfxPass pass,
    uint32_t permutationOptions,
    const FfxPipelineDescription* pipelineDescription,
    FfxUInt32 effectContextId,
    FfxPipelineState* outPipeline)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != pipelineDescription);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];

    // start by fetching the shader blob
    FfxShaderBlob shaderBlob = { };
    // WON'T WORK WITH FSR3!!
    backendInterface->fpGetPermutationBlobByIndex(effect, pass, FFX_BIND_COMPUTE_SHADER_STAGE, permutationOptions, &shaderBlob);
    FFX_ASSERT(shaderBlob.data && shaderBlob.size);

    //////////////////////////////////////////////////////////////////////////
    // One root signature (or pipeline layout) per pipeline
    FFX_ASSERT_MESSAGE(effectContext.nextPipelineLayout < (effectContextId * FFX_MAX_PASS_COUNT) + FFX_MAX_PASS_COUNT, "FFXInterface: Vulkan: Ran out of pipeline layouts. Please increase FFX_MAX_PASS_COUNT");
    BackendContext_VK::PipelineLayout* pPipelineLayout = &backendContext->pPipelineLayouts[effectContext.nextPipelineLayout++];

    // Start by creating samplers
    FFX_ASSERT(pipelineDescription->samplerCount <= FFX_MAX_SAMPLERS);
    const size_t samplerCount = pipelineDescription->samplerCount;
    for (uint32_t currentSamplerIndex = 0; currentSamplerIndex < samplerCount; ++currentSamplerIndex)
    {
        VkSamplerCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.flags = 0;
        createInfo.minLod = 0.f;
        createInfo.maxLod = VK_LOD_CLAMP_NONE;
        createInfo.anisotropyEnable = false; // TODO: Do a check for anisotropy once it's an available filter option
        createInfo.compareEnable = false;
        createInfo.compareOp = VK_COMPARE_OP_NEVER;
        createInfo.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        createInfo.unnormalizedCoordinates = VK_FALSE;
        createInfo.addressModeU = FfxGetAddressModeVK(pipelineDescription->samplers[currentSamplerIndex].addressModeU);
        createInfo.addressModeV = FfxGetAddressModeVK(pipelineDescription->samplers[currentSamplerIndex].addressModeV);
        createInfo.addressModeW = FfxGetAddressModeVK(pipelineDescription->samplers[currentSamplerIndex].addressModeW);

        // Set the right filter
        switch (pipelineDescription->samplers[currentSamplerIndex].filter)
        {
        case FFX_FILTER_TYPE_MINMAGMIP_POINT:
            createInfo.minFilter = VK_FILTER_NEAREST;
            createInfo.magFilter = VK_FILTER_NEAREST;
            createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;
        case FFX_FILTER_TYPE_MINMAGMIP_LINEAR:
            createInfo.minFilter = VK_FILTER_LINEAR;
            createInfo.magFilter = VK_FILTER_LINEAR;
            createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            break;
        case FFX_FILTER_TYPE_MINMAGLINEARMIP_POINT:
            createInfo.minFilter = VK_FILTER_LINEAR;
            createInfo.magFilter = VK_FILTER_LINEAR;
            createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            break;

        default:
            FFX_ASSERT_MESSAGE(false, "FFXInterface: Vulkan: Unsupported filter type requested. Please implement");
            break;
        }

        if (backendContext->vkFunctionTable.vkCreateSampler(backendContext->device, &createInfo, nullptr, &pPipelineLayout->samplers[currentSamplerIndex]) != VK_SUCCESS) {
            return FFX_ERROR_BACKEND_API_ERROR;
        }
    }

    int32_t staticTextureSrvCount = 0;
    int32_t staticBufferSrvCount  = 0;
    int32_t staticTextureUavCount = 0;
    int32_t staticBufferUavCount  = 0;

    pPipelineLayout->staticTextureSrvSet = -1;
    pPipelineLayout->staticBufferSrvSet  = -1;
    pPipelineLayout->staticTextureUavSet = -1;
    pPipelineLayout->staticBufferUavSet  = -1;

    // Setup descriptor sets
    VkDescriptorSetLayoutBinding layoutBindings[MAX_DESCRIPTOR_SET_LAYOUTS];
    uint32_t numLayoutBindings = 0;

    // Support more when needed
    VkShaderStageFlags shaderStageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Samplers - just the static ones for now
    for (uint32_t currentSamplerIndex = 0; currentSamplerIndex < samplerCount; ++currentSamplerIndex)
        layoutBindings[numLayoutBindings++] = {
            currentSamplerIndex + SAMPLER_BINDING_SHIFT, VK_DESCRIPTOR_TYPE_SAMPLER, 1, shaderStageFlags, &pPipelineLayout->samplers[currentSamplerIndex]};

    // Texture SRVs
    for (uint32_t srvIndex = 0; srvIndex < shaderBlob.srvTextureCount; ++srvIndex)
    {
        // count static srvs separately.
        if (shaderBlob.boundSRVTextureSpaces[srvIndex] != 0)
        {
            if (staticTextureSrvCount > 0)
                FFX_ASSERT(pPipelineLayout->staticTextureSrvSet != shaderBlob.boundSRVTextureSpaces[srvIndex]);

            staticTextureSrvCount += shaderBlob.boundSRVTextureCounts[srvIndex];
            pPipelineLayout->staticTextureSrvSet = shaderBlob.boundSRVTextureSpaces[srvIndex];
            continue;
        }

        layoutBindings[numLayoutBindings++] = { shaderBlob.boundSRVTextures[srvIndex], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            shaderBlob.boundSRVTextureCounts[srvIndex], shaderStageFlags, nullptr };
    }

    // Buffer SRVs
    for (uint32_t srvIndex = 0; srvIndex < shaderBlob.srvBufferCount; ++srvIndex)
    {
        // count static srvs separately.
        if (shaderBlob.boundSRVBufferSpaces[srvIndex] != 0)
        {
            if (staticBufferSrvCount > 0)
                FFX_ASSERT(pPipelineLayout->staticBufferSrvSet != shaderBlob.boundSRVBufferSpaces[srvIndex]);

            staticBufferSrvCount += shaderBlob.boundSRVBufferCounts[srvIndex];
            pPipelineLayout->staticBufferSrvSet = shaderBlob.boundSRVBufferSpaces[srvIndex];
            continue;
        }

        layoutBindings[numLayoutBindings++] = { shaderBlob.boundSRVBuffers[srvIndex], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            shaderBlob.boundSRVBufferCounts[srvIndex], shaderStageFlags, nullptr};
    }

    // Texture UAVs
    for (uint32_t uavIndex = 0; uavIndex < shaderBlob.uavTextureCount; ++uavIndex)
    {
        // count static uavs separately.
        if (shaderBlob.boundUAVTextureSpaces[uavIndex] != 0)
        {
            if (staticTextureUavCount > 0)
                FFX_ASSERT(pPipelineLayout->staticTextureUavSet != shaderBlob.boundUAVTextureSpaces[uavIndex]);

            staticTextureUavCount += shaderBlob.boundUAVTextureCounts[uavIndex];
            pPipelineLayout->staticTextureUavSet = shaderBlob.boundUAVTextureSpaces[uavIndex];
            continue;
        }

        layoutBindings[numLayoutBindings++] = { shaderBlob.boundUAVTextures[uavIndex], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            shaderBlob.boundUAVTextureCounts[uavIndex], shaderStageFlags, nullptr };
    }

    // Buffer UAVs
    for (uint32_t uavIndex = 0; uavIndex < shaderBlob.uavBufferCount; ++uavIndex)
    {
        // count static uavs separately.
        if (shaderBlob.boundUAVBufferSpaces[uavIndex] != 0)
        {
            if (staticBufferUavCount > 0)
                FFX_ASSERT(pPipelineLayout->staticBufferUavSet != shaderBlob.boundUAVBufferSpaces[uavIndex]);

            staticBufferUavCount += shaderBlob.boundUAVBufferCounts[uavIndex];
            pPipelineLayout->staticBufferUavSet = shaderBlob.boundUAVBufferSpaces[uavIndex];
            continue;
        }

        layoutBindings[numLayoutBindings++] = { shaderBlob.boundUAVBuffers[uavIndex], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            shaderBlob.boundUAVBufferCounts[uavIndex], shaderStageFlags, nullptr };
    }

    // Constant buffers (uniforms)
    for (uint32_t cbIndex = 0; cbIndex < shaderBlob.cbvCount; ++cbIndex)
    {
        layoutBindings[numLayoutBindings++] = { shaderBlob.boundConstantBuffers[cbIndex], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            shaderBlob.boundConstantBufferCounts[cbIndex], shaderStageFlags, nullptr };
    }

    // Create the descriptor layout
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = numLayoutBindings;
    layoutInfo.pBindings = layoutBindings;

    if (backendContext->vkFunctionTable.vkCreateDescriptorSetLayout(backendContext->device, &layoutInfo, nullptr, &pPipelineLayout->descriptorSetLayout) != VK_SUCCESS) {
        return FFX_ERROR_BACKEND_API_ERROR;
    }

    // allocate descriptor sets
    pPipelineLayout->descriptorSetIndex = 0;
    for (uint32_t i = 0; i < (FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME); i++)
    {
        VkDescriptorSetAllocateInfo allocateInfo = {};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = backendContext->descriptorPool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &pPipelineLayout->descriptorSetLayout;

        if (backendContext->vkFunctionTable.vkAllocateDescriptorSets(backendContext->device, &allocateInfo, &pPipelineLayout->descriptorSets[i]) != VK_SUCCESS)
        {
            return FFX_ERROR_BACKEND_API_ERROR;
        }
    }

    uint32_t setCount = 0;

    VkDescriptorSetLayout layouts[5];

    layouts[setCount++] = pPipelineLayout->descriptorSetLayout;

    if (staticTextureSrvCount > 0)
    {
        layouts[pPipelineLayout->staticTextureSrvSet] = effectContext.bindlessTextureSrvDescriptorSetLayout;
        setCount++;
    }

    if (staticBufferSrvCount > 0)
    {
        layouts[pPipelineLayout->staticBufferSrvSet] = effectContext.bindlessBufferSrvDescriptorSetLayout;
        setCount++;
    }

    if (staticTextureUavCount > 0)
    {
        layouts[pPipelineLayout->staticTextureUavSet] = effectContext.bindlessTextureUavDescriptorSetLayout;
        setCount++;
    }

    if (staticBufferUavCount > 0)
    {
        layouts[pPipelineLayout->staticBufferUavSet] = effectContext.bindlessBufferUavDescriptorSetLayout;
        setCount++;
    }

    // create the pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = setCount;
    pipelineLayoutInfo.pSetLayouts            = layouts;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges    = nullptr;

    if (backendContext->vkFunctionTable.vkCreatePipelineLayout(backendContext->device, &pipelineLayoutInfo, nullptr, &pPipelineLayout->pipelineLayout) != VK_SUCCESS) {
        return FFX_ERROR_BACKEND_API_ERROR;
    }

    // set the root signature to pipeline
    outPipeline->rootSignature = reinterpret_cast<FfxRootSignature>(pPipelineLayout);

    // Only set the command signature if this is setup as an indirect workload
    if (pipelineDescription->indirectWorkload)
    {
        // Only need the stride ahead of time in Vulkan
        outPipeline->cmdSignature = reinterpret_cast<FfxCommandSignature>(sizeof(VkDispatchIndirectCommand));
    }
    else
    {
        outPipeline->cmdSignature = nullptr;
    }

    uint32_t flattenedSrvTextureCount = 0;

    for (uint32_t srvIndex = 0; srvIndex < shaderBlob.srvTextureCount; ++srvIndex)
    {
        uint32_t slotIndex = shaderBlob.boundSRVTextures[srvIndex];
        uint32_t bindCount = shaderBlob.boundSRVTextureCounts[srvIndex];

        for (uint32_t arrayIndex = 0; arrayIndex < bindCount; arrayIndex++)
        {
            uint32_t bindingIndex = flattenedSrvTextureCount++;

            outPipeline->srvTextureBindings[bindingIndex].slotIndex  = slotIndex;
            outPipeline->srvTextureBindings[bindingIndex].arrayIndex = arrayIndex;
            ConvertUTF8ToUTF16(shaderBlob.boundSRVTextureNames[srvIndex], outPipeline->srvTextureBindings[bindingIndex].name, FFX_RESOURCE_NAME_SIZE);
        }
    }

    outPipeline->srvTextureCount = flattenedSrvTextureCount;
    FFX_ASSERT(outPipeline->srvTextureCount < FFX_MAX_NUM_SRVS);

    uint32_t flattenedUavTextureCount = 0;

    for (uint32_t uavIndex = 0; uavIndex < shaderBlob.uavTextureCount; ++uavIndex)
    {
        uint32_t slotIndex = shaderBlob.boundUAVTextures[uavIndex];
        uint32_t bindCount = shaderBlob.boundUAVTextureCounts[uavIndex];

        for (uint32_t arrayIndex = 0; arrayIndex < bindCount; arrayIndex++)
        {
            uint32_t bindingIndex = flattenedUavTextureCount++;

            outPipeline->uavTextureBindings[bindingIndex].slotIndex  = slotIndex;
            outPipeline->uavTextureBindings[bindingIndex].arrayIndex = arrayIndex;
            ConvertUTF8ToUTF16(shaderBlob.boundUAVTextureNames[uavIndex], outPipeline->uavTextureBindings[bindingIndex].name, FFX_RESOURCE_NAME_SIZE);
        }
    }

    outPipeline->uavTextureCount = flattenedUavTextureCount;
    FFX_ASSERT(outPipeline->uavTextureCount < FFX_MAX_NUM_UAVS);

    uint32_t flattenedSrvBufferCount = 0;

    for (uint32_t srvIndex = 0; srvIndex < shaderBlob.srvBufferCount; ++srvIndex)
    {
        uint32_t slotIndex  = shaderBlob.boundSRVBuffers[srvIndex];
        uint32_t spaceIndex = shaderBlob.boundSRVBufferSpaces[srvIndex];
        uint32_t bindCount  = shaderBlob.boundSRVBufferCounts[srvIndex];

        // Skip static resources
        if (spaceIndex == 1)
            continue;

        for (uint32_t arrayIndex = 0; arrayIndex < bindCount; arrayIndex++)
        {
            uint32_t bindingIndex = flattenedSrvBufferCount++;

            outPipeline->srvBufferBindings[bindingIndex].slotIndex  = slotIndex;
            outPipeline->srvBufferBindings[bindingIndex].arrayIndex = arrayIndex;
            ConvertUTF8ToUTF16(shaderBlob.boundSRVBufferNames[srvIndex], outPipeline->srvBufferBindings[bindingIndex].name, FFX_RESOURCE_NAME_SIZE);
        }
    }

    outPipeline->srvBufferCount = flattenedSrvBufferCount;
    FFX_ASSERT(outPipeline->srvBufferCount < FFX_MAX_NUM_SRVS);

    uint32_t flattenedUavBufferCount = 0;

    for (uint32_t uavIndex = 0; uavIndex < shaderBlob.uavBufferCount; ++uavIndex)
    {
        uint32_t slotIndex = shaderBlob.boundUAVBuffers[uavIndex];
        uint32_t bindCount = shaderBlob.boundUAVBufferCounts[uavIndex];

        for (uint32_t arrayIndex = 0; arrayIndex < bindCount; arrayIndex++)
        {
            uint32_t bindingIndex = flattenedUavBufferCount++;

            outPipeline->uavBufferBindings[bindingIndex].slotIndex  = slotIndex;
            outPipeline->uavBufferBindings[bindingIndex].arrayIndex = arrayIndex;
            ConvertUTF8ToUTF16(shaderBlob.boundUAVBufferNames[uavIndex], outPipeline->uavBufferBindings[bindingIndex].name, FFX_RESOURCE_NAME_SIZE);
        }
    }

    outPipeline->uavBufferCount = flattenedUavBufferCount;
    FFX_ASSERT(outPipeline->uavBufferCount < FFX_MAX_NUM_UAVS);

    for (uint32_t cbIndex = 0; cbIndex < shaderBlob.cbvCount; ++cbIndex)
    {
        outPipeline->constantBufferBindings[cbIndex].slotIndex  = shaderBlob.boundConstantBuffers[cbIndex];
        outPipeline->constantBufferBindings[cbIndex].arrayIndex = 1;
        ConvertUTF8ToUTF16(shaderBlob.boundConstantBufferNames[cbIndex], outPipeline->constantBufferBindings[cbIndex].name, FFX_RESOURCE_NAME_SIZE);
    }

    outPipeline->constCount = shaderBlob.cbvCount;
    FFX_ASSERT(outPipeline->constCount < FFX_MAX_NUM_CONST_BUFFERS);

    outPipeline->staticTextureSrvCount = staticTextureSrvCount;
    FFX_ASSERT(outPipeline->staticTextureSrvCount <= effectContext.bindlessTextureSrvHeapSize);

    outPipeline->staticBufferSrvCount = staticBufferSrvCount;
    FFX_ASSERT(outPipeline->staticBufferSrvCount <= effectContext.bindlessBufferSrvHeapSize);

    outPipeline->staticTextureUavCount = staticTextureUavCount;
    FFX_ASSERT(outPipeline->staticTextureUavCount <= effectContext.bindlessTextureUavHeapSize);

    outPipeline->staticBufferUavCount = staticBufferUavCount;
    FFX_ASSERT(outPipeline->staticBufferUavCount <= effectContext.bindlessBufferUavHeapSize);

    // Todo when needed
    //outPipeline->samplerCount      = shaderBlob.samplerCount;
    //outPipeline->rtAccelStructCount= shaderBlob.rtAccelStructCount;

    //////////////////////////////////////////////////////////////////////////
    // pipeline creation
    FfxDeviceCapabilities capabilities;
    backendInterface->fpGetDeviceCapabilities(backendInterface, &capabilities);

    // shader module
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
    shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfo.pCode = (uint32_t*)shaderBlob.data;
    shaderModuleCreateInfo.codeSize = shaderBlob.size;

    if (backendContext->vkFunctionTable.vkCreateShaderModule(backendContext->device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return FFX_ERROR_BACKEND_API_ERROR;
    }

    // fill out shader stage create info
    VkPipelineShaderStageCreateInfo shaderStageCreateInfo = {};
    shaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageCreateInfo.pName = "main";
    shaderStageCreateInfo.module = shaderModule;

    // check if wave64 is requested
    bool isWave64 = false;
    ffxIsWave64(effect, permutationOptions, isWave64);
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroupSizeCreateInfo = {};
    if (isWave64 && (capabilities.waveLaneCountMin <= 64 && capabilities.waveLaneCountMax >= 64))
    {
        subgroupSizeCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT;
        subgroupSizeCreateInfo.requiredSubgroupSize = 64;
        shaderStageCreateInfo.pNext = &subgroupSizeCreateInfo;
    }

    // create the compute pipeline
    VkComputePipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stage = shaderStageCreateInfo;
    pipelineCreateInfo.layout = pPipelineLayout->pipelineLayout;

    VkPipeline computePipeline = VK_NULL_HANDLE;
    if (backendContext->vkFunctionTable.vkCreateComputePipelines(backendContext->device, nullptr, 1, &pipelineCreateInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        return FFX_ERROR_BACKEND_API_ERROR;
    }

    // done with shader module, so clean up
    backendContext->vkFunctionTable.vkDestroyShaderModule(backendContext->device, shaderModule, nullptr);

    // set the pipeline
    outPipeline->pipeline = reinterpret_cast<FfxPipeline>(computePipeline);

    // Setup the pipeline name
    wcscpy_s(outPipeline->name, pipelineDescription->name);

    return FFX_OK;
}

FfxErrorCode DestroyPipelineVK(FfxInterface* backendInterface, FfxPipelineState* pipeline, FfxUInt32 effectContextId)
{
    FFX_ASSERT(backendInterface != nullptr);
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];

    if (!pipeline)
        return FFX_OK;

    // Destroy the pipeline
    VkPipeline vkPipeline = reinterpret_cast<VkPipeline>(pipeline->pipeline);
    if (vkPipeline != VK_NULL_HANDLE) {
        backendContext->vkFunctionTable.vkDestroyPipeline(backendContext->device, vkPipeline, VK_NULL_HANDLE);
        pipeline->pipeline = VK_NULL_HANDLE;
    }

    // Zero out the cmd signature
    pipeline->cmdSignature = nullptr;

    // Destroy the pipeline layout
    BackendContext_VK::PipelineLayout* pPipelineLayout = reinterpret_cast<BackendContext_VK::PipelineLayout*>(pipeline->rootSignature);

    if (pPipelineLayout)
    {
        // Descriptor set layout
        if (pPipelineLayout->pipelineLayout != VK_NULL_HANDLE) {
            backendContext->vkFunctionTable.vkDestroyPipelineLayout(backendContext->device, pPipelineLayout->pipelineLayout, VK_NULL_HANDLE);
            pPipelineLayout->pipelineLayout = VK_NULL_HANDLE;
        }

        // Descriptor sets
        for (uint32_t i = 0; i < FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME; i++) {
            backendContext->vkFunctionTable.vkFreeDescriptorSets(backendContext->device, backendContext->descriptorPool, 1, &pPipelineLayout->descriptorSets[i]);
            pPipelineLayout->descriptorSets[i] = VK_NULL_HANDLE;
        }

        // Descriptor set layout
        if (pPipelineLayout->descriptorSetLayout != VK_NULL_HANDLE) {
            backendContext->vkFunctionTable.vkDestroyDescriptorSetLayout(backendContext->device, pPipelineLayout->descriptorSetLayout, VK_NULL_HANDLE);
            pPipelineLayout->descriptorSetLayout = VK_NULL_HANDLE;
        }

        // Samplers
        for (uint32_t currentSamplerIndex = 0; currentSamplerIndex < FFX_MAX_SAMPLERS; ++currentSamplerIndex)
        {
            if (pPipelineLayout->samplers[currentSamplerIndex] != VK_NULL_HANDLE) {
                backendContext->vkFunctionTable.vkDestroySampler(backendContext->device, pPipelineLayout->samplers[currentSamplerIndex], VK_NULL_HANDLE);
                pPipelineLayout->samplers[currentSamplerIndex] = VK_NULL_HANDLE;
            }
        }
    }

    return FFX_OK;
}

FfxErrorCode ScheduleGpuJobVK(FfxInterface* backendInterface, const FfxGpuJobDescription* job)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != job);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    FFX_ASSERT(backendContext->gpuJobCount < FFX_MAX_GPU_JOBS);

    backendContext->pGpuJobs[backendContext->gpuJobCount] = *job;
    backendContext->gpuJobCount++;

    return FFX_OK;
}

static FfxErrorCode executeGpuJobCompute(BackendContext_VK*    backendContext,
                                         FfxGpuJobDescription* job,
                                         VkCommandBuffer       vkCommandBuffer,
                                         FfxUInt32             effectContextId)
{
    BackendContext_VK::PipelineLayout* pipelineLayout = reinterpret_cast<BackendContext_VK::PipelineLayout*>(job->computeJobDescriptor.pipeline.rootSignature);

    // bind texture & buffer UAVs (note the binding order here MUST match the root signature mapping order from CreatePipeline!)
    uint32_t               descriptorWriteIndex = 0;
    VkWriteDescriptorSet   writeDescriptorSets[FFX_MAX_RESOURCE_COUNT];

    // These MUST be initialized
    uint32_t               imageDescriptorIndex = 0;
    VkDescriptorImageInfo  imageDescriptorInfos[FFX_MAX_RESOURCE_COUNT];
    for (int i = 0; i < FFX_MAX_RESOURCE_COUNT; ++i)
        imageDescriptorInfos[i] = { VK_NULL_HANDLE, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    // These MUST be initialized
    uint32_t               bufferDescriptorIndex = 0;
    VkDescriptorBufferInfo bufferDescriptorInfos[FFX_MAX_RESOURCE_COUNT];
    for (int i = 0; i < FFX_MAX_RESOURCE_COUNT; ++i)
        bufferDescriptorInfos[i] = { VK_NULL_HANDLE, 0, VK_WHOLE_SIZE };

    // bind texture UAVs
    for (uint32_t currentPipelineUavIndex = 0; currentPipelineUavIndex < job->computeJobDescriptor.pipeline.uavTextureCount; ++currentPipelineUavIndex)
    {
        FfxTextureUAV& textureUAV = job->computeJobDescriptor.uavTextures[currentPipelineUavIndex];

        // continue if this is a null resource.
        if (job->computeJobDescriptor.uavTextures[currentPipelineUavIndex].resource.internalIndex == 0)
            continue;

        addBarrier(backendContext, &textureUAV.resource, FFX_RESOURCE_STATE_UNORDERED_ACCESS);

        const FfxResourceBinding binding = job->computeJobDescriptor.pipeline.uavTextureBindings[currentPipelineUavIndex];

        // where to bind it
        const uint32_t currentUavResourceIndex = job->computeJobDescriptor.pipeline.uavTextureBindings[currentPipelineUavIndex].slotIndex;

        // source: UAV of resource to bind
        const uint32_t resourceIndex = textureUAV.resource.internalIndex;
        uint32_t mipOffset = textureUAV.mip;
        if (textureUAV.mip >= backendContext->pResources[resourceIndex].resourceDescription.mipCount)
            mipOffset = backendContext->pResources[resourceIndex].resourceDescription.mipCount - 1;
        const uint32_t uavViewIndex  = backendContext->pResources[resourceIndex].uavViewIndex + mipOffset;

        writeDescriptorSets[descriptorWriteIndex]                 = {};
        writeDescriptorSets[descriptorWriteIndex].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[descriptorWriteIndex].dstSet          = pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex];
        writeDescriptorSets[descriptorWriteIndex].descriptorCount = 1;
        writeDescriptorSets[descriptorWriteIndex].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeDescriptorSets[descriptorWriteIndex].pImageInfo      = &imageDescriptorInfos[imageDescriptorIndex];
        writeDescriptorSets[descriptorWriteIndex].dstBinding      = binding.slotIndex;
        writeDescriptorSets[descriptorWriteIndex].dstArrayElement = binding.arrayIndex;

        imageDescriptorInfos[imageDescriptorIndex]             = {};
        imageDescriptorInfos[imageDescriptorIndex].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageDescriptorInfos[imageDescriptorIndex].imageView   = backendContext->pResourceViews[uavViewIndex].imageView;

        imageDescriptorIndex++;
        descriptorWriteIndex++;
    }

    // bind buffer UAVs
    for (uint32_t currentPipelineUavIndex = 0; currentPipelineUavIndex < job->computeJobDescriptor.pipeline.uavBufferCount; ++currentPipelineUavIndex) 
    {
        FfxBufferUAV& bufferUAV = job->computeJobDescriptor.uavBuffers[currentPipelineUavIndex];

        // continue if this is a null resource.
        if (job->computeJobDescriptor.uavBuffers[currentPipelineUavIndex].resource.internalIndex == 0)
            continue;

        addBarrier(backendContext, &bufferUAV.resource, FFX_RESOURCE_STATE_UNORDERED_ACCESS);

        const FfxResourceBinding binding = job->computeJobDescriptor.pipeline.uavBufferBindings[currentPipelineUavIndex];

        // source: UAV of buffer to bind
        const uint32_t resourceIndex = bufferUAV.resource.internalIndex;

        // where to bind it
        const uint32_t currentUavResourceIndex = job->computeJobDescriptor.pipeline.uavBufferBindings[currentPipelineUavIndex].slotIndex;

        writeDescriptorSets[descriptorWriteIndex] = {};
        writeDescriptorSets[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[descriptorWriteIndex].dstSet = pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex];
        writeDescriptorSets[descriptorWriteIndex].descriptorCount = 1;
        writeDescriptorSets[descriptorWriteIndex].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeDescriptorSets[descriptorWriteIndex].pBufferInfo = &bufferDescriptorInfos[bufferDescriptorIndex];
        writeDescriptorSets[descriptorWriteIndex].dstBinding      = binding.slotIndex;
        writeDescriptorSets[descriptorWriteIndex].dstArrayElement = binding.arrayIndex;

        bufferDescriptorInfos[bufferDescriptorIndex] = {};
        bufferDescriptorInfos[bufferDescriptorIndex].buffer = backendContext->pResources[resourceIndex].bufferResource;
        bufferDescriptorInfos[bufferDescriptorIndex].offset = bufferUAV.offset;
        bufferDescriptorInfos[bufferDescriptorIndex].range  = bufferUAV.size > 0 ? bufferUAV.size : VK_WHOLE_SIZE;

        bufferDescriptorIndex++;
        descriptorWriteIndex++;
    }

    // bind texture SRVs
    for (uint32_t currentPipelineSrvIndex = 0; currentPipelineSrvIndex < job->computeJobDescriptor.pipeline.srvTextureCount; ++currentPipelineSrvIndex)
    {
        FfxTextureSRV& textureSRV = job->computeJobDescriptor.srvTextures[currentPipelineSrvIndex];

        // continue if this is a null resource.
        if (job->computeJobDescriptor.srvTextures[currentPipelineSrvIndex].resource.internalIndex == 0)
            continue;

        addBarrier(backendContext, &textureSRV.resource, FFX_RESOURCE_STATE_COMPUTE_READ);

        const FfxResourceBinding binding = job->computeJobDescriptor.pipeline.srvTextureBindings[currentPipelineSrvIndex];

        // where to bind it
        const uint32_t currentSrvResourceIndex = job->computeJobDescriptor.pipeline.srvTextureBindings[currentPipelineSrvIndex].slotIndex;

        writeDescriptorSets[descriptorWriteIndex]                 = {};
        writeDescriptorSets[descriptorWriteIndex].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[descriptorWriteIndex].dstSet          = pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex];
        writeDescriptorSets[descriptorWriteIndex].descriptorCount = 1;
        writeDescriptorSets[descriptorWriteIndex].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        writeDescriptorSets[descriptorWriteIndex].pImageInfo      = &imageDescriptorInfos[imageDescriptorIndex];
        writeDescriptorSets[descriptorWriteIndex].dstBinding      = binding.slotIndex;
        writeDescriptorSets[descriptorWriteIndex].dstArrayElement = binding.arrayIndex;

        const uint32_t resourceIndex = textureSRV.resource.internalIndex;
        const uint32_t srvViewIndex  = backendContext->pResources[resourceIndex].srvViewIndex;

        imageDescriptorInfos[imageDescriptorIndex]             = {};
        imageDescriptorInfos[imageDescriptorIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageDescriptorInfos[imageDescriptorIndex].imageView   = backendContext->pResourceViews[srvViewIndex].imageView;

        imageDescriptorIndex++;
        descriptorWriteIndex++;
    }

    // bind buffer SRVs
    for (uint32_t currentPipelineSrvIndex = 0; currentPipelineSrvIndex < job->computeJobDescriptor.pipeline.srvBufferCount; ++currentPipelineSrvIndex)
    {
        FfxBufferSRV& bufferSRV = job->computeJobDescriptor.srvBuffers[currentPipelineSrvIndex];

        // continue if this is a null resource.
        if (job->computeJobDescriptor.srvBuffers[currentPipelineSrvIndex].resource.internalIndex == 0)
            continue;

        addBarrier(backendContext, &bufferSRV.resource, FFX_RESOURCE_STATE_COMPUTE_READ);

        const FfxResourceBinding binding = job->computeJobDescriptor.pipeline.srvBufferBindings[currentPipelineSrvIndex];

        // source: SRV of buffer to bind
        const uint32_t resourceIndex = bufferSRV.resource.internalIndex;

        // where to bind it
        const uint32_t currentSrvResourceIndex = job->computeJobDescriptor.pipeline.srvBufferBindings[currentPipelineSrvIndex].slotIndex;

        writeDescriptorSets[descriptorWriteIndex]                 = {};
        writeDescriptorSets[descriptorWriteIndex].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[descriptorWriteIndex].dstSet          = pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex];
        writeDescriptorSets[descriptorWriteIndex].descriptorCount = 1;
        writeDescriptorSets[descriptorWriteIndex].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writeDescriptorSets[descriptorWriteIndex].pBufferInfo     = &bufferDescriptorInfos[bufferDescriptorIndex];
        writeDescriptorSets[descriptorWriteIndex].dstBinding      = binding.slotIndex;
        writeDescriptorSets[descriptorWriteIndex].dstArrayElement = binding.arrayIndex;

        bufferDescriptorInfos[bufferDescriptorIndex]        = {};
        bufferDescriptorInfos[bufferDescriptorIndex].buffer = backendContext->pResources[resourceIndex].bufferResource;
        bufferDescriptorInfos[bufferDescriptorIndex].offset = bufferSRV.offset;
        bufferDescriptorInfos[bufferDescriptorIndex].range  = bufferSRV.size > 0 ? bufferSRV.size : VK_WHOLE_SIZE;
    
        bufferDescriptorIndex++;
        descriptorWriteIndex++;
    }

    // update uniform buffers
    for (uint32_t currentRootConstantIndex = 0; currentRootConstantIndex < job->computeJobDescriptor.pipeline.constCount; ++currentRootConstantIndex)
    {
        uint32_t dataSize = job->computeJobDescriptor.cbs[currentRootConstantIndex].num32BitEntries * sizeof(uint32_t);
        
        // If we have a constant buffer allocator, use that, otherwise use the default backend allocator
        FfxConstantAllocation allocation;
        if (s_fpConstantAllocator)
            allocation = s_fpConstantAllocator(job->computeJobDescriptor.cbs[currentRootConstantIndex].data, dataSize);
        else
            allocation = backendContext->FallbackConstantAllocator(job->computeJobDescriptor.cbs[currentRootConstantIndex].data, dataSize);
            
        writeDescriptorSets[descriptorWriteIndex]                 = {};
        writeDescriptorSets[descriptorWriteIndex].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeDescriptorSets[descriptorWriteIndex].dstSet          = pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex];
        writeDescriptorSets[descriptorWriteIndex].descriptorCount = 1;
        writeDescriptorSets[descriptorWriteIndex].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeDescriptorSets[descriptorWriteIndex].pBufferInfo     = &bufferDescriptorInfos[bufferDescriptorIndex];
        writeDescriptorSets[descriptorWriteIndex].dstBinding =
            job->computeJobDescriptor.pipeline.constantBufferBindings[currentRootConstantIndex].slotIndex;
        writeDescriptorSets[descriptorWriteIndex].dstArrayElement = 0;

        bufferDescriptorInfos[bufferDescriptorIndex].buffer = static_cast<VkBuffer>(allocation.resource.resource);
        bufferDescriptorInfos[bufferDescriptorIndex].offset = static_cast<VkDeviceSize>(allocation.handle);
        bufferDescriptorInfos[bufferDescriptorIndex].range  = dataSize;

        bufferDescriptorIndex++;
        descriptorWriteIndex++;
    }

    // If we are dispatching indirectly, transition the argument resource to indirect argument
    if (job->computeJobDescriptor.pipeline.cmdSignature)
    {
        addBarrier(backendContext, &job->computeJobDescriptor.cmdArgument, FFX_RESOURCE_STATE_INDIRECT_ARGUMENT);
    }

    // insert all the barriers
    flushBarriers(backendContext, vkCommandBuffer);

    // update all uavs and srvs
    backendContext->vkFunctionTable.vkUpdateDescriptorSets(backendContext->device, descriptorWriteIndex, writeDescriptorSets, 0, nullptr);

    // bind pipeline
    backendContext->vkFunctionTable.vkCmdBindPipeline(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, reinterpret_cast<VkPipeline>(job->computeJobDescriptor.pipeline.pipeline));

    // bind descriptor sets
    {
        backendContext->vkFunctionTable.vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->pipelineLayout, 0, 1, &pipelineLayout->descriptorSets[pipelineLayout->descriptorSetIndex], 0, nullptr);

        BackendContext_VK::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];

        if (job->computeJobDescriptor.pipeline.staticTextureSrvCount > 0)
            backendContext->vkFunctionTable.vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->pipelineLayout, pipelineLayout->staticTextureSrvSet, 1, &effectContext.bindlessTextureSrvDescriptorSet, 0, nullptr);

        if (job->computeJobDescriptor.pipeline.staticBufferSrvCount > 0)
            backendContext->vkFunctionTable.vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->pipelineLayout, pipelineLayout->staticBufferSrvSet, 1, &effectContext.bindlessBufferSrvDescriptorSet, 0, nullptr);

        if (job->computeJobDescriptor.pipeline.staticTextureUavCount > 0)
            backendContext->vkFunctionTable.vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->pipelineLayout, pipelineLayout->staticTextureUavSet, 1, &effectContext.bindlessTextureUavDescriptorSet, 0, nullptr);

        if (job->computeJobDescriptor.pipeline.staticBufferUavCount > 0)
            backendContext->vkFunctionTable.vkCmdBindDescriptorSets(vkCommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout->pipelineLayout, pipelineLayout->staticBufferUavSet, 1, &effectContext.bindlessBufferUavDescriptorSet, 0, nullptr);
    }
    
    // Dispatch (or dispatch indirect)
    if (job->computeJobDescriptor.pipeline.cmdSignature)
    {
        const uint32_t resourceIndex = job->computeJobDescriptor.cmdArgument.internalIndex;
        VkBuffer buffer = backendContext->pResources[resourceIndex].bufferResource;
        backendContext->vkFunctionTable.vkCmdDispatchIndirect(vkCommandBuffer, buffer, job->computeJobDescriptor.cmdArgumentOffset);
    }
    else
    {
        backendContext->vkFunctionTable.vkCmdDispatch(vkCommandBuffer, job->computeJobDescriptor.dimensions[0], job->computeJobDescriptor.dimensions[1], job->computeJobDescriptor.dimensions[2]);
    }

    // move to another descriptor set for the next compute render job so that we don't overwrite descriptors in-use
    ++pipelineLayout->descriptorSetIndex;
    if (pipelineLayout->descriptorSetIndex >= (FFX_MAX_QUEUED_FRAMES * MAX_PIPELINE_USAGE_PER_FRAME))
        pipelineLayout->descriptorSetIndex = 0;

    return FFX_OK;
}

static FfxErrorCode executeGpuJobCopy(BackendContext_VK* backendContext, FfxGpuJobDescription* job, VkCommandBuffer vkCommandBuffer)
{
    BackendContext_VK::Resource ffxResourceSrc = backendContext->pResources[job->copyJobDescriptor.src.internalIndex];
    BackendContext_VK::Resource ffxResourceDst = backendContext->pResources[job->copyJobDescriptor.dst.internalIndex];

    addBarrier(backendContext, &job->copyJobDescriptor.src, FFX_RESOURCE_STATE_COPY_SRC);
    addBarrier(backendContext, &job->copyJobDescriptor.dst, FFX_RESOURCE_STATE_COPY_DEST);
    flushBarriers(backendContext, vkCommandBuffer);

    if (ffxResourceSrc.resourceDescription.type == FFX_RESOURCE_TYPE_BUFFER && ffxResourceDst.resourceDescription.type == FFX_RESOURCE_TYPE_BUFFER)
    {
        VkBuffer vkResourceSrc = ffxResourceSrc.bufferResource;
        VkBuffer vkResourceDst = ffxResourceDst.bufferResource;

        VkBufferCopy bufferCopy = {};

        bufferCopy.dstOffset = job->copyJobDescriptor.dstOffset;
        bufferCopy.srcOffset = job->copyJobDescriptor.srcOffset;
        bufferCopy.size      = job->copyJobDescriptor.size > 0 ? job->copyJobDescriptor.size : ffxResourceSrc.resourceDescription.width;

        backendContext->vkFunctionTable.vkCmdCopyBuffer(vkCommandBuffer, vkResourceSrc, vkResourceDst, 1, &bufferCopy);
    }
    else if (ffxResourceSrc.resourceDescription.type == FFX_RESOURCE_TYPE_BUFFER && ffxResourceDst.resourceDescription.type != FFX_RESOURCE_TYPE_BUFFER)
    {
        VkBuffer vkResourceSrc = ffxResourceSrc.bufferResource;
        VkImage vkResourceDst = ffxResourceDst.imageResource;

        VkImageSubresourceLayers subresourceLayers = {};

        subresourceLayers.aspectMask = FFX_CONTAINS_FLAG(ffxResourceDst.resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceLayers.baseArrayLayer = 0;
        subresourceLayers.layerCount = 1;
        subresourceLayers.mipLevel = 0;

        VkOffset3D offset = {};

        offset.x = 0;
        offset.y = 0;
        offset.z = 0;

        VkExtent3D extent = {};

        extent.width = ffxResourceDst.resourceDescription.width;
        extent.height = ffxResourceDst.resourceDescription.height;
        extent.depth = ffxResourceDst.resourceDescription.depth;

        // TODO: account for source buffer offset
        VkBufferImageCopy bufferImageCopy = {};

        bufferImageCopy.bufferOffset = 0;
        bufferImageCopy.bufferRowLength = 0;
        bufferImageCopy.bufferImageHeight = 0;
        bufferImageCopy.imageSubresource = subresourceLayers;
        bufferImageCopy.imageOffset = offset;
        bufferImageCopy.imageExtent = extent;

        backendContext->vkFunctionTable.vkCmdCopyBufferToImage(vkCommandBuffer, vkResourceSrc, vkResourceDst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);
    }
    else
    {
        bool isSrcDepth = FFX_CONTAINS_FLAG(ffxResourceSrc.resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET);
        bool isDstDepth = FFX_CONTAINS_FLAG(ffxResourceDst.resourceDescription.usage, FFX_RESOURCE_USAGE_DEPTHTARGET);
        FFX_ASSERT_MESSAGE(isSrcDepth == isDstDepth, "Copy operations aren't allowed between depth and color textures in the vulkan backend of the FFX SDK.");

        #define FFX_MAX_IMAGE_COPY_MIPS 14 // Will handle 4k down to 1x1
        VkImageCopy             imageCopies[FFX_MAX_IMAGE_COPY_MIPS];
        VkImage vkResourceSrc = ffxResourceSrc.imageResource;
        VkImage vkResourceDst = ffxResourceDst.imageResource;

        uint32_t numMipsToCopy = FFX_MINIMUM(ffxResourceSrc.resourceDescription.mipCount, ffxResourceDst.resourceDescription.mipCount);

        for (uint32_t mip = 0; mip < numMipsToCopy; mip++)
        {
            VkImageSubresourceLayers srcSubresourceLayers = {};

            srcSubresourceLayers.aspectMask     = isSrcDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            srcSubresourceLayers.baseArrayLayer = 0;
            srcSubresourceLayers.layerCount     = 1;
            srcSubresourceLayers.mipLevel       = mip;

            VkImageSubresourceLayers dstSubresourceLayers = {};

            dstSubresourceLayers.aspectMask     = isDstDepth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            dstSubresourceLayers.baseArrayLayer = 0;
            dstSubresourceLayers.layerCount     = 1;
            dstSubresourceLayers.mipLevel       = mip;

            VkOffset3D offset = {};

            offset.x = 0;
            offset.y = 0;
            offset.z = 0;

            VkExtent3D extent = {};

            extent.width = ffxResourceSrc.resourceDescription.width / (mip + 1);
            extent.height = ffxResourceSrc.resourceDescription.height / (mip + 1);
            extent.depth = ffxResourceSrc.resourceDescription.depth / (mip + 1);

            VkImageCopy& copyRegion = imageCopies[mip];

            copyRegion.srcSubresource = srcSubresourceLayers;
            copyRegion.srcOffset = offset;
            copyRegion.dstSubresource = dstSubresourceLayers;
            copyRegion.dstOffset = offset;
            copyRegion.extent = extent;
        }

        backendContext->vkFunctionTable.vkCmdCopyImage(vkCommandBuffer, vkResourceSrc, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vkResourceDst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, numMipsToCopy, imageCopies);
    }

    return FFX_OK;
}

static FfxErrorCode executeGpuJobBarrier(BackendContext_VK* backendContext, FfxGpuJobDescription* job, VkCommandBuffer vkCommandBuffer)
{
    addBarrier(backendContext, &job->barrierDescriptor.resource, job->barrierDescriptor.newState);
    flushBarriers(backendContext, vkCommandBuffer);

    return FFX_OK;
}

static FfxErrorCode executeGpuJobTimestamp(BackendContext_VK* backendContext, FfxGpuJobDescription* job, VkCommandBuffer vkCommandBuffer)
{
    return FFX_OK;
}

static FfxErrorCode executeGpuJobClearFloat(BackendContext_VK* backendContext, FfxGpuJobDescription* job, VkCommandBuffer vkCommandBuffer)
{
    uint32_t idx = job->clearJobDescriptor.target.internalIndex;
    BackendContext_VK::Resource ffxResource = backendContext->pResources[idx];

    if (ffxResource.resourceDescription.type == FFX_RESOURCE_TYPE_BUFFER)
    {
        addBarrier(backendContext, &job->clearJobDescriptor.target, FFX_RESOURCE_STATE_COPY_DEST);
        flushBarriers(backendContext, vkCommandBuffer);

        VkBuffer vkResource = ffxResource.bufferResource;

        backendContext->vkFunctionTable.vkCmdFillBuffer(vkCommandBuffer, vkResource, 0, VK_WHOLE_SIZE, (uint32_t)job->clearJobDescriptor.color[0]);
    }
    else
    {
        addBarrier(backendContext, &job->clearJobDescriptor.target, FFX_RESOURCE_STATE_COPY_DEST);
        flushBarriers(backendContext, vkCommandBuffer);

        VkImage vkResource = ffxResource.imageResource;

        VkClearColorValue clearColorValue = {};

        clearColorValue.float32[0] = job->clearJobDescriptor.color[0];
        clearColorValue.float32[1] = job->clearJobDescriptor.color[1];
        clearColorValue.float32[2] = job->clearJobDescriptor.color[2];
        clearColorValue.float32[3] = job->clearJobDescriptor.color[3];

        VkImageSubresourceRange range;
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = ffxResource.resourceDescription.mipCount;
        range.baseArrayLayer = 0;
        if (ffxResource.resourceDescription.type == FFX_RESOURCE_TYPE_TEXTURE3D)
            range.layerCount = 1;
        else
            range.layerCount = ffxResource.resourceDescription.depth;  // in that case depth is the number of layers
        
        backendContext->vkFunctionTable.vkCmdClearColorImage(vkCommandBuffer, vkResource, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue, 1, &range);
    }

    return FFX_OK;
}

FfxErrorCode ExecuteGpuJobsVK(FfxInterface* backendInterface, FfxCommandList commandList, FfxUInt32 effectContextId)
{
    FFX_ASSERT(nullptr != backendInterface);
    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;

    FFX_ASSERT(nullptr != commandList);
    VkCommandBuffer vkCommandBuffer = reinterpret_cast<VkCommandBuffer>(commandList);

    FfxErrorCode errorCode = FFX_OK;

    // execute all renderjobs
    for (uint32_t i = 0; i < backendContext->gpuJobCount; ++i)
    {
        FfxGpuJobDescription* gpuJob = &backendContext->pGpuJobs[i];

        // If we have a label for the job, drop a marker for it
        if (gpuJob->jobLabel[0]) {
            beginMarkerVK(backendContext, vkCommandBuffer, gpuJob->jobLabel);
        }

        
        switch (gpuJob->jobType)
        {
        case FFX_GPU_JOB_CLEAR_FLOAT:
        {
            errorCode = executeGpuJobClearFloat(backendContext, gpuJob, vkCommandBuffer);
            break;
        }
        case FFX_GPU_JOB_COPY:
        {
            errorCode = executeGpuJobCopy(backendContext, gpuJob, vkCommandBuffer);
            break;
        }
        case FFX_GPU_JOB_COMPUTE:
        {
            errorCode = executeGpuJobCompute(backendContext, gpuJob, vkCommandBuffer, effectContextId);
            break;
        }
        case FFX_GPU_JOB_BARRIER:
        {
            errorCode = executeGpuJobBarrier(backendContext, gpuJob, vkCommandBuffer);
            break;
        }
        default:;
        }

        if (gpuJob->jobLabel[0]) {
            endMarkerVK(backendContext, vkCommandBuffer);
        }
    }

    // check the execute function returned cleanly.
    FFX_RETURN_ON_ERROR(
        errorCode == FFX_OK,
        FFX_ERROR_BACKEND_API_ERROR);

    backendContext->gpuJobCount = 0;

    return FFX_OK;
}

FfxErrorCode BreadcrumbsAllocBlockVK(
    FfxInterface* backendInterface,
    uint64_t blockBytes,
    FfxBreadcrumbsBlockData* blockData)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != blockData);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    void* mappedMemory = nullptr;
    uint64_t baseAddress = 0;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.flags = 0;
    bufferInfo.size = blockBytes;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.queueFamilyIndexCount = 0;
    bufferInfo.pQueueFamilyIndices = nullptr;

    VkBuffer buffer = VK_NULL_HANDLE;
    if (backendContext->vkFunctionTable.vkCreateBuffer(backendContext->device, &bufferInfo, nullptr, &buffer) == VK_SUCCESS)
    {
        VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr };
        allocInfo.allocationSize = blockBytes;
        allocInfo.memoryTypeIndex = backendContext->breadcrumbsMemoryIndex;

        VkMemoryDedicatedAllocateInfo dedicatedAlloc = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, nullptr };
        if (FFX_CONTAINS_FLAG(backendContext->breadcrumbsFlags, BackendContext_VK::BREADCRUMBS_DEDICATED_MEMORY_ENABLED))
        {
            dedicatedAlloc.image = VK_NULL_HANDLE;
            dedicatedAlloc.buffer = buffer;
            dedicatedAlloc.pNext = allocInfo.pNext;
            allocInfo.pNext = &dedicatedAlloc;
        }

        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (backendContext->vkFunctionTable.vkAllocateMemory(backendContext->device, &allocInfo, nullptr, &memory) == VK_SUCCESS)
        {
            if (backendContext->vkFunctionTable.vkBindBufferMemory(backendContext->device, buffer, memory, 0) == VK_SUCCESS)
            {
                if (mappedMemory || backendContext->vkFunctionTable.vkMapMemory(backendContext->device, memory, 0, blockBytes, 0, &mappedMemory) == VK_SUCCESS)
                {
                    blockData->memory = mappedMemory;
                    blockData->heap = (void*)memory;
                    blockData->buffer = (void*)buffer;
                    blockData->baseAddress = baseAddress;
                    return FFX_OK;
                }
            }
            backendContext->vkFunctionTable.vkFreeMemory(backendContext->device, memory, nullptr);
        }
    }
    backendContext->vkFunctionTable.vkDestroyBuffer(backendContext->device, buffer, nullptr);
    return FFX_ERROR_BACKEND_API_ERROR;
}

void BreadcrumbsFreeBlockVK(
    FfxInterface* backendInterface,
    FfxBreadcrumbsBlockData* blockData)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != blockData);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    if (blockData->memory && blockData->baseAddress == 0)
    {
        backendContext->vkFunctionTable.vkUnmapMemory(backendContext->device, (VkDeviceMemory)blockData->heap);
        blockData->memory = nullptr;
    }
    if (blockData->buffer)
    {
        backendContext->vkFunctionTable.vkDestroyBuffer(backendContext->device, (VkBuffer)blockData->buffer, nullptr);
        blockData->buffer = nullptr;
    }
    if (blockData->heap)
    {
        backendContext->vkFunctionTable.vkFreeMemory(backendContext->device, (VkDeviceMemory)blockData->heap, nullptr);
        blockData->heap = nullptr;
    }
}

void BreadcrumbsWriteVK(
    FfxInterface* backendInterface,
    FfxCommandList commandList,
    uint32_t value,
    uint64_t gpuLocation,
    void* gpuBuffer,
    bool isBegin)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != gpuBuffer);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    if (FFX_CONTAINS_FLAG(backendContext->breadcrumbsFlags, BackendContext_VK::BREADCRUMBS_BUFFER_MARKER_ENABLED))
    {
        if (FFX_CONTAINS_FLAG(backendContext->breadcrumbsFlags, BackendContext_VK::BREADCRUMBS_SYNCHRONIZATION2_ENABLED))
        {
            backendContext->vkFunctionTable.vkCmdWriteBufferMarker2AMD((VkCommandBuffer)commandList,
                isBegin ? VK_PIPELINE_STAGE_2_NONE_KHR : VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
                (VkBuffer)gpuBuffer, gpuLocation, value);
        }
        else
        {
            backendContext->vkFunctionTable.vkCmdWriteBufferMarkerAMD((VkCommandBuffer)commandList,
                isBegin ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                (VkBuffer)gpuBuffer, gpuLocation, value);
        }
    }
    else
    {
        backendContext->vkFunctionTable.vkCmdFillBuffer((VkCommandBuffer)commandList, (VkBuffer)gpuBuffer, gpuLocation, sizeof(uint32_t), value);
    }
}

void BreadcrumbsPrintDeviceInfoVK(
    FfxInterface* backendInterface,
    FfxAllocationCallbacks* allocs,
    bool extendedInfo,
    char** printBuffer,
    size_t* printSize)
{
    FFX_ASSERT(NULL != backendInterface);
    FFX_ASSERT(NULL != allocs);
    FFX_ASSERT(NULL != printBuffer);
    FFX_ASSERT(NULL != printSize);

    BackendContext_VK* backendContext = (BackendContext_VK*)backendInterface->scratchBuffer;
    char* buff = *printBuffer;
    size_t buffSize = *printSize;

    VkPhysicalDeviceProperties devProps = {};
    VkPhysicalDeviceFeatures devFeatures = {};
    vkGetPhysicalDeviceProperties(backendContext->physicalDevice, &devProps);
    vkGetPhysicalDeviceFeatures(backendContext->physicalDevice, &devFeatures);

    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "[VkPhysicalDeviceProperties]\n" FFX_BREADCRUMBS_PRINTING_INDENT "apiVersion: ");
    FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, VK_API_VERSION_MAJOR(devProps.apiVersion));
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ".");
    FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, VK_API_VERSION_MINOR(devProps.apiVersion));
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ".");
    FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, VK_API_VERSION_PATCH(devProps.apiVersion));
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "\n");

    FFX_BREADCRUMBS_PRINT_HEX32(buff, buffSize, devProps, driverVersion);
    FFX_BREADCRUMBS_PRINT_HEX32(buff, buffSize, devProps, vendorID);
    FFX_BREADCRUMBS_PRINT_HEX32(buff, buffSize, devProps, deviceID);

    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT "deviceType: ");
    switch (devProps.deviceType)
    {
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    {
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "OTHER\n");
        break;
    }
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
    {
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "INTEGRATED_GPU\n");
        break;
    }
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
    {
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "DISCRETE_GPU\n");
        break;
    }
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
    {
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "VIRTUAL_GPU\n");
        break;
    }
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
    {
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "CPU\n");
        break;
    }
    default:
    {
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.deviceType);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "\n");
        break;
    }
    }

    FFX_BREADCRUMBS_PRINT_STRING(buff, buffSize, devProps, deviceName);
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT "pipelineCacheUUID: ");
    FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[0]);
    FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[1]);
    FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[2]);
    FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[3]);

    for (uint8_t i = 4; i < 12; i += 2)
    {
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "-");
        FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[i]);
        FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[i + 1]);
    }

    FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[12]);
    FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[13]);
    FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[14]);
    FFX_BREADCRUMBS_PRINT_HEX_BYTE(buff, buffSize, devProps.pipelineCacheUUID[15]);

    // Helper for printing device limits uint32_t.
#define BREAD_PRINT_LIMIT(name) \
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT); \
    FFX_BREADCRUMBS_PRINT_UINT(buff, buffSize, devProps.limits, name)
    // Helper for printing device limits uint64_t.
#define BREAD_PRINT_LIMIT64(name) \
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT); \
    FFX_BREADCRUMBS_PRINT_UINT64(buff, buffSize, devProps.limits, name)
    // Helper for printing device limits uint64_t.
#define BREAD_PRINT_LIMIT_FLOAT(name) \
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT); \
    FFX_BREADCRUMBS_PRINT_FLOAT(buff, buffSize, devProps.limits, name)
    // Helper for printing device limits as 32bit hexadecimal.
#define BREAD_PRINT_LIMIT_HEX(name) \
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT); \
    FFX_BREADCRUMBS_PRINT_HEX32(buff, buffSize, devProps.limits, name)

    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "\n" FFX_BREADCRUMBS_PRINTING_INDENT "limits: [VkPhysicalDeviceLimits]\n");
    BREAD_PRINT_LIMIT(maxImageDimension1D);
    BREAD_PRINT_LIMIT(maxImageDimension2D);
    BREAD_PRINT_LIMIT(maxImageDimension3D);
    BREAD_PRINT_LIMIT(maxImageDimensionCube);
    BREAD_PRINT_LIMIT(maxImageArrayLayers);
    BREAD_PRINT_LIMIT(maxTexelBufferElements);
    BREAD_PRINT_LIMIT(maxUniformBufferRange);
    BREAD_PRINT_LIMIT(maxStorageBufferRange);
    BREAD_PRINT_LIMIT(maxPushConstantsSize);
    BREAD_PRINT_LIMIT(maxMemoryAllocationCount);
    BREAD_PRINT_LIMIT(maxSamplerAllocationCount);
    BREAD_PRINT_LIMIT64(bufferImageGranularity);
    BREAD_PRINT_LIMIT64(sparseAddressSpaceSize);

    if (extendedInfo)
    {
        BREAD_PRINT_LIMIT(maxBoundDescriptorSets);
        BREAD_PRINT_LIMIT(maxPerStageDescriptorSamplers);
        BREAD_PRINT_LIMIT(maxPerStageDescriptorUniformBuffers);
        BREAD_PRINT_LIMIT(maxPerStageDescriptorStorageBuffers);
        BREAD_PRINT_LIMIT(maxPerStageDescriptorSampledImages);
        BREAD_PRINT_LIMIT(maxPerStageDescriptorStorageImages);
        BREAD_PRINT_LIMIT(maxPerStageDescriptorInputAttachments);
        BREAD_PRINT_LIMIT(maxPerStageResources);
        BREAD_PRINT_LIMIT(maxDescriptorSetSamplers);
        BREAD_PRINT_LIMIT(maxDescriptorSetUniformBuffers);
        BREAD_PRINT_LIMIT(maxDescriptorSetUniformBuffersDynamic);
        BREAD_PRINT_LIMIT(maxDescriptorSetStorageBuffers);
        BREAD_PRINT_LIMIT(maxDescriptorSetStorageBuffersDynamic);
        BREAD_PRINT_LIMIT(maxDescriptorSetSampledImages);
        BREAD_PRINT_LIMIT(maxDescriptorSetStorageImages);
        BREAD_PRINT_LIMIT(maxDescriptorSetInputAttachments);
        BREAD_PRINT_LIMIT(maxVertexInputAttributes);
        BREAD_PRINT_LIMIT(maxVertexInputBindings);
        BREAD_PRINT_LIMIT(maxVertexInputAttributeOffset);
        BREAD_PRINT_LIMIT(maxVertexInputBindingStride);
        BREAD_PRINT_LIMIT(maxVertexOutputComponents);
        BREAD_PRINT_LIMIT(maxTessellationGenerationLevel);
        BREAD_PRINT_LIMIT(maxTessellationPatchSize);
        BREAD_PRINT_LIMIT(maxTessellationControlPerVertexInputComponents);
        BREAD_PRINT_LIMIT(maxTessellationControlPerVertexOutputComponents);
        BREAD_PRINT_LIMIT(maxTessellationControlPerPatchOutputComponents);
        BREAD_PRINT_LIMIT(maxTessellationControlTotalOutputComponents);
        BREAD_PRINT_LIMIT(maxTessellationEvaluationInputComponents);
        BREAD_PRINT_LIMIT(maxTessellationEvaluationOutputComponents);
        BREAD_PRINT_LIMIT(maxGeometryShaderInvocations);
        BREAD_PRINT_LIMIT(maxGeometryInputComponents);
        BREAD_PRINT_LIMIT(maxGeometryOutputComponents);
        BREAD_PRINT_LIMIT(maxGeometryTotalOutputComponents);
        BREAD_PRINT_LIMIT(maxFragmentInputComponents);
        BREAD_PRINT_LIMIT(maxFragmentOutputAttachments);
        BREAD_PRINT_LIMIT(maxFragmentDualSrcAttachments);
        BREAD_PRINT_LIMIT(maxFragmentCombinedOutputResources);
        BREAD_PRINT_LIMIT(maxComputeSharedMemorySize);

        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT FFX_BREADCRUMBS_PRINTING_INDENT "maxComputeWorkGroupCount: [ ");
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.limits.maxComputeWorkGroupCount[0]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ", ");
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.limits.maxComputeWorkGroupCount[1]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ", ");
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.limits.maxComputeWorkGroupCount[2]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, " ]\n");

        BREAD_PRINT_LIMIT(maxComputeWorkGroupInvocations);

        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT FFX_BREADCRUMBS_PRINTING_INDENT "maxComputeWorkGroupSize: [ ");
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.limits.maxComputeWorkGroupSize[0]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ", ");
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.limits.maxComputeWorkGroupSize[1]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ", ");
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.limits.maxComputeWorkGroupSize[2]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, " ]\n");

        BREAD_PRINT_LIMIT(subPixelPrecisionBits);
        BREAD_PRINT_LIMIT(subTexelPrecisionBits);
        BREAD_PRINT_LIMIT(mipmapPrecisionBits);
        BREAD_PRINT_LIMIT(maxDrawIndexedIndexValue);
        BREAD_PRINT_LIMIT(maxDrawIndirectCount);
        BREAD_PRINT_LIMIT_FLOAT(maxSamplerLodBias);
        BREAD_PRINT_LIMIT_FLOAT(maxSamplerAnisotropy);
        BREAD_PRINT_LIMIT(maxViewports);

        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT FFX_BREADCRUMBS_PRINTING_INDENT "maxViewportDimensions: [ ");
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.limits.maxViewportDimensions[0]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ", ");
        FFX_BREADCRUMBS_APPEND_UINT(buff, buffSize, devProps.limits.maxViewportDimensions[1]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, " ]\n" FFX_BREADCRUMBS_PRINTING_INDENT FFX_BREADCRUMBS_PRINTING_INDENT "viewportBoundsRange: [ ");
        FFX_BREADCRUMBS_APPEND_FLOAT(buff, buffSize, devProps.limits.viewportBoundsRange[0]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ", ");
        FFX_BREADCRUMBS_APPEND_FLOAT(buff, buffSize, devProps.limits.viewportBoundsRange[1]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, " ]\n");

        BREAD_PRINT_LIMIT(viewportSubPixelBits);
        BREAD_PRINT_LIMIT64(minMemoryMapAlignment);
        BREAD_PRINT_LIMIT64(minMemoryMapAlignment);
        BREAD_PRINT_LIMIT64(minTexelBufferOffsetAlignment);
        BREAD_PRINT_LIMIT64(minUniformBufferOffsetAlignment);
        BREAD_PRINT_LIMIT64(minStorageBufferOffsetAlignment);
        BREAD_PRINT_LIMIT(minTexelOffset);
        BREAD_PRINT_LIMIT(maxTexelOffset);
        BREAD_PRINT_LIMIT(minTexelGatherOffset);
        BREAD_PRINT_LIMIT(maxTexelGatherOffset);
        BREAD_PRINT_LIMIT_FLOAT(minInterpolationOffset);
        BREAD_PRINT_LIMIT_FLOAT(maxInterpolationOffset);
        BREAD_PRINT_LIMIT(subPixelInterpolationOffsetBits);
        BREAD_PRINT_LIMIT(maxFramebufferWidth);
        BREAD_PRINT_LIMIT(maxFramebufferHeight);
        BREAD_PRINT_LIMIT(maxFramebufferLayers);
        BREAD_PRINT_LIMIT_HEX(framebufferColorSampleCounts);
        BREAD_PRINT_LIMIT_HEX(framebufferDepthSampleCounts);
        BREAD_PRINT_LIMIT_HEX(framebufferStencilSampleCounts);
        BREAD_PRINT_LIMIT_HEX(framebufferNoAttachmentsSampleCounts);
        BREAD_PRINT_LIMIT(maxColorAttachments);
        BREAD_PRINT_LIMIT_HEX(sampledImageColorSampleCounts);
        BREAD_PRINT_LIMIT_HEX(sampledImageIntegerSampleCounts);
        BREAD_PRINT_LIMIT_HEX(sampledImageDepthSampleCounts);
        BREAD_PRINT_LIMIT_HEX(sampledImageStencilSampleCounts);
        BREAD_PRINT_LIMIT_HEX(storageImageSampleCounts);
        BREAD_PRINT_LIMIT(maxSampleMaskWords);

        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devProps.limits, timestampComputeAndGraphics);

        BREAD_PRINT_LIMIT_FLOAT(timestampPeriod);
        BREAD_PRINT_LIMIT(maxClipDistances);
        BREAD_PRINT_LIMIT(maxCullDistances);
        BREAD_PRINT_LIMIT(maxCombinedClipAndCullDistances);
        BREAD_PRINT_LIMIT(discreteQueuePriorities);

        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT FFX_BREADCRUMBS_PRINTING_INDENT "pointSizeRange: [ ");
        FFX_BREADCRUMBS_APPEND_FLOAT(buff, buffSize, devProps.limits.pointSizeRange[0]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ", ");
        FFX_BREADCRUMBS_APPEND_FLOAT(buff, buffSize, devProps.limits.pointSizeRange[1]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, " ]\n" FFX_BREADCRUMBS_PRINTING_INDENT FFX_BREADCRUMBS_PRINTING_INDENT "lineWidthRange: [ ");
        FFX_BREADCRUMBS_APPEND_FLOAT(buff, buffSize, devProps.limits.lineWidthRange[0]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, ", ");
        FFX_BREADCRUMBS_APPEND_FLOAT(buff, buffSize, devProps.limits.lineWidthRange[1]);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, " ]\n");

        BREAD_PRINT_LIMIT_FLOAT(pointSizeGranularity);
        BREAD_PRINT_LIMIT_FLOAT(lineWidthGranularity);

        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devProps.limits, strictLines);
        FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devProps.limits, standardSampleLocations);

        BREAD_PRINT_LIMIT64(optimalBufferCopyOffsetAlignment);
        BREAD_PRINT_LIMIT64(optimalBufferCopyRowPitchAlignment);
        BREAD_PRINT_LIMIT64(nonCoherentAtomSize);
    }
#undef BREAD_PRINT_LIMIT
#undef BREAD_PRINT_LIMIT64
#undef BREAD_PRINT_LIMIT_HEX

    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT "sparseProperties: [VkPhysicalDeviceSparseProperties]\n" FFX_BREADCRUMBS_PRINTING_INDENT);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devProps.sparseProperties, residencyStandard2DBlockShape);
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devProps.sparseProperties, residencyStandard2DMultisampleBlockShape);
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devProps.sparseProperties, residencyStandard3DBlockShape);
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devProps.sparseProperties, residencyAlignedMipSize);
    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, FFX_BREADCRUMBS_PRINTING_INDENT);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devProps.sparseProperties, residencyNonResidentStrict);

    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "[VkPhysicalDeviceFeatures]\n");
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, robustBufferAccess);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, fullDrawIndexUint32);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, imageCubeArray);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, independentBlend);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, geometryShader);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, tessellationShader);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sampleRateShading);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, dualSrcBlend);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, logicOp);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, multiDrawIndirect);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, drawIndirectFirstInstance);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, depthClamp);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, depthBiasClamp);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, fillModeNonSolid);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, depthBounds);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, wideLines);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, largePoints);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, alphaToOne);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, multiViewport);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, samplerAnisotropy);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, textureCompressionETC2);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, textureCompressionASTC_LDR);
    FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, textureCompressionBC);

    if (extendedInfo)
    {
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, occlusionQueryPrecise);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, pipelineStatisticsQuery);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, vertexPipelineStoresAndAtomics);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, fragmentStoresAndAtomics);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderTessellationAndGeometryPointSize);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderImageGatherExtended);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderStorageImageExtendedFormats);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderStorageImageMultisample);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderStorageImageReadWithoutFormat);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderStorageImageWriteWithoutFormat);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderUniformBufferArrayDynamicIndexing);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderSampledImageArrayDynamicIndexing);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderStorageBufferArrayDynamicIndexing);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderStorageImageArrayDynamicIndexing);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderClipDistance);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderCullDistance);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderFloat64);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderInt64);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderInt16);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderResourceResidency);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, shaderResourceMinLod);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseBinding);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseResidencyBuffer);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseResidencyImage2D);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseResidencyImage3D);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseResidency2Samples);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseResidency4Samples);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseResidency8Samples);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseResidency16Samples);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, sparseResidencyAliased);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, variableMultisampleRate);
        FFX_BREADCRUMBS_PRINT_BOOL(buff, buffSize, devFeatures, inheritedQueries);
    }

    FFX_BREADCRUMBS_APPEND_STRING(buff, buffSize, "\n");
    *printBuffer = buff;
    *printSize = buffSize;
}

void RegisterConstantBufferAllocatorVK(FfxInterface*, FfxConstantBufferAllocator fpConstantAllocator)
{
    s_fpConstantAllocator = fpConstantAllocator;
}

FfxCommandQueue ffxGetCommandQueueVK(VkQueue commandQueue)
{
    FFX_ASSERT(commandQueue != VK_NULL_HANDLE);
    return reinterpret_cast<FfxCommandQueue>(commandQueue);
}

FfxSwapchain ffxGetSwapchainVK(VkSwapchainKHR swapchain)
{
    FFX_ASSERT(swapchain != VK_NULL_HANDLE);
    return reinterpret_cast<FfxSwapchain>(swapchain);
}

VkSwapchainKHR ffxGetVKSwapchain(FfxSwapchain ffxSwapchain)
{
    return reinterpret_cast<VkSwapchainKHR>(ffxSwapchain);
}
