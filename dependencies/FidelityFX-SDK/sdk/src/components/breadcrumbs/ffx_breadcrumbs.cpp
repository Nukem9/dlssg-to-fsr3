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

#include <cstring>     // for memset

#include <ffx_object_management.h>           
#include <ffx_breadcrumbs_list.h>           

#include "ffx_breadcrumbs_private.h"

static const char* breadDecodeMarkerType(FfxBreadcrumbsMarkerType type)
{
#define X(marker) case FFX_BREADCRUMBS_MARKER_##marker: return #marker;
    switch (type)
    {
    default:
        FFX_ASSERT_FAIL("Unhandled enum value!");
        FFX_BREADCRUMBS_MARKER_LIST
    }
#undef X
}

static void breadcrumbsSetName(FfxAllocationCallbacks* allocs, BreadcrumbsCustomNameBuffer* nameBuffer, const FfxBreadcrumbsNameTag* tag, bool enableLock, BreadcrumbsCustomName* name)
{
    FFX_ASSERT(nameBuffer);
    FFX_ASSERT(tag);
    FFX_ASSERT(name);

    // If enabled only copy pointer to the name tag if present.
    if (tag->pName && !tag->isNameExternallyOwned)
    {
        const size_t length = strlen(tag->pName) + 1;

        if (enableLock)
        {
            FFX_MUTEX_LOCK(nameBuffer->mutex);
        }

        const size_t nameOffset = nameBuffer->currentNamesOffset;
        nameBuffer->currentNamesOffset += length;
        if (nameBuffer->bufferSize < nameBuffer->currentNamesOffset)
        {
            nameBuffer->pBuffer = (char*)ffxBreadcrumbsAppendList(nameBuffer->pBuffer,
                nameBuffer->bufferSize, 1, nameBuffer->currentNamesOffset - nameBuffer->bufferSize, allocs);
        }
        memcpy(nameBuffer->pBuffer + nameOffset, tag->pName, length);

        if (enableLock)
        {
            FFX_MUTEX_UNLOCK(nameBuffer->mutex);
        }

        name->pName = (char*)(nameOffset + 1);
        name->isCopied = true;
    }
    else
    {
        name->pName = (char*)tag->pName;
        name->isCopied = false;
    }
}

static char* breadcrumbsGetName(BreadcrumbsCustomNameBuffer* nameBuffer, const BreadcrumbsCustomName* name)
{
    // No need for lock
    FFX_ASSERT(nameBuffer);
    FFX_ASSERT(name);
    FFX_ASSERT(name->pName);

    if (name->isCopied)
        return nameBuffer->pBuffer + (size_t)name->pName - 1;
    return name->pName;
}

static BreadcrumbsListData* breadcrumbsSearchList(BreadcrumbsFrameData* frame, FfxCommandList list)
{
    // Lock placed externally
    FFX_ASSERT(frame);
    FFX_ASSERT(list);
    for (size_t i = 0; i < frame->usedListsCount; ++i)
    {
        if (frame->pUsedLists[i].list == list)
            return frame->pUsedLists + i;
    }
    return nullptr;
}

static BreadcrumbsPipelineData* breadcrumbsSearchPipeline(FfxBreadcrumbsContext_Private* context, FfxPipeline pipeline)
{
    // Lock placed externally
    FFX_ASSERT(context);
    FFX_ASSERT(pipeline);
    for (size_t i = 0; i < context->registeredPipelinesCount; ++i)
    {
        if (context->pRegisteredPipelines[i].pipeline == pipeline)
            return context->pRegisteredPipelines + i;
    }
    return nullptr;
}

static bool breadcrumbsIsCorrectPipeline(FfxBreadcrumbsContext_Private* context, FfxPipeline pipeline, bool newPipeline)
{
    if (pipeline == nullptr)
        return !newPipeline;
    if (FFX_CONTAINS_FLAG(context->contextDescription.flags, FFX_BREADCRUMBS_PRINT_SKIP_PIPELINE_INFO))
        return true;

    const bool lockEnable = FFX_CONTAINS_FLAG(context->contextDescription.flags, FFX_BREADCRUMBS_ENABLE_THREAD_SYNCHRONIZATION);
    if (lockEnable)
    {
        FFX_MUTEX_LOCK_SHARED(context->pipelinesNamesBuffer.mutex);
    }
    BreadcrumbsPipelineData* data = breadcrumbsSearchPipeline(context, pipeline);
    if (lockEnable)
    {
        FFX_MUTEX_UNLOCK_SHARED(context->pipelinesNamesBuffer.mutex);
    }

    return (data == nullptr) == newPipeline;
}

static BreadcrumbsFrameData* breadcrumbsGetCurrentFrame(FfxBreadcrumbsContext_Private* context)
{
    // Lock placed externally.
    FFX_ASSERT(context);
    FFX_ASSERT(context->pFrameData);
    return context->pFrameData + (context->frameIndex % context->contextDescription.frameHistoryLength);
}

static FfxBreadcrumbsBlockData* breadcrumbsGetLastBlock(BreadcrumbsBlockVector* blockVector)
{
    // Lock placed externally.
    FFX_ASSERT(blockVector);
    FFX_ASSERT(blockVector->currentBlock < blockVector->memoryBlocksCount);
    return blockVector->pMemoryBlocks + blockVector->currentBlock;
}

static FfxBreadcrumbsBlockData* breadcrumbsGetQueueLastBlock(BreadcrumbsFrameData* frame, uint32_t queue)
{
    // Lock placed externally.
    FFX_ASSERT(frame);
    return breadcrumbsGetLastBlock(frame->pBlockPerQueue + queue);
}

static FfxErrorCode breadcrumbsAllocBlock(FfxInterface* ptr, FfxAllocationCallbacks* allocs,
    BreadcrumbsBlockVector* blockVector, uint32_t markersPerBlock)
{
    // Lock placed externally.
    FFX_ASSERT(ptr);
    FFX_ASSERT(blockVector);

    FfxBreadcrumbsBlockData newBlock;
    FfxErrorCode            errorCode = ptr->fpBreadcrumbsAllocBlock(ptr, 4ULL * markersPerBlock, &newBlock);
    if (errorCode != FFX_OK)
        return errorCode;

    blockVector->pMemoryBlocks = (FfxBreadcrumbsBlockData*)ffxBreadcrumbsAppendList(blockVector->pMemoryBlocks,
        blockVector->memoryBlocksCount, sizeof(FfxBreadcrumbsBlockData), 1, allocs);
    blockVector->pMemoryBlocks[blockVector->memoryBlocksCount] = newBlock;
    ++blockVector->memoryBlocksCount;

    return FFX_OK;
}

static FfxErrorCode breadcrumbsRelease(FfxBreadcrumbsContext_Private* context)
{
    // Not protected by lock, should be called from single thread only!
    FFX_ASSERT(context);

    FFX_SAFE_FREE(context->contextDescription.pUsedGpuQueues, context->contextDescription.allocCallbacks.fpFree);
    if (context->pFrameData)
    {
        for (uint32_t f = 0; f < context->contextDescription.frameHistoryLength; ++f)
        {
            BreadcrumbsFrameData* frame = context->pFrameData + f;
            for (uint32_t list = 0; list < frame->usedListsCount; ++list)
            {
                FFX_SAFE_FREE(frame->pUsedLists->pMarkers, context->contextDescription.allocCallbacks.fpFree);
            }
            FFX_SAFE_FREE(frame->pUsedLists, context->contextDescription.allocCallbacks.fpFree);

            for (uint32_t queue = 0; queue < context->contextDescription.usedGpuQueuesCount; ++queue)
            {
                if (frame->pBlockPerQueue)
                {
                    BreadcrumbsBlockVector* blockVector = frame->pBlockPerQueue + queue;
                    for (uint32_t block = 0; block < blockVector->memoryBlocksCount; ++block)
                    {
                        FFX_ASSERT(blockVector->pMemoryBlocks);

                        context->contextDescription.backendInterface.fpBreadcrumbsFreeBlock(&context->contextDescription.backendInterface, blockVector->pMemoryBlocks + block);
                        // All data should be cleared at this point
                        FFX_ASSERT(!blockVector->pMemoryBlocks[block].buffer);
                        FFX_ASSERT(!blockVector->pMemoryBlocks[block].heap);
                        FFX_ASSERT(!blockVector->pMemoryBlocks[block].memory);
                    }
                    FFX_SAFE_FREE(blockVector->pMemoryBlocks, context->contextDescription.allocCallbacks.fpFree);
                }
            }
            FFX_SAFE_FREE(frame->pBlockPerQueue, context->contextDescription.allocCallbacks.fpFree);
            FFX_SAFE_FREE(frame->namesBuffer.pBuffer, context->contextDescription.allocCallbacks.fpFree);
            frame->~BreadcrumbsFrameData();
        }
        context->contextDescription.allocCallbacks.fpFree(context->pFrameData);
    }
    FFX_SAFE_FREE(context->pRegisteredPipelines, context->contextDescription.allocCallbacks.fpFree);
    FFX_SAFE_FREE(context->pipelinesNamesBuffer.pBuffer, context->contextDescription.allocCallbacks.fpFree);

    // Destroy the context
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);
    context->~FfxBreadcrumbsContext_Private();

    return FFX_OK;
}

static FfxErrorCode breadcrumbsCreate(FfxBreadcrumbsContext_Private* context, const FfxBreadcrumbsContextDescription* contextDescription)
{
    // Not protected by lock, should be called from single thread only!
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    new(context) FfxBreadcrumbsContext_Private();
    context->frameIndex = UINT32_MAX;
    memcpy(&context->contextDescription, contextDescription, sizeof(FfxBreadcrumbsContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);
    
    // Create the context.
    FfxErrorCode errorCode = context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_BREADCRUMBS, nullptr, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // Save GPU queues indetifiers
    context->contextDescription.pUsedGpuQueues = (uint32_t*)context->contextDescription.allocCallbacks.fpAlloc(sizeof(uint32_t) * contextDescription->usedGpuQueuesCount);
    FFX_ASSERT(context->contextDescription.pUsedGpuQueues);
    memcpy(context->contextDescription.pUsedGpuQueues, contextDescription->pUsedGpuQueues, sizeof(uint32_t) * contextDescription->usedGpuQueuesCount);

    context->pFrameData = (BreadcrumbsFrameData*)context->contextDescription.allocCallbacks.fpAlloc(sizeof(BreadcrumbsFrameData) * contextDescription->frameHistoryLength);
    FFX_ASSERT(context->pFrameData);

    // Alloc one initial block for every frame in flight
    for (uint32_t frame = 0; frame < contextDescription->frameHistoryLength; ++frame)
    {
        new(context->pFrameData + frame) BreadcrumbsFrameData();
        context->pFrameData[frame].pBlockPerQueue = (BreadcrumbsBlockVector*)context->contextDescription.allocCallbacks.fpAlloc(sizeof(BreadcrumbsBlockVector) * contextDescription->usedGpuQueuesCount);
        FFX_ASSERT(context->pFrameData[frame].pBlockPerQueue);
        for (uint32_t queue = 0; queue < contextDescription->usedGpuQueuesCount; ++queue)
        {
            context->pFrameData[frame].pBlockPerQueue[queue] = {};
            errorCode = breadcrumbsAllocBlock(&context->contextDescription.backendInterface, &context->contextDescription.allocCallbacks, context->pFrameData[frame].pBlockPerQueue + queue, contextDescription->maxMarkersPerMemoryBlock);
            if (errorCode != FFX_OK)
            {
                breadcrumbsRelease(context);
                return errorCode;
            }
        }
    }

    return FFX_OK;
}

FfxErrorCode ffxBreadcrumbsContextCreate(FfxBreadcrumbsContext* context, const FfxBreadcrumbsContextDescription* contextDescription)
{
    // No need for lock.

    // Zero context memory
    memset(context, 0, sizeof(FfxBreadcrumbsContext));

    // Check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextDescription, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextDescription->pUsedGpuQueues, FFX_ERROR_INVALID_POINTER);

    // Check if parameters are valid.
    FFX_RETURN_ON_ERROR(contextDescription->frameHistoryLength != 0, FFX_ERROR_INVALID_ARGUMENT);
    FFX_RETURN_ON_ERROR(contextDescription->maxMarkersPerMemoryBlock != 0, FFX_ERROR_INVALID_ARGUMENT);
    FFX_RETURN_ON_ERROR(contextDescription->maxMarkersPerMemoryBlock <= FFX_BREADCRUMBS_MAX_MARKERS_PER_BLOCK, FFX_ERROR_INVALID_ARGUMENT);
    FFX_RETURN_ON_ERROR(contextDescription->usedGpuQueuesCount > 0, FFX_ERROR_INVALID_ARGUMENT);

    // Check if flag combinations are valid.
    if (FFX_CONTAINS_FLAG(contextDescription->flags, FFX_BREADCRUMBS_PRINT_EXTENDED_DEVICE_INFO)
        && FFX_CONTAINS_FLAG(contextDescription->flags, FFX_BREADCRUMBS_PRINT_SKIP_DEVICE_INFO))
    {
        return FFX_ERROR_INVALID_ENUM;
    }

    // Validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpBreadcrumbsAllocBlock, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpBreadcrumbsFreeBlock, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpBreadcrumbsWrite, FFX_ERROR_INCOMPLETE_INTERFACE);

    // If requesting GPU info, then we must have proper callback for that
    if ((contextDescription->flags & FFX_BREADCRUMBS_PRINT_SKIP_DEVICE_INFO) == 0)
    {
        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpBreadcrumbsPrintDeviceInfo, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // If a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer)
    {
        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // Ensure the context is large enough for the internal context.
    FFX_STATIC_ASSERT(sizeof(FfxBreadcrumbsContext) >= sizeof(FfxBreadcrumbsContext_Private));

    // Create the context.
    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    const FfxErrorCode errorCode = breadcrumbsCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxBreadcrumbsContextDestroy(FfxBreadcrumbsContext* context)
{
    // Not protected by lock, should be called from single thread only!
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // Destroy the context.
    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    const FfxErrorCode errorCode = breadcrumbsRelease(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxBreadcrumbsStartFrame(FfxBreadcrumbsContext* context)
{
    // Not protected by lock, should be called from single thread only!
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // Move to next frame.
    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    FFX_RETURN_ON_ERROR(contextPrivate->pFrameData, FFX_ERROR_INVALID_POINTER);

    ++contextPrivate->frameIndex;
    BreadcrumbsFrameData* frame = breadcrumbsGetCurrentFrame(contextPrivate);
    frame->namesBuffer.currentNamesOffset = 0;
    for (uint32_t list = 0; list < frame->usedListsCount; ++list)
    {
        FFX_SAFE_FREE(frame->pUsedLists->pMarkers, contextPrivate->contextDescription.allocCallbacks.fpFree);
    }
    FFX_SAFE_FREE(frame->pUsedLists, contextPrivate->contextDescription.allocCallbacks.fpFree);
    frame->usedListsCount = 0;

    for (uint32_t queue = 0; queue < contextPrivate->contextDescription.usedGpuQueuesCount; ++queue)
    {
        BreadcrumbsBlockVector* blockVector = frame->pBlockPerQueue + queue;
        blockVector->currentBlock = 0;
        if (blockVector->memoryBlocksCount)
            breadcrumbsGetLastBlock(blockVector)->nextMarker = 0;
    }
    return FFX_OK;
}

FfxErrorCode ffxBreadcrumbsRegisterCommandList(FfxBreadcrumbsContext* context, const FfxBreadcrumbsCommandListDescription* commandListDescription)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(commandListDescription, FFX_ERROR_INVALID_POINTER);

    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    FFX_RETURN_ON_ERROR(breadcrumbsIsCorrectPipeline(contextPrivate, commandListDescription->pipeline, false), FFX_ERROR_INVALID_ARGUMENT);
    FFX_RETURN_ON_ERROR(commandListDescription->queueType < contextPrivate->contextDescription.usedGpuQueuesCount, FFX_ERROR_INVALID_ARGUMENT);

    BreadcrumbsFrameData* frame = breadcrumbsGetCurrentFrame(contextPrivate);
    BreadcrumbsCustomName name;

    const bool lockEnable = FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_ENABLE_THREAD_SYNCHRONIZATION);
    breadcrumbsSetName(&contextPrivate->contextDescription.allocCallbacks, &frame->namesBuffer, &commandListDescription->name, lockEnable, &name);

    if (lockEnable)
    {
        FFX_MUTEX_LOCK(frame->listMutex);
    }
    if (breadcrumbsSearchList(frame, commandListDescription->commandList))
    {
        if (lockEnable)
        {
            FFX_MUTEX_UNLOCK(frame->listMutex);
        }
        return FFX_ERROR_INVALID_ARGUMENT;
    }
    frame->pUsedLists = (BreadcrumbsListData*)ffxBreadcrumbsAppendList(frame->pUsedLists, frame->usedListsCount, sizeof(BreadcrumbsListData), 1, &contextPrivate->contextDescription.allocCallbacks);
    frame->pUsedLists[frame->usedListsCount] =
    {
        commandListDescription->commandList,
        commandListDescription->queueType,
        commandListDescription->submissionIndex,
        name,
        FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_SKIP_PIPELINE_INFO) ? nullptr : commandListDescription->pipeline,
        0,
        nullptr,
        0,
        nullptr
    };
    ++frame->usedListsCount;
    if (lockEnable)
    {
        FFX_MUTEX_UNLOCK(frame->listMutex);
    }
    return FFX_OK;
}

FfxErrorCode ffxBreadcrumbsRegisterPipeline(FfxBreadcrumbsContext* context, const FfxBreadcrumbsPipelineStateDescription* pipelineDescription)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(pipelineDescription, FFX_ERROR_INVALID_POINTER);

    // Correct shader combinations checks
    const bool compute = pipelineDescription->computeShader.pName != nullptr;
    const bool rayTracing = pipelineDescription->rayTracingShader.pName != nullptr;
    const bool legacyGeometry = pipelineDescription->vertexShader.pName != nullptr
        || pipelineDescription->hullShader.pName != nullptr
        || pipelineDescription->domainShader.pName != nullptr
        || pipelineDescription->geometryShader.pName != nullptr;
    const bool meshShading = pipelineDescription->meshShader.pName != nullptr
        || pipelineDescription->amplificationShader.pName != nullptr;
    const bool graphics = legacyGeometry || meshShading || pipelineDescription->pixelShader.pName != nullptr;

    // Cannot pass compute shader name when graphics or ray tracing shaders are used.
    FFX_RETURN_ON_ERROR((!compute && !rayTracing) || (!compute && !graphics) || (!rayTracing && !graphics),
        FFX_ERROR_INVALID_ARGUMENT);
    // Cannot use vertex shading with mesh shading pipeline
    FFX_RETURN_ON_ERROR(!(legacyGeometry && meshShading),
        FFX_ERROR_INVALID_ARGUMENT);

    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    FFX_RETURN_ON_ERROR(breadcrumbsIsCorrectPipeline(contextPrivate, pipelineDescription->pipeline, true), FFX_ERROR_INVALID_ARGUMENT);

    if (FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_SKIP_PIPELINE_INFO))
        return FFX_OK;

    const bool lockEnable = FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_ENABLE_THREAD_SYNCHRONIZATION);
    if (lockEnable)
    {
        FFX_MUTEX_LOCK(contextPrivate->pipelinesNamesBuffer.mutex);
    }

    contextPrivate->pRegisteredPipelines = (BreadcrumbsPipelineData*)ffxBreadcrumbsAppendList(contextPrivate->pRegisteredPipelines,
        contextPrivate->registeredPipelinesCount, sizeof(BreadcrumbsPipelineData), 1, &contextPrivate->contextDescription.allocCallbacks);
    BreadcrumbsPipelineData* newPipeline = contextPrivate->pRegisteredPipelines + contextPrivate->registeredPipelinesCount;
    ++contextPrivate->registeredPipelinesCount;

    FfxAllocationCallbacks* allocs = &contextPrivate->contextDescription.allocCallbacks;
    BreadcrumbsCustomNameBuffer* nameBuffer = &contextPrivate->pipelinesNamesBuffer;

    newPipeline->pipeline = pipelineDescription->pipeline;
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->name, false, &newPipeline->name);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->vertexShader, false, &newPipeline->vertexShader);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->hullShader, false, &newPipeline->hullShader);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->domainShader, false, &newPipeline->domainShader);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->geometryShader, false, &newPipeline->geometryShader);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->meshShader, false, &newPipeline->meshShader);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->amplificationShader, false, &newPipeline->amplificationShader);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->pixelShader, false, &newPipeline->pixelShader);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->computeShader, false, &newPipeline->computeShader);
    breadcrumbsSetName(allocs, nameBuffer, &pipelineDescription->rayTracingShader, false, &newPipeline->rayTracingShader);

    if (lockEnable)
    {
        FFX_MUTEX_UNLOCK(contextPrivate->pipelinesNamesBuffer.mutex);
    }
    return FFX_OK;
}

FfxErrorCode ffxBreadcrumbsSetPipeline(FfxBreadcrumbsContext* context, FfxCommandList commandList, FfxPipeline pipeline)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(commandList, FFX_ERROR_INVALID_POINTER);
    
    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    FFX_RETURN_ON_ERROR(breadcrumbsIsCorrectPipeline(contextPrivate, pipeline, false), FFX_ERROR_INVALID_ARGUMENT);

    if (FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_SKIP_PIPELINE_INFO))
        return FFX_OK;

    FfxErrorCode ret = FFX_OK;
    BreadcrumbsFrameData* frame = breadcrumbsGetCurrentFrame(contextPrivate);

    const bool lockEnable = FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_ENABLE_THREAD_SYNCHRONIZATION);
    if (lockEnable)
    {
        FFX_MUTEX_LOCK_SHARED(frame->listMutex);
    }

    BreadcrumbsListData* listData = breadcrumbsSearchList(frame, commandList);
    if (listData)
        listData->currentPipeline = pipeline;
    else
        ret = FFX_ERROR_INVALID_ARGUMENT;

    if (lockEnable)
    {
        FFX_MUTEX_UNLOCK_SHARED(frame->listMutex);
    }
    return ret;
}

FfxErrorCode ffxBreadcrumbsBeginMarker(FfxBreadcrumbsContext* context, FfxCommandList commandList, FfxBreadcrumbsMarkerType type, const FfxBreadcrumbsNameTag* name)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(name, FFX_ERROR_INVALID_POINTER);

    // When specifying marker as custom pass, name cannot be empty.
    if (type == FFX_BREADCRUMBS_MARKER_PASS && name->pName == nullptr)
        return FFX_ERROR_INVALID_ARGUMENT;

    // Save CLs for later and place them in a cache to group commands and ensure Begin-End stays on same CL.
    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    BreadcrumbsFrameData* frame = breadcrumbsGetCurrentFrame(contextPrivate);

    BreadcrumbsMarkerData markerData = {};
    markerData.type = type;

    FfxAllocationCallbacks* allocs = &contextPrivate->contextDescription.allocCallbacks;
    const bool lockEnable = FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_ENABLE_THREAD_SYNCHRONIZATION);
    breadcrumbsSetName(allocs, &frame->namesBuffer, name, lockEnable, &markerData.name);

    // Get queue type of current command list.
    if (lockEnable)
    {
        FFX_MUTEX_LOCK_SHARED(frame->listMutex);
    }
    BreadcrumbsListData* listData = breadcrumbsSearchList(frame, commandList);
    if (listData == nullptr)
    {
        if (lockEnable)
        {
            FFX_MUTEX_UNLOCK_SHARED(frame->listMutex);
        }
        return FFX_ERROR_INVALID_ARGUMENT;
    }
    uint32_t queueType = listData->queueType;
    FFX_ASSERT(queueType < contextPrivate->contextDescription.usedGpuQueuesCount);

    if (lockEnable)
    {
        FFX_MUTEX_UNLOCK_SHARED(frame->listMutex);
        FFX_MUTEX_LOCK(frame->blockMutex);
    }

    // Select block with free region for marker.
    BreadcrumbsBlockVector* queueBlocks = frame->pBlockPerQueue + queueType;
    FfxBreadcrumbsBlockData* block = breadcrumbsGetLastBlock(queueBlocks);
    FFX_ASSERT(block);

    if (block->nextMarker >= contextPrivate->contextDescription.maxMarkersPerMemoryBlock)
    {
        // Advance to next block.
        if (++queueBlocks->currentBlock >= queueBlocks->memoryBlocksCount)
        {
            FfxErrorCode error = breadcrumbsAllocBlock(&contextPrivate->contextDescription.backendInterface, allocs,
                queueBlocks, contextPrivate->contextDescription.maxMarkersPerMemoryBlock);
            if (error != FFX_OK)
            {
                FFX_MUTEX_UNLOCK(frame->blockMutex);
                return error;
            }
            block = breadcrumbsGetLastBlock(queueBlocks);
        }
        else
            block->nextMarker = 0;
    }

    markerData.block = queueBlocks->currentBlock;
    markerData.offset = block->nextMarker++;
    void* buffer = block->buffer;
    const uint64_t baseAddress = block->baseAddress;

    if (lockEnable)
    {
        FFX_MUTEX_UNLOCK(frame->blockMutex);
        FFX_MUTEX_LOCK_SHARED(frame->listMutex);
    }

    // Find CL.
    listData = breadcrumbsSearchList(frame, commandList);
    FFX_ASSERT(listData);
    markerData.usedPipeline = listData->currentPipeline;
    markerData.nestingLevel = listData->currentStackCount;

    listData->pCurrentStack = (uint32_t*)ffxBreadcrumbsAppendList(listData->pCurrentStack, listData->currentStackCount, sizeof(uint32_t), 1, allocs);
    listData->pCurrentStack[listData->currentStackCount++] = listData->markersCount;
    listData->pMarkers = (BreadcrumbsMarkerData*)ffxBreadcrumbsAppendList(listData->pMarkers, listData->markersCount, sizeof(BreadcrumbsMarkerData), 1, allocs);
    listData->pMarkers[listData->markersCount++] = markerData;

    if (lockEnable)
    {
        FFX_MUTEX_UNLOCK_SHARED(frame->listMutex);
    }

    // Unset bit 0 indicates that it's starting marker.
    contextPrivate->contextDescription.backendInterface.fpBreadcrumbsWrite(&contextPrivate->contextDescription.backendInterface,
        commandList, (contextPrivate->frameIndex + 1) << 1, baseAddress + 4ULL * markerData.offset, buffer, true);
    return FFX_OK;
}

FfxErrorCode ffxBreadcrumbsEndMarker(FfxBreadcrumbsContext* context, FfxCommandList commandList)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // Save CLs for later and place them in a cache to group commands and ensure Begin-End stays on same CL.
    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    BreadcrumbsFrameData* frame = breadcrumbsGetCurrentFrame(contextPrivate);

    const bool lockEnable = FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_ENABLE_THREAD_SYNCHRONIZATION);
    if (lockEnable)
    {
        FFX_MUTEX_LOCK(frame->listMutex);
    }

    // Find CL that is used with this marker
    BreadcrumbsListData* listData = breadcrumbsSearchList(frame, commandList);
    if (listData == nullptr || listData->currentStackCount == 0)
    {
        if (lockEnable)
        {
            FFX_MUTEX_UNLOCK(frame->listMutex);
        }
        return FFX_ERROR_INVALID_ARGUMENT;
    }

    // Retrieve data about which marker is being closed now.
    uint32_t markerIndex = listData->pCurrentStack[--listData->currentStackCount];
    FFX_ASSERT(markerIndex < listData->markersCount);
    listData->pCurrentStack = (uint32_t*)ffxBreadcrumbsPopList(listData->pCurrentStack, listData->currentStackCount, sizeof(uint32_t),
        &contextPrivate->contextDescription.allocCallbacks);

    // Get correct location for writing
    BreadcrumbsMarkerData* marker = listData->pMarkers + markerIndex;
    FfxBreadcrumbsBlockData* block = frame->pBlockPerQueue[listData->queueType].pMemoryBlocks + marker->block;
    void* buffer = block->buffer;
    const uint64_t baseAddress = block->baseAddress;
    const uint32_t offset = marker->offset;

    if (lockEnable)
    {
        FFX_MUTEX_UNLOCK(frame->listMutex);
    }

    // Set bit 0 indicates that it's ending marker.
    contextPrivate->contextDescription.backendInterface.fpBreadcrumbsWrite(&contextPrivate->contextDescription.backendInterface,
        commandList, ((contextPrivate->frameIndex + 1) << 1) + 1, baseAddress + 4ULL * offset, buffer, false);
    return FFX_OK;
}

FfxErrorCode ffxBreadcrumbsPrintStatus(FfxBreadcrumbsContext* context, FfxBreadcrumbsMarkersStatus* markersStatus)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(markersStatus, FFX_ERROR_INVALID_POINTER);
    
    FfxBreadcrumbsContext_Private* contextPrivate = (FfxBreadcrumbsContext_Private*)(context);
    markersStatus->bufferSize = 0;
    markersStatus->pBuffer = nullptr;

    const bool skipFinishedLists = !FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_FINISHED_LISTS);
    const bool skipNotStartedLists = !FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_NOT_STARTED_LISTS);
    const bool skipFinishedNodes = !FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_FINISHED_NODES);
    const bool skipNotStartedNodes = !FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_NOT_STARTED_NODES);
    
    FfxAllocationCallbacks* allocs = &contextPrivate->contextDescription.allocCallbacks;
    if (!FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_SKIP_DEVICE_INFO))
    {
        FFX_RETURN_ON_ERROR(contextPrivate->contextDescription.backendInterface.fpBreadcrumbsPrintDeviceInfo, FFX_ERROR_INVALID_ARGUMENT);
        contextPrivate->contextDescription.backendInterface.fpBreadcrumbsPrintDeviceInfo(&contextPrivate->contextDescription.backendInterface, allocs,
            FFX_CONTAINS_FLAG(contextPrivate->contextDescription.flags, FFX_BREADCRUMBS_PRINT_EXTENDED_DEVICE_INFO),
            &markersStatus->pBuffer, &markersStatus->bufferSize);
    }
    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "[BREADCRUMBS]\n");
    for (uint32_t i = contextPrivate->contextDescription.frameHistoryLength; i--;)
    {
        if (i > contextPrivate->frameIndex)
            continue;
        const uint32_t currentFrame = contextPrivate->frameIndex - i;
        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "<Frame ");
        FFX_BREADCRUMBS_APPEND_UINT(markersStatus->pBuffer, markersStatus->bufferSize, currentFrame);
        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, ">\n");

        // Move backwards in recorded frames inside ring buffer for frames in flight.
        BreadcrumbsFrameData* frame = contextPrivate->pFrameData + (currentFrame % contextPrivate->contextDescription.frameHistoryLength);
        if (frame->usedListsCount == 0)
        {
            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " - No command lists\n");
            continue;
        }
        for (size_t j = 0; j < frame->usedListsCount; ++j)
        {
            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " - [");

            BreadcrumbsListData* cl = frame->pUsedLists + j;
            bool skipList = false;
            uint32_t markerFrame = UINT32_MAX;
            const uint32_t* location = nullptr;
            const BreadcrumbsBlockVector* queueBlocks = frame->pBlockPerQueue + cl->queueType;
            // Check for finished or not started CLs
            if (cl->markersCount > 0)
            {
                // Inspect last marker's memory location to determine it's status and decode it's frame value (coded in 31-1 bits of saved data minus 1)
                BreadcrumbsMarkerData* lastMarker = cl->pMarkers + cl->markersCount - 1;
                location = (uint32_t*)(queueBlocks->pMemoryBlocks[lastMarker->block].memory) + lastMarker->offset;
                markerFrame = (*location >> 1) - 1;
                FFX_ASSERT_MESSAGE(markerFrame <= currentFrame, "Should not find value higher than current frame!");

                if (markerFrame == currentFrame)
                {
                    // Check marker status: 0 - started, 1 - finished
                    // If finished then all previous have also finished
                    if (*location & 1)
                    {
                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "X");
                        skipList = skipFinishedLists;
                    }
                    else
                    {
                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, ">");
                    }
                }
                else // markerFrame < currentFrame, last case is asserted for
                {
                    // Same check for first marker
                    location = (uint32_t*)(queueBlocks->pMemoryBlocks[cl->pMarkers->block].memory) + cl->pMarkers->offset;
                    markerFrame = (*location >> 1) - 1;
                    FFX_ASSERT_MESSAGE(markerFrame <= currentFrame, "Should not find value higher than current frame!");

                    // If first marker have not started yet, then none in this command list has started too
                    if (markerFrame < currentFrame)
                    {
                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " ");
                        skipList = skipNotStartedLists;
                    }
                    else
                    {
                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, ">");
                    }
                }
            }
            else
            {
                FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " ");
                skipList = true;
            }

            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "] Queue type <");
            FFX_BREADCRUMBS_APPEND_UINT(markersStatus->pBuffer, markersStatus->bufferSize, cl->queueType);
            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, ">, submission no. ");
            FFX_BREADCRUMBS_APPEND_UINT(markersStatus->pBuffer, markersStatus->bufferSize, cl->submissionIndex);
            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, ", command list ");
            FFX_BREADCRUMBS_APPEND_UINT64(markersStatus->pBuffer, markersStatus->bufferSize, j + 1);
            if (cl->name.pName)
            {
                FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, ": \"");
                FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&frame->namesBuffer, &cl->name));
                FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\"");
            }
            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\n");
            if (skipList)
                continue;

            // Indices determining how long given nesting level will be present before moving up in hierarchy.
            uint32_t* nestingLevelIndicatorIndices = (uint32_t*)ffxBreadcrumbsAppendList(nullptr, 0, sizeof(uint32_t), 1, allocs);
            nestingLevelIndicatorIndices[0] = 0;
            uint32_t nestingLevelIndicesCount = 1;

            // Display level informs from which point deeper markers can be cut out
            // in case of collapsing uniform nodes (markers that all nested markers have same finished or not started status).
            uint32_t displayLevel = UINT32_MAX;
            uint32_t markerId = 0;

            FfxBreadcrumbsMarkerType currentType = FFX_BREADCRUMBS_MARKER_PASS;
            // Go through every marker in command list and display it's info
            for (uint32_t m = 0; m < cl->markersCount; ++m)
            {
                BreadcrumbsMarkerData* marker = cl->pMarkers + m;
                // Same checks as before with determining if marker is finished or not started
                location = (uint32_t*)(queueBlocks->pMemoryBlocks[marker->block].memory) + marker->offset;
                markerFrame = (*location >> 1) - 1;
                FFX_ASSERT_MESSAGE(markerFrame <= currentFrame, "Should not find value higher than current frame!");

                char status = ' ';
                if (markerFrame == currentFrame)
                {
                    if (*location & 1)
                        status = 'X';
                    else
                        status = '>';
                }

                // When going deeper into hierarchy allocate new index for marker.
                if (marker->nestingLevel >= nestingLevelIndicesCount)
                {
                    markerId = 0;
                    nestingLevelIndicatorIndices = (uint32_t*)ffxBreadcrumbsAppendList(nestingLevelIndicatorIndices, nestingLevelIndicesCount, sizeof(uint32_t), 1, allocs);
                    nestingLevelIndicatorIndices[nestingLevelIndicesCount++] = 0;
                    // Check whether deeper nodes will be collapsed or not.
                    if (((skipFinishedNodes && status == 'X') || (skipNotStartedNodes && status == ' ')) && displayLevel == UINT32_MAX)
                        displayLevel = marker->nestingLevel;
                }
                else
                {
                    if (skipFinishedNodes || skipNotStartedNodes)
                    {
                        // If going up in hierarchy check wheter displayLevel can be relaxed or restricted to upper level.
                        if (displayLevel != UINT32_MAX)
                        {
                            if (displayLevel > marker->nestingLevel)
                                displayLevel = status == '>' ? UINT32_MAX : marker->nestingLevel;
                            else if (displayLevel == marker->nestingLevel && status == '>')
                                displayLevel = UINT32_MAX;
                        }
                        else if ((skipFinishedNodes && status == 'X') || (skipNotStartedNodes && status == ' '))
                            displayLevel = marker->nestingLevel;
                    }
                    // Pop indicators when moving to up in nesting levels
                    if (marker->nestingLevel + 1 < nestingLevelIndicesCount)
                        markerId = 0;
                    while (marker->nestingLevel + 1 < nestingLevelIndicesCount)
                        nestingLevelIndicatorIndices = (uint32_t*)ffxBreadcrumbsPopList(nestingLevelIndicatorIndices, --nestingLevelIndicesCount, sizeof(uint32_t), allocs);
                }
                if (marker->type != currentType)
                {
                    currentType = marker->type;
                    markerId = 0;
                }

                if (marker->nestingLevel <= displayLevel)
                {
                    // When on next level, check for newer indices
                    uint32_t* lastIdx = nestingLevelIndicatorIndices + nestingLevelIndicesCount - 1;
                    if (*lastIdx != UINT32_MAX && *lastIdx <= m)
                    {
                        *lastIdx = UINT32_MAX;
                        // Detect how long given level will be present to calculate proper tree branches
                        for (uint32_t next = m + 1; next < cl->markersCount; ++next)
                        {
                            const uint32_t nestingLevel = cl->pMarkers[next].nestingLevel;
                            if (nestingLevel < marker->nestingLevel)
                                break;
                            else if (nestingLevel == marker->nestingLevel)
                                *lastIdx = next;
                        }
                    }

                    // Mark previous levels in tree and display current entry
                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "  ");
                    for (uint32_t k = 0; k < marker->nestingLevel; ++k)
                    {
                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "  ");
                        if (nestingLevelIndicatorIndices[k] != UINT32_MAX && nestingLevelIndicatorIndices[k] > m)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\xe2\x94\x82"); // `|`
                        }
                        else
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " ");
                        }
                    }

                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "  ");
                    if (nestingLevelIndicatorIndices[nestingLevelIndicesCount - 1] == UINT32_MAX || nestingLevelIndicatorIndices[nestingLevelIndicesCount - 1] == m)
                    {
                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\xe2\x94\x94"); // `'-`
                    }
                    else
                    {
                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\xe2\x94\x9c"); // `|-`
                    }

                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\xe2\x94\x80["); // `-`
                    markersStatus->pBuffer = (char*)ffxBreadcrumbsAppendList(markersStatus->pBuffer, markersStatus->bufferSize, sizeof(char), 1, allocs);
                    markersStatus->pBuffer[markersStatus->bufferSize++] = status;
                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "] ");

                    if (marker->type == FFX_BREADCRUMBS_MARKER_PASS)
                    {
                        FFX_ASSERT_MESSAGE(marker->name.pName, "Custom passes should always have names!");
                        FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&frame->namesBuffer, &marker->name));
                    }
                    else
                    {
                        FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadDecodeMarkerType(marker->type));
                        if (markerId != 0 || m + 1 < cl->markersCount && cl->pMarkers[m + 1].type == marker->type)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " ");
                            FFX_BREADCRUMBS_APPEND_UINT(markersStatus->pBuffer, markersStatus->bufferSize, ++markerId);
                        }
                        if (marker->name.pName)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, ": \"");
                            FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&frame->namesBuffer, &marker->name));
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\"");
                        }
                    }
                    if (marker->usedPipeline)
                    {
                        BreadcrumbsPipelineData* pipeline = breadcrumbsSearchPipeline(contextPrivate, marker->usedPipeline);
                        FFX_ASSERT_MESSAGE(pipeline, "When pipeline has been properly set on command list it should always be present here!");

                        const bool isCompute = pipeline->computeShader.pName != nullptr;
                        const bool isRT = pipeline->rayTracingShader.pName != nullptr;
                        const bool isVertexShading = pipeline->vertexShader.pName || pipeline->hullShader.pName || pipeline->domainShader.pName || pipeline->geometryShader.pName;
                        const bool isMeshShading = pipeline->meshShader.pName || pipeline->amplificationShader.pName;
                        const bool isGfx = isVertexShading || isMeshShading || pipeline->pixelShader.pName;
                        FFX_ASSERT_MESSAGE((!isCompute && !isRT) || (!isCompute && !isGfx) || (!isRT && !isGfx), "Wrong combination of shaders for pipeline!");
                        FFX_ASSERT_MESSAGE(!(isVertexShading && isMeshShading), "Wrong combination of geometry processing for graphics pipeline!");

                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, ", ");
                        if (isCompute)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "compute ");
                        }
                        else if (isRT)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "ray tracing ");
                        }
                        else if (isGfx)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "graphics ");
                        }
                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "pipeline");
                        if (pipeline->name.pName)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " \"");
                            FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->name));
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\"");
                        }

                        if (isCompute)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " [ CS: ");
                            FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->computeShader));
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " ]");
                        }
                        else if (isRT)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " [ RT: ");
                            FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->rayTracingShader));
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " ]");
                        }
                        else if (isGfx)
                        {
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " [");
                            bool before = false;
                            if (isVertexShading)
                            {
                                if (pipeline->vertexShader.pName)
                                {
                                    before = true;
                                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " VS: ");
                                    FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->vertexShader));
                                }
                                if (pipeline->hullShader.pName)
                                {
                                    if (before)
                                    {
                                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " |");
                                    }
                                    before = true;
                                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " HS: ");
                                    FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->hullShader));
                                }
                                if (pipeline->domainShader.pName)
                                {
                                    if (before)
                                    {
                                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " |");
                                    }
                                    before = true;
                                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " DS: ");
                                    FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->domainShader));
                                }
                                if (pipeline->geometryShader.pName)
                                {
                                    if (before)
                                    {
                                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " |");
                                    }
                                    before = true;
                                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " GS: ");
                                    FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->geometryShader));
                                }
                            }
                            else if (isMeshShading)
                            {
                                if (pipeline->meshShader.pName)
                                {
                                    before = true;
                                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " MS: ");
                                    FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->meshShader));
                                }
                                if (pipeline->amplificationShader.pName)
                                {
                                    if (before)
                                    {
                                        FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " |");
                                    }
                                    before = true;
                                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " AS: ");
                                    FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->amplificationShader));
                                }
                            }
                            if (pipeline->pixelShader.pName)
                            {
                                if (before)
                                {
                                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " |");
                                }
                                before = true;
                                FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " PS: ");
                                FFX_BREADCRUMBS_APPEND_STRING_DYNAMIC(markersStatus->pBuffer, markersStatus->bufferSize, breadcrumbsGetName(&contextPrivate->pipelinesNamesBuffer, &pipeline->pixelShader));
                            }
                            FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, " ]");
                        }
                    }
                    FFX_BREADCRUMBS_APPEND_STRING(markersStatus->pBuffer, markersStatus->bufferSize, "\n");
                }
            }
        }
    }

    return FFX_OK;
}

FFX_API FfxVersionNumber ffxBreadcrumbsGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_BREADCRUMBS_VERSION_MAJOR, FFX_BREADCRUMBS_VERSION_MINOR, FFX_BREADCRUMBS_VERSION_PATCH);
}
