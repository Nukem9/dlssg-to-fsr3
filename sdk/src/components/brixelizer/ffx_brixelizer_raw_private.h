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

#include <FidelityFX/gpu/brixelizer/ffx_brixelizer_host_gpu_shared_private.h>
#include <FidelityFX/gpu/brixelizer/ffx_brixelizer_resources.h>

typedef enum BrixelizerShaderPermutationOptions
{
    BRIXELIZER_SHADER_PERMUTATION_FORCE_WAVE64 = (1 << 0),  ///< doesn't map to a define, selects different table
    BRIXELIZER_SHADER_PERMUTATION_ALLOW_FP16   = (1 << 1),  ///< Enables fast math computations where possible
} BrixelizerShaderPermutationOptions;

#define MEMBER_LIST                           \
        MEMBER(counters)                      \
        MEMBER(triangle_swap)                 \
        MEMBER(voxel_allocation_fail_counter) \
        MEMBER(bricks_storage)                \
        MEMBER(bricks_storage_offsets)        \
        MEMBER(bricks_compression_list)       \
        MEMBER(bricks_clear_list)             \
        MEMBER(job_counters)                  \
        MEMBER(job_counters_scan)             \
        MEMBER(job_global_counters_scan)      \
        MEMBER(cr1_references)                \
        MEMBER(cr1_compacted_references)      \
        MEMBER(cr1_ref_counters)              \
        MEMBER(cr1_ref_counter_scan)          \
        MEMBER(cr1_ref_global_scan)           \
        MEMBER(cr1_stamp_scan)                \
        MEMBER(cr1_stamp_global_scan)         \
        MEMBER(debug_aabbs)

#define MEMBER(_name) + 1
const uint32_t FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES = 0 + MEMBER_LIST;
#undef MEMBER

typedef struct FfxBrixelizerScratchPartition
{
    union
    {
        struct
        {
        #define MEMBER(name) uint32_t name##_size;
            MEMBER_LIST
        #undef MEMBER

        #define MEMBER(name) uint32_t name##_offset;
            MEMBER_LIST
        #undef MEMBER
        };
        uint32_t array[FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES * 2];
    };
} FfxBrixelizerScratchPartition;

#undef MEMBER_LIST

// FfxBrixelizerCascade_Private
// The private implementation of the brixelizer cascade.
typedef struct FfxBrixelizerCascade_Private
{
    bool             isAllocated;
    bool             resourcesRegistered;
    FfxBrixelizerCascadeInfo info;
} FfxBrixelizerCascade_Private;

#define FFX_BRIXELIZER_NUM_IN_FLIGHT_FRAMES     3

typedef struct FfxBrixelizerUploadBufferMetaData {
    uint32_t          size;
    uint32_t          stride;
    uint32_t          id;
    FfxResourceUsage  usage;
    FfxResourceStates state;
    const wchar_t    *name;
} FfxBrixelizerUploadBufferMetaData;

#define ALIGN_UP_256(val) ((val + 255) & ~(255))

static const FfxBrixelizerUploadBufferMetaData uploadBufferMetaData[] = {
    { FFX_BRIXELIZER_MAX_INSTANCES * FFX_BRIXELIZER_NUM_IN_FLIGHT_FRAMES * sizeof(FfxBrixelizerInstanceInfo),     sizeof(FfxBrixelizerInstanceInfo),     FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_INSTANCE_INFO_BUFFER,      FFX_RESOURCE_USAGE_UAV, FFX_RESOURCE_STATE_COPY_SRC,     L"Brixelizer_UploadInstanceBuffer"        },
    { FFX_BRIXELIZER_MAX_INSTANCES * FFX_BRIXELIZER_NUM_IN_FLIGHT_FRAMES * sizeof(FfxFloat32x3x4),                sizeof(FfxFloat32x4),                  FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_INSTANCE_TRANSFORM_BUFFER, FFX_RESOURCE_USAGE_UAV, FFX_RESOURCE_STATE_COPY_SRC,     L"Brixelizer_UploadTransformBuffer"       },
    { FFX_BRIXELIZER_MAX_INSTANCES * FFX_BRIXELIZER_NUM_IN_FLIGHT_FRAMES * sizeof(FfxBrixelizerBrixelizationJob), sizeof(FfxBrixelizerBrixelizationJob), FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_JOB_BUFFER,                FFX_RESOURCE_USAGE_UAV, FFX_RESOURCE_STATE_GENERIC_READ, L"Brixelizer_UploadJobBuffer"             },
    { FFX_BRIXELIZER_MAX_INSTANCES * FFX_BRIXELIZER_NUM_IN_FLIGHT_FRAMES * sizeof(uint32_t),                      sizeof(uint32_t),                      FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_JOB_INDEX_BUFFER,          FFX_RESOURCE_USAGE_UAV, FFX_RESOURCE_STATE_GENERIC_READ, L"Brixelizer_UploadJobIndexBuffer"        },
    { FFX_BRIXELIZER_MAX_INSTANCES * FFX_BRIXELIZER_NUM_IN_FLIGHT_FRAMES * sizeof(uint32_t),                      sizeof(uint32_t),                      FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_DEBUG_INSTANCE_ID_BUFFER,  FFX_RESOURCE_USAGE_UAV, FFX_RESOURCE_STATE_COPY_SRC,     L"Brixelizer_UploadDebugInstanceIDBuffer" },
};

#define FFX_BRIXELIZER_NUM_UPLOAD_BUFFERS FFX_ARRAY_ELEMENTS(uploadBufferMetaData)

typedef struct BufferBindingInfo
{
    uint32_t offset;
    uint32_t size;
    uint32_t stride;
} BufferBindingInfo;

#define SRV_BUFFER_BINDING_INFOS           \
    INFO(UPLOAD_JOB_BUFFER)                \
    INFO(UPLOAD_JOB_INDEX_BUFFER)          \
    INFO(UPLOAD_DEBUG_INSTANCE_ID_BUFFER)

typedef enum SrvBufferBindingInfoID {
#define INFO(name) SRV_BUFFER_BINDING_INFO_##name,
    SRV_BUFFER_BINDING_INFOS
#undef INFO
    NUM_SRV_BUFFER_BINDING_INFOS,
} SrvBufferBindingInfoID;

// FfxBrixelizerContext_Private
// The private implementation of the brixelizer context.
typedef struct FfxBrixelizerRawContext_Private
{
    FfxBrixelizerRawContextDescription contextDescription;
    FfxUInt32               effectContextId;
    FfxDevice               device;
    FfxDeviceCapabilities   deviceCapabilities;
    FfxPipelineState        pipelineContextClearCounters;
    FfxPipelineState        pipelineContextCollectClearBricks;
    FfxPipelineState        pipelineContextPrepareClearBricks;
    FfxPipelineState        pipelineContextClearBrick;
    FfxPipelineState        pipelineContextCollectDirtyBricks;
    FfxPipelineState        pipelineContextPrepareEikonalArgs;
    FfxPipelineState        pipelineContextEikonal;
    FfxPipelineState        pipelineContextMergeCascades;
    FfxPipelineState        pipelineContextPrepareMergeBricksArgs;
    FfxPipelineState        pipelineContextMergeBricks;
    FfxPipelineState        pipelineCascadeClearBuildCounters;
    FfxPipelineState        pipelineCascadeResetCascade;
    FfxPipelineState        pipelineCascadeScrollCascade;
    FfxPipelineState        pipelineCascadeClearRefCounters;
    FfxPipelineState        pipelineCascadeClearJobCounter;
    FfxPipelineState        pipelineCascadeInvalidateJobAreas;
    FfxPipelineState        pipelineCascadeCoarseCulling;
    FfxPipelineState        pipelineCascadeScanJobs;
    FfxPipelineState        pipelineCascadeVoxelize;
    FfxPipelineState        pipelineCascadeScanReferences;
    FfxPipelineState        pipelineCascadeCompactReferences;
    FfxPipelineState        pipelineCascadeClearBrickStorage;
    FfxPipelineState        pipelineCascadeEmitSDF;
    FfxPipelineState        pipelineCascadeCompressBrick;
    FfxPipelineState        pipelineCascadeInitializeCascade;
    FfxPipelineState        pipelineCascadeMarkCascadeUninitialized;
    FfxPipelineState        pipelineCascadeBuildTreeAABB;
    FfxPipelineState        pipelineCascadeFreeCascade;
    FfxPipelineState        pipelineDebugVisualization;
    FfxPipelineState        pipelineDebugInstanceAABBs;
    FfxPipelineState        pipelineDebugDrawAABBTree;

    BufferBindingInfo       srvBufferBindingInfos[NUM_SRV_BUFFER_BINDING_INFOS];
    BufferBindingInfo       uavInfo[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_COUNT];
    FfxBrixelizerCascade_Private    cascades[FFX_BRIXELIZER_MAX_CASCADES];
    FfxResourceInternal     resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_COUNT];
    FfxBrixelizerBrixelizationJob   jobs[FFX_BRIXELIZER_MAX_INSTANCES];
    FfxConstantBuffer       constantBuffers[4];
    uint32_t                indexOffsets[FFX_BRIXELIZER_MAX_INSTANCES];
    FfxGpuJobDescription    gpuJobDescription;
    uint8_t*                uploadBufferMappedPointers[FFX_BRIXELIZER_NUM_UPLOAD_BUFFERS];
    uint32_t                uploadBufferOffsets[FFX_BRIXELIZER_NUM_UPLOAD_BUFFERS];
    uint32_t                uploadBufferSizes[FFX_BRIXELIZER_NUM_UPLOAD_BUFFERS];
    void*                   cascadeReadbackBufferMappedPointers[FFX_BRIXELIZER_MAX_CASCADES * 3];
    uint8_t*                readbackBufferMappedPointers[3];
    uint32_t                totalBricks;
    uint32_t                frameIndex;
    FfxBrixelizerDebugCounters      debugCounters;
    FfxBrixelizerScratchCounters    cascadeCounters[FFX_BRIXELIZER_MAX_CASCADES];
    uint32_t                cascadeCounterPositions[FFX_BRIXELIZER_MAX_CASCADES];
    FfxBoolean              doInit;
    
    uint32_t                numInstances;
    FfxBrixelizerInstanceInfo       hostInstances[FFX_BRIXELIZER_MAX_INSTANCES];
    FfxFloat32x3x4                  hostTransforms[FFX_BRIXELIZER_MAX_INSTANCES];
    FfxBrixelizerInstanceID         hostFreelist[FFX_BRIXELIZER_MAX_INSTANCES];
    uint32_t                hostFreelistSize;
    FfxBrixelizerInstanceID         hostNewInstanceList[FFX_BRIXELIZER_MAX_INSTANCES];
    uint32_t                hostNewInstanceListSize;
    uint32_t                        bufferIndexFreeList[FFX_BRIXELIZER_MAX_INSTANCES];
    uint32_t                        bufferIndexFreeListSize;
    uint32_t                refCount;
} FfxBrixelizerContext_Private;
