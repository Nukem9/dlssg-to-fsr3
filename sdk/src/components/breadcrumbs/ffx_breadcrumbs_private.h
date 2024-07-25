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

#pragma once
#include <FidelityFX/host/ffx_breadcrumbs.h>

typedef struct BreadcrumbsBlockVector {

    size_t                              memoryBlocksCount;
    size_t                              currentBlock;
    FfxBreadcrumbsBlockData*            pMemoryBlocks;
} BreadcrumbsBlockVector;

typedef struct BreadcrumbsCustomName {

    char*                               pName;
    bool                                isCopied;
} BreadcrumbsCustomName;

typedef struct BreadcrumbsCustomNameBuffer {

    size_t                              bufferSize;
    char*                               pBuffer;
    size_t                              currentNamesOffset;
    FFX_MUTEX                           mutex;
} BreadcrumbsCustomNameBuffer;

typedef struct BreadcrumbsMarkerData {

    FfxBreadcrumbsMarkerType            type;
    uint32_t                            nestingLevel;
    size_t                              block;
    uint32_t                            offset;
    BreadcrumbsCustomName               name;
    FfxPipeline                         usedPipeline;
} BreadcrumbsMarkerData;

typedef struct BreadcrumbsListData {

    FfxCommandList                      list;
    uint32_t                            queueType;
    uint16_t                            submissionIndex;
    BreadcrumbsCustomName               name;
    FfxPipeline                         currentPipeline;
    uint32_t                            markersCount;
    BreadcrumbsMarkerData*              pMarkers;
    // Indices for ending markers.
    uint32_t                            currentStackCount;
    uint32_t*                           pCurrentStack;
} BreadcrumbsListData;

typedef struct BreadcrumbsFrameData {

    size_t                              usedListsCount;
    BreadcrumbsListData*                pUsedLists;
    BreadcrumbsBlockVector*             pBlockPerQueue;
    BreadcrumbsCustomNameBuffer         namesBuffer;
    FFX_MUTEX                           listMutex;
    FFX_MUTEX                           blockMutex;
} BreadcrumbsFrameData;

typedef struct BreadcrumbsPipelineData {

    FfxPipeline                         pipeline;
    BreadcrumbsCustomName               name;
    BreadcrumbsCustomName               vertexShader;
    BreadcrumbsCustomName               hullShader;
    BreadcrumbsCustomName               domainShader;
    BreadcrumbsCustomName               geometryShader;
    BreadcrumbsCustomName               meshShader;
    BreadcrumbsCustomName               amplificationShader;
    BreadcrumbsCustomName               pixelShader;
    BreadcrumbsCustomName               computeShader;
    BreadcrumbsCustomName               rayTracingShader;
} BreadcrumbsPipelineData;

// FfxBreadcrumbsContext_Private
// The private implementation of the Breadcrumbs context.
typedef struct FfxBreadcrumbsContext_Private {

    FfxBreadcrumbsContextDescription    contextDescription;
    uint32_t                            frameIndex;
    FfxUInt32                           effectContextId;
    BreadcrumbsFrameData*               pFrameData;
    size_t                              registeredPipelinesCount;
    BreadcrumbsPipelineData*            pRegisteredPipelines;
    BreadcrumbsCustomNameBuffer         pipelinesNamesBuffer;
} FfxBreadcrumbsContext_Private;
