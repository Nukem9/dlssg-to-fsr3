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

#include <stdint.h>   // for integer types.
#include <algorithm>  // for max used inside SPD CPU code.
#include <cmath>      // for fabs, abs, sinf, sqrt, etc.
#include <string.h>   // for memset.
#include <cfloat>     // for FLT_EPSILON.

#define FFX_CPU
#include <FidelityFX/gpu/ffx_core.h>
#include <FidelityFX/gpu/brixelizer/ffx_brixelizer_host_gpu_shared_private.h>
#include <FidelityFX/host/ffx_brixelizer_raw.h>
#include <FidelityFX/gpu/brixelizer/ffx_brixelizer_resources.h>
#include <ffx_object_management.h>

#include "ffx_brixelizer_raw_private.h"

// lists to map shader resource bindpoint name to resource identifier
typedef struct ResourceBinding
{
    uint32_t index;
    wchar_t  name[64];
} ResourceBinding;

static const ResourceBinding srvResourceBindingTable[] = {
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_JOB_BUFFER, L"r_job_buffer"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_JOB_INDEX_BUFFER, L"r_job_index_buffer"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INSTANCE_INFO_BUFFER, L"r_instance_info_buffer"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INSTANCE_TRANSFORM_BUFFER, L"r_instance_transform_buffer"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_SDF_ATLAS, L"r_sdf_atlas"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_DEBUG_INSTANCE_ID_BUFFER, L"r_debug_instance_id"},
};

static const ResourceBinding uavResourceBindingTable[] = {
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_AABB_TREE, L"rw_cascade_aabbtree"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_AABB_TREES, L"rw_cascade_aabbtrees"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_BRICK_MAP, L"rw_cascade_brick_map"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_BRICK_MAPS, L"rw_cascade_brick_maps"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_COUNTERS, L"rw_scratch_counters"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_INDEX_SWAP, L"rw_scratch_index_swap"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER, L"rw_scratch_voxel_allocation_fail_counter"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BRICKS_STORAGE, L"rw_scratch_bricks_storage"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BRICKS_STORAGE_OFFSETS, L"rw_scratch_bricks_storage_offsets"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BRICKS_COMPRESSION_LIST, L"rw_scratch_bricks_compression_list"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BRICKS_CLEAR_LIST, L"rw_scratch_bricks_clear_list"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_JOB_COUNTERS, L"rw_scratch_job_counters"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_JOB_COUNTERS_SCAN, L"rw_scratch_job_counters_scan"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN, L"rw_scratch_job_global_counters_scan"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_CR1_REFERENCES, L"rw_scratch_cr1_references"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_CR1_COMPACTED_REFERENCES, L"rw_scratch_cr1_compacted_references"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_CR1_REF_COUNTERS, L"rw_scratch_cr1_ref_counters"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_CR1_REF_COUNTER_SCAN, L"rw_scratch_cr1_ref_counter_scan"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_CR1_REF_GLOBAL_SCAN, L"rw_scratch_cr1_ref_global_scan"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_CR1_STAMP_SCAN, L"rw_scratch_cr1_stamp_scan"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_CR1_STAMP_GLOBAL_SCAN, L"rw_scratch_cr1_stamp_global_scan"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1, L"rw_indirect_args_1"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_VOXEL_MAP, L"rw_bctx_bricks_voxel_map"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_AABB, L"rw_bctx_bricks_aabb"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_FREE_LIST, L"rw_bctx_bricks_free_list"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_CLEAR_LIST, L"rw_bctx_bricks_clear_list"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_EIKONAL_LIST, L"rw_bctx_bricks_eikonal_list"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_MERGE_LIST, L"rw_bctx_bricks_merge_list"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_EIKONAL_COUNTERS, L"rw_bctx_bricks_eikonal_counters"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS, L"rw_bctx_counters"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_SDF_ATLAS, L"rw_sdf_atlas"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_DEBUG_OUTPUT, L"rw_debug_output"},
    {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_DEBUG_AABBS, L"rw_debug_aabbs"},
};

static const ResourceBinding cbvResourceBindingTable[] = {
    {FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CASCADE_INFO, L"cbBrixelizerCascadeInfo"},
    {FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CONTEXT_INFO, L"cbBrixelizerContextInfo"},
    {FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, L"cbBrixelizerBuildInfo"},
    {FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_DEBUG_INFO, L"cbBrixelizerDebugInfo"},
};

static size_t cbSizes[] = {
    sizeof(FfxBrixelizerCascadeInfo), 
    sizeof(FfxBrixelizerContextInfo), 
    sizeof(FfxBrixelizerBuildInfo), 
    sizeof(FfxBrixelizerDebugInfo)
};

static void setSRVBindingInfo(FfxBrixelizerRawContext_Private *context, uint32_t id, uint32_t offset, uint32_t size, uint32_t stride)
{
    BufferBindingInfo *info = NULL;
    switch (id) {
    #define INFO(name) case FFX_BRIXELIZER_RESOURCE_IDENTIFIER_##name: info = &context->srvBufferBindingInfos[SRV_BUFFER_BINDING_INFO_##name]; break;
        SRV_BUFFER_BINDING_INFOS
    #undef INFO
        default: FFX_ASSERT(false);
    }
    info->offset = offset;
    info->size   = size;
    info->stride = stride;
}

static BufferBindingInfo getSRVBindingInfo(FfxBrixelizerRawContext_Private *context, uint32_t id)
{
    switch (id) {
    #define INFO(name) case FFX_BRIXELIZER_RESOURCE_IDENTIFIER_##name: return context->srvBufferBindingInfos[SRV_BUFFER_BINDING_INFO_##name];
        SRV_BUFFER_BINDING_INFOS
    #undef INFO
    }
    BufferBindingInfo result = {};
    return result;
}

static void setUAVBindingInfo(FfxBrixelizerRawContext_Private *context, uint32_t id, uint32_t offset, uint32_t size, uint32_t stride)
{
    context->uavInfo[id].offset = offset;
    context->uavInfo[id].size   = size;
    context->uavInfo[id].stride = stride;
}

static uint32_t getUploadBufferID(uint32_t resourceID)
{
    for (uint32_t i = 0; i < FFX_ARRAY_ELEMENTS(uploadBufferMetaData); ++i) {
        const FfxBrixelizerUploadBufferMetaData *metaData = &uploadBufferMetaData[i];
        if (metaData->id == resourceID) {
            return i;
        }
    }
    FFX_ASSERT(0);
    return (uint32_t)-1;
}

static uint32_t getCascadeReadbackBufferID(uint32_t cascadeID, uint32_t readbackBufferID)
{
    FFX_ASSERT(0 <= readbackBufferID && readbackBufferID < 3);
    FFX_ASSERT(0 <= cascadeID && cascadeID < FFX_BRIXELIZER_MAX_CASCADES);
    return readbackBufferID * FFX_BRIXELIZER_MAX_CASCADES + cascadeID;
}

static uint32_t getCascadeReadbackBufferResourceID(uint32_t cascadeID, uint32_t readbackBufferID)
{
    return FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_READBACK_BUFFERS + getCascadeReadbackBufferID(cascadeID, readbackBufferID);
}

static size_t alignUp(size_t size, size_t alignment = 256)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

static bool isCascadeResource(uint32_t resourceId)
{
    return resourceId == FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_AABB_TREE || resourceId == FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_BRICK_MAP;
}

static bool isScratchResource(uint32_t resourceId)
{
    return resourceId >= FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_COUNTERS && resourceId < FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_COUNTERS + FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES;
}

static bool isExternalResource(uint32_t resourceId)
{
    return resourceId == FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BUFFER
        || resourceId == FFX_BRIXELIZER_RESOURCE_IDENTIFIER_DEBUG_OUTPUT
        || resourceId == FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_SDF_ATLAS
        || resourceId == FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_AABB
        || (FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_AABB_TREES <= resourceId && resourceId < FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_AABB_TREES + FFX_BRIXELIZER_MAX_CASCADES)
        || (FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_BRICK_MAPS <= resourceId && resourceId < FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_BRICK_MAPS + FFX_BRIXELIZER_MAX_CASCADES);
}

static void patchResourceBindings(FfxPipelineState* inoutPipeline)
{
    for (uint32_t srvTextureIndex = 0; srvTextureIndex < inoutPipeline->srvTextureCount; ++srvTextureIndex)
    {
        FfxResourceBinding& binding = inoutPipeline->srvTextureBindings[srvTextureIndex];

        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(srvResourceBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(srvResourceBindingTable[mapIndex].name, binding.name))
                break;
        }
        if (mapIndex == _countof(srvResourceBindingTable))
            return;

        binding.resourceIdentifier = srvResourceBindingTable[mapIndex].index + binding.arrayIndex;
    }

    for (uint32_t srvBufferIndex = 0; srvBufferIndex < inoutPipeline->srvBufferCount; ++srvBufferIndex)
    {
        FfxResourceBinding& binding = inoutPipeline->srvBufferBindings[srvBufferIndex];

        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(srvResourceBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(srvResourceBindingTable[mapIndex].name, binding.name))
                break;
        }
        if (mapIndex == _countof(srvResourceBindingTable))
            return;

        binding.resourceIdentifier = srvResourceBindingTable[mapIndex].index + binding.arrayIndex;
    }

    for (uint32_t uavTextureIndex = 0; uavTextureIndex < inoutPipeline->uavTextureCount; ++uavTextureIndex)
    {
        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(uavResourceBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavResourceBindingTable[mapIndex].name, inoutPipeline->uavTextureBindings[uavTextureIndex].name))
                break;
        }
        if (mapIndex == _countof(uavResourceBindingTable))
            return;

        inoutPipeline->uavTextureBindings[uavTextureIndex].resourceIdentifier = uavResourceBindingTable[mapIndex].index;
    }

    for (uint32_t uavBufferIndex = 0; uavBufferIndex < inoutPipeline->uavBufferCount; ++uavBufferIndex)
    {
        FfxResourceBinding& binding = inoutPipeline->uavBufferBindings[uavBufferIndex];

        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(uavResourceBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(uavResourceBindingTable[mapIndex].name, binding.name))
                break;
        }
        if (mapIndex == _countof(uavResourceBindingTable))
            return;

        binding.resourceIdentifier = uavResourceBindingTable[mapIndex].index + binding.arrayIndex;
    }

    for (uint32_t cbvIndex = 0; cbvIndex < inoutPipeline->constCount; ++cbvIndex)
    {
        FfxResourceBinding& binding = inoutPipeline->constantBufferBindings[cbvIndex];

        int32_t mapIndex = 0;
        for (mapIndex = 0; mapIndex < _countof(cbvResourceBindingTable); ++mapIndex)
        {
            if (0 == wcscmp(cbvResourceBindingTable[mapIndex].name, binding.name))
                break;
        }
        if (mapIndex == _countof(cbvResourceBindingTable))
            return;

        binding.resourceIdentifier = cbvResourceBindingTable[mapIndex].index;
    }
}

static uint32_t getPipelinePermutationFlags(uint32_t contextFlags, bool fp16, bool force64)
{
    // work out what permutation to load.
    uint32_t flags = 0;
    flags |= (force64) ? BRIXELIZER_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (fp16) ? BRIXELIZER_SHADER_PERMUTATION_ALLOW_FP16 : 0;

    return flags;
}

static FfxErrorCode createPipelineStates(FfxBrixelizerRawContext_Private* context)
{
    FFX_ASSERT(context);
    
    const size_t samplerCount = 1;
    FfxSamplerDescription samplers[samplerCount];
    samplers[0].filter       = FFX_FILTER_TYPE_MINMAGMIP_LINEAR;
    samplers[0].addressModeU = FFX_ADDRESS_MODE_CLAMP;
    samplers[0].addressModeV = FFX_ADDRESS_MODE_CLAMP;
    samplers[0].addressModeW = FFX_ADDRESS_MODE_CLAMP;
    samplers[0].stage        = FFX_BIND_COMPUTE_SHADER_STAGE;

    // Query device capabilities
    FfxDeviceCapabilities capabilities;
    context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &capabilities);

    // Setup a few options used to determine permutation flags
    bool haveShaderModel66 = capabilities.maximumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
    bool supportedFP16     = capabilities.fp16Supported;
    bool canForceWave64    = false;

    const uint32_t waveLaneCountMin = capabilities.waveLaneCountMin;
    const uint32_t waveLaneCountMax = capabilities.waveLaneCountMax;
    if (waveLaneCountMin <= 64 && waveLaneCountMax >= 64)
        canForceWave64 = haveShaderModel66;
    else
        canForceWave64 = false;

    // Wave64 disabled due to negative impact on performance
    uint32_t pipelineFlags = getPipelinePermutationFlags(context->contextDescription.flags, supportedFP16, false /*canForceWave64*/); 

    FfxPipelineDescription pipelineDescription;
    memset(&pipelineDescription, 0, sizeof(FfxPipelineDescription));

    // Set up pipeline descriptor (basically RootSignature and binding)
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_MARK_UNINITIALIZED");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_MARK_UNINITIALIZED,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeMarkCascadeUninitialized));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_CLEAR_COUNTERS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_CLEAR_COUNTERS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextClearCounters));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_COLLECT_CLEAR_BRICKS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_COLLECT_CLEAR_BRICKS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextCollectClearBricks));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_CLEAR_BRICKS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_CLEAR_BRICKS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextPrepareClearBricks));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_COLLECT_DIRTY_BRICKS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_COLLECT_DIRTY_BRICKS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextCollectDirtyBricks));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_EIKONAL_ARGS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_EIKONAL_ARGS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextPrepareEikonalArgs));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_MERGE_CASCADES");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_MERGE_CASCADES,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextMergeCascades));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_MERGE_BRICKS_ARGS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_PREPARE_MERGE_BRICKS_ARGS,
                                                                               0,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextPrepareMergeBricksArgs));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_CLEAR_BUILD_COUNTERS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_CLEAR_BUILD_COUNTERS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeClearBuildCounters));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_RESET_CASCADE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_RESET_CASCADE,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeResetCascade));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_SCROLL_CASCADE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_SCROLL_CASCADE,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeScrollCascade));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_CLEAR_REF_COUNTERS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_CLEAR_REF_COUNTERS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeClearRefCounters));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_CLEAR_JOB_COUNTER");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_CLEAR_JOB_COUNTER,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeClearJobCounter));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_INVALIDATE_JOB_AREAS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_INVALIDATE_JOB_AREAS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeInvalidateJobAreas));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_COARSE_CULLING");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_COARSE_CULLING,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeCoarseCulling));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_SCAN_JOBS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_SCAN_JOBS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeScanJobs));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_SCAN_REFERENCES");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_SCAN_REFERENCES,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeScanReferences));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_INITIALIZE_CASCADE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_INITIALIZE_CASCADE,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeInitializeCascade));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_BUILD_TREE_AABB");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_BUILD_TREE_AABB,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeBuildTreeAABB));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_FREE_CASCADE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_FREE_CASCADE,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeFreeCascade));
    pipelineDescription.samplerCount = samplerCount;
    pipelineDescription.samplers     = samplers;
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_DEBUG_VISUALIZATION");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_DEBUG_VISUALIZATION,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineDebugVisualization));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_DEBUG_INSTANCE_AABBS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_DEBUG_INSTANCE_AABBS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineDebugInstanceAABBs));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_DEBUG_DRAW_AABB_TREE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_DEBUG_AABB_TREE,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineDebugDrawAABBTree));
    pipelineDescription.indirectWorkload = 1;
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_CLEAR_BRICK");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_CLEAR_BRICK,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextClearBrick));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_EIKONAL");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_EIKONAL,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextEikonal));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_VOXELIZE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_VOXELIZE,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeVoxelize));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_COMPACT_REFERENCES");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_COMPACT_REFERENCES,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeCompactReferences));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_CLEAR_BRICK_STORAGE");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_CLEAR_BRICK_STORAGE,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeClearBrickStorage));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_EMIT_SDF");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_EMIT_SDF,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeEmitSDF));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CASCADE_COMPRESS_BRICK");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CASCADE_COMPRESS_BRICK,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineCascadeCompressBrick));
    wcscpy_s(pipelineDescription.name, L"FFX_BRIXELIZER_PASS_CONTEXT_MERGE_BRICKS");
    FFX_VALIDATE(context->contextDescription.backendInterface.fpCreatePipeline(&context->contextDescription.backendInterface,
                                                                               FFX_EFFECT_BRIXELIZER,
                                                                               FFX_BRIXELIZER_PASS_CONTEXT_MERGE_BRICKS,
                                                                               pipelineFlags,
                                                                               &pipelineDescription,
                                                                               context->effectContextId,
                                                                               &context->pipelineContextMergeBricks));

    patchResourceBindings(&context->pipelineCascadeMarkCascadeUninitialized);
    patchResourceBindings(&context->pipelineContextClearCounters);
    patchResourceBindings(&context->pipelineContextCollectClearBricks);
    patchResourceBindings(&context->pipelineContextPrepareClearBricks);
    patchResourceBindings(&context->pipelineContextClearBrick);
    patchResourceBindings(&context->pipelineContextCollectDirtyBricks);
    patchResourceBindings(&context->pipelineContextPrepareEikonalArgs);
    patchResourceBindings(&context->pipelineContextEikonal);
    patchResourceBindings(&context->pipelineContextMergeCascades);
    patchResourceBindings(&context->pipelineContextPrepareMergeBricksArgs);
    patchResourceBindings(&context->pipelineContextMergeBricks);
    patchResourceBindings(&context->pipelineCascadeClearBuildCounters);
    patchResourceBindings(&context->pipelineCascadeResetCascade);
    patchResourceBindings(&context->pipelineCascadeScrollCascade);
    patchResourceBindings(&context->pipelineCascadeClearRefCounters);
    patchResourceBindings(&context->pipelineCascadeClearJobCounter);
    patchResourceBindings(&context->pipelineCascadeInvalidateJobAreas);
    patchResourceBindings(&context->pipelineCascadeCoarseCulling);
    patchResourceBindings(&context->pipelineCascadeScanJobs);
    patchResourceBindings(&context->pipelineCascadeVoxelize);
    patchResourceBindings(&context->pipelineCascadeScanReferences);
    patchResourceBindings(&context->pipelineCascadeCompactReferences);
    patchResourceBindings(&context->pipelineCascadeClearBrickStorage);
    patchResourceBindings(&context->pipelineCascadeEmitSDF);
    patchResourceBindings(&context->pipelineCascadeCompressBrick);
    patchResourceBindings(&context->pipelineCascadeInitializeCascade);
    patchResourceBindings(&context->pipelineCascadeBuildTreeAABB);
    patchResourceBindings(&context->pipelineCascadeFreeCascade);
    patchResourceBindings(&context->pipelineDebugVisualization);
    patchResourceBindings(&context->pipelineDebugInstanceAABBs);
    patchResourceBindings(&context->pipelineDebugDrawAABBTree);

    return FFX_OK;
}

static void scheduleDispatchInternal(FfxBrixelizerRawContext_Private*   context,
                                     const FfxPipelineState* pipeline,
                                     uint32_t                dispatchX,
                                     uint32_t                dispatchY,
                                     uint32_t                dispatchZ,
                                     FfxResourceInternal     indirectArgsBuffer,
                                     uint32_t                indirectArgsOffset,
                                     uint32_t                cascadeIdx)
{
    context->gpuJobDescription = {FFX_GPU_JOB_COMPUTE};

    wcscpy_s(context->gpuJobDescription.jobLabel, pipeline->name);
    
    FFX_ASSERT(pipeline->srvTextureCount < FFX_MAX_NUM_SRVS);

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvTextureCount; ++currentShaderResourceViewIndex)
    {
        // TODO: Add inner loop that iterates over the bindCount of each SRV.
        const uint32_t            currentResourceId = pipeline->srvTextureBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource   = context->resources[currentResourceId];
        
        context->gpuJobDescription.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].resource = currentResource;

#ifdef FFX_DEBUG
        wcscpy_s(context->gpuJobDescription.computeJobDescriptor.srvTextures[currentShaderResourceViewIndex].name, pipeline->srvTextureBindings[currentShaderResourceViewIndex].name);
#endif
    }

    FFX_ASSERT(pipeline->srvBufferCount < FFX_MAX_NUM_SRVS);

    for (uint32_t currentShaderResourceViewIndex = 0; currentShaderResourceViewIndex < pipeline->srvBufferCount; ++currentShaderResourceViewIndex)
    {
        const uint32_t            currentResourceId = pipeline->srvBufferBindings[currentShaderResourceViewIndex].resourceIdentifier;
        const FfxResourceInternal currentResource   = context->resources[currentResourceId];
        const BufferBindingInfo   srvInfo = getSRVBindingInfo(context, currentResourceId);

        context->gpuJobDescription.computeJobDescriptor.srvBuffers[currentShaderResourceViewIndex].resource = currentResource;
        context->gpuJobDescription.computeJobDescriptor.srvBuffers[currentShaderResourceViewIndex].offset   = srvInfo.offset;
        context->gpuJobDescription.computeJobDescriptor.srvBuffers[currentShaderResourceViewIndex].size     = srvInfo.size;
        context->gpuJobDescription.computeJobDescriptor.srvBuffers[currentShaderResourceViewIndex].stride   = srvInfo.stride;
#ifdef FFX_DEBUG
        wcscpy_s(context->gpuJobDescription.computeJobDescriptor.srvBuffers[currentShaderResourceViewIndex].name, pipeline->srvBufferBindings[currentShaderResourceViewIndex].name);
#endif
    }

    FFX_ASSERT(pipeline->uavTextureCount < FFX_MAX_NUM_UAVS);

    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavTextureCount; ++currentUnorderedAccessViewIndex)
    {
        const uint32_t baseResourceId    = pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
        const uint32_t            currentResourceId = isCascadeResource(baseResourceId) ? baseResourceId + cascadeIdx : baseResourceId;
        const FfxResourceInternal currentResource   = isScratchResource(baseResourceId) ? context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BUFFER] : context->resources[currentResourceId];

        context->gpuJobDescription.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].resource = currentResource;
        context->gpuJobDescription.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].mip      = 0;
#ifdef FFX_DEBUG
        wcscpy_s(context->gpuJobDescription.computeJobDescriptor.uavTextures[currentUnorderedAccessViewIndex].name, pipeline->uavTextureBindings[currentUnorderedAccessViewIndex].name);
#endif
    }

    FFX_ASSERT(pipeline->uavBufferCount < FFX_MAX_NUM_UAVS);

    for (uint32_t currentUnorderedAccessViewIndex = 0; currentUnorderedAccessViewIndex < pipeline->uavBufferCount; ++currentUnorderedAccessViewIndex)
    {
        const uint32_t            baseResourceId    = pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].resourceIdentifier;
        const uint32_t            currentResourceId = isCascadeResource(baseResourceId) ? baseResourceId + cascadeIdx : baseResourceId;
        const FfxResourceInternal currentResource =
            isScratchResource(baseResourceId) ? context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BUFFER] : context->resources[currentResourceId];

        context->gpuJobDescription.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].resource = currentResource;
        context->gpuJobDescription.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].offset   = context->uavInfo[currentResourceId].offset;
        context->gpuJobDescription.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].size     = context->uavInfo[currentResourceId].size;
        context->gpuJobDescription.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].stride   = context->uavInfo[currentResourceId].stride;
#ifdef FFX_DEBUG
        wcscpy_s(context->gpuJobDescription.computeJobDescriptor.uavBuffers[currentUnorderedAccessViewIndex].name, pipeline->uavBufferBindings[currentUnorderedAccessViewIndex].name);
#endif
    }

    FFX_ASSERT(pipeline->constCount < FFX_MAX_NUM_CONST_BUFFERS);

    for (uint32_t currentConstantBufferViewIndex = 0; currentConstantBufferViewIndex < pipeline->constCount; ++currentConstantBufferViewIndex)
    {
        const uint32_t currentResourceId = pipeline->constantBufferBindings[currentConstantBufferViewIndex].resourceIdentifier;

        uint32_t cbvInfoIdx = 0;

        for (cbvInfoIdx = 0; cbvInfoIdx < 4; cbvInfoIdx++)
        {
            if (cbvResourceBindingTable[cbvInfoIdx].index == currentResourceId)
                break;
        }

        FFX_ASSERT(cbvInfoIdx == currentResourceId);

        context->gpuJobDescription.computeJobDescriptor.cbs[currentConstantBufferViewIndex] = context->constantBuffers[cbvInfoIdx];
#ifdef FFX_DEBUG
        wcscpy_s(context->gpuJobDescription.computeJobDescriptor.cbNames[currentConstantBufferViewIndex], pipeline->constantBufferBindings[currentConstantBufferViewIndex].name);
#endif
    }

    context->gpuJobDescription.computeJobDescriptor.dimensions[0]     = dispatchX;
    context->gpuJobDescription.computeJobDescriptor.dimensions[1]     = dispatchY;
    context->gpuJobDescription.computeJobDescriptor.dimensions[2]     = dispatchZ;
    context->gpuJobDescription.computeJobDescriptor.pipeline          = *pipeline;
    context->gpuJobDescription.computeJobDescriptor.cmdArgument       = indirectArgsBuffer;
    context->gpuJobDescription.computeJobDescriptor.cmdArgumentOffset = indirectArgsOffset;

    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &context->gpuJobDescription);
}

static void scheduleCopy(FfxBrixelizerRawContext_Private* context, FfxResourceInternal src, uint32_t srcOffset, FfxResourceInternal dst, uint32_t dstOffset, uint32_t size, const wchar_t* name)
{
    context->gpuJobDescription = {FFX_GPU_JOB_COPY};

    wcscpy_s(context->gpuJobDescription.jobLabel, name);
    
    context->gpuJobDescription.copyJobDescriptor.src       = src;
    context->gpuJobDescription.copyJobDescriptor.srcOffset = srcOffset;
    context->gpuJobDescription.copyJobDescriptor.dst       = dst;
    context->gpuJobDescription.copyJobDescriptor.dstOffset = dstOffset;
    context->gpuJobDescription.copyJobDescriptor.size      = size;

    context->contextDescription.backendInterface.fpScheduleGpuJob(&context->contextDescription.backendInterface, &context->gpuJobDescription);
}

static void scheduleDispatch(FfxBrixelizerRawContext_Private* context, const FfxPipelineState* pipeline, uint32_t dispatchX, uint32_t dispatchY, uint32_t dispatchZ, uint32_t cascadeIdx)
{
    scheduleDispatchInternal(context, pipeline, dispatchX, dispatchY, dispatchZ, {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_NULL}, 0, cascadeIdx);
}

static void scheduleIndirectDispatch(FfxBrixelizerRawContext_Private*   context,
                                     const FfxPipelineState* pipeline,
                                     FfxResourceInternal     indirectArgsBuffer, 
                                     uint32_t                indirectArgsOffset, 
                                     uint32_t                cascadeIdx)
{
    scheduleDispatchInternal(context, pipeline, 0, 0, 0, indirectArgsBuffer, indirectArgsOffset, cascadeIdx);
}

static FfxBrixelizerContextInfo getContextInfo(FfxBrixelizerRawContext_Private* context)
{
    FfxBrixelizerContextInfo info = {};
    for (int i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; i++)
    {
        if (context->cascades[i].isAllocated)
            info.cascades[i] = context->cascades[i].info;
    }
    info.num_bricks  = FFX_BRIXELIZER_MAX_BRICKS_X8;
    info.mesh_unit   = 0.0f;
    info.imesh_unit  = 0.0f;
    info.frame_index = context->frameIndex;
    return info;
}

static size_t getTotalScratchMemorySize(FfxBrixelizerScratchPartition* scratchPartition)
{
    return (size_t)(scratchPartition->array[FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES * 2 - 1] + scratchPartition->array[FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES - 1]);
}

static size_t getScratchMemorySize(FfxBrixelizerRawContext_Private*                context,
                                   const FfxBrixelizerRawCascadeUpdateDescription* cascadeUpdateDescription,
                                   FfxBrixelizerScratchPartition*               outScratchPartition)
{
    FfxBrixelizerScratchPartition scratchPartition = {};
    uint32_t              num_total_items  = ((uint32_t)cascadeUpdateDescription->numJobs + (uint32_t)context->numInstances);
    FfxBrixelizerCascadeInfo      cinfo            = context->cascades[cascadeUpdateDescription->cascadeIndex].info;
    uint32_t              total_texel_cnt  = FFX_BRIXELIZER_MAX_BRICKS_X8 * 8 * 8 * 8;

    scratchPartition.counters_size                      = FFX_BRIXELIZER_NUM_SCRATCH_COUNTERS * sizeof(uint32_t);
    scratchPartition.bricks_compression_list_size       = FFX_BRIXELIZER_MAX_BRICKS_X8 * sizeof(uint32_t);
    scratchPartition.triangle_swap_size                 = cascadeUpdateDescription->triangleSwapSize;
    uint32_t brick_32bit_storage_size = cascadeUpdateDescription->maxBricksPerBake * 8 * 8 * 8 * sizeof(uint32_t);
    scratchPartition.bricks_storage_size                = ffxMin(brick_32bit_storage_size, total_texel_cnt * (uint32_t)sizeof(uint32_t));
    scratchPartition.bricks_storage_offsets_size        = scratchPartition.bricks_compression_list_size;
    scratchPartition.bricks_clear_list_size             = scratchPartition.bricks_compression_list_size;
    scratchPartition.job_counters_size                  = num_total_items * (uint32_t)sizeof(uint32_t);
    scratchPartition.voxel_allocation_fail_counter_size = FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * (uint32_t)sizeof(uint32_t);
    scratchPartition.job_counters_scan_size             = num_total_items * sizeof(uint32_t);
    scratchPartition.job_global_counters_scan_size      = (num_total_items + FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE * (uint32_t)sizeof(uint32_t);
    scratchPartition.cr1_references_size                = sizeof(FfxBrixelizerTriangleReference) * cascadeUpdateDescription->maxReferences;
    scratchPartition.cr1_compacted_references_size      = (uint32_t)sizeof(uint32_t) * cascadeUpdateDescription->maxReferences;
    scratchPartition.cr1_ref_counters_size              = FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * (uint32_t)sizeof(uint32_t);
    scratchPartition.cr1_ref_counter_scan_size          = FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * (uint32_t)sizeof(uint32_t);
    scratchPartition.cr1_ref_global_scan_size           = (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION + FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE * (uint32_t)sizeof(uint32_t);
    scratchPartition.cr1_stamp_scan_size                = FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * (uint32_t)sizeof(uint32_t);
    scratchPartition.cr1_stamp_global_scan_size         = (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION + FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE * (uint32_t)sizeof(uint32_t);
    if (context->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_AABBS) {
        scratchPartition.debug_aabbs_size               = sizeof(FfxBrixelizerDebugAABB) * context->contextDescription.maxDebugAABBs;
    }

    uint32_t* pArray = scratchPartition.array;

    pArray[FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES] = 0;

    for(uint32_t i = 0; i < FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES - 1; i++)
    {
        // Don't assign anything with zero size
        if (pArray[i] == 0) {
            pArray[i] = 64;
        }
        pArray[FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES + i + 1] = (uint32_t)alignUp(pArray[FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES + i] + pArray[i]);
    }

    if (outScratchPartition)
        *outScratchPartition = scratchPartition;

    return getTotalScratchMemorySize(&scratchPartition);
}

static FfxBrixelizerInstanceID popBackHostNewInstanceList(FfxBrixelizerRawContext_Private* context)
{
    if (context->hostNewInstanceListSize > 0)
    {
        context->hostNewInstanceListSize--;
        return context->hostNewInstanceList[context->hostNewInstanceListSize];
    }
    else
        return FFX_BRIXELIZER_INVALID_ID;
}

static void clearHostNewInstanceList(FfxBrixelizerRawContext_Private* context)
{
    context->hostNewInstanceListSize = 0;
}

static FfxBrixelizerInstanceInfo* getFlatInstancePtr(FfxBrixelizerRawContext_Private* context)
{
    return &context->hostInstances[0];
}

static FfxFloat32x3x4* getFlatTransformPtr(FfxBrixelizerRawContext_Private* context)
{
    return &context->hostTransforms[0];
}

static uint32_t copyToUploadBuffer(FfxBrixelizerRawContext_Private* context, uint32_t id, void* data, size_t size, size_t alignedSize = 0)
{
    if (id >= FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_INSTANCE_INFO_BUFFER)
    {
        uint32_t stagingId = getUploadBufferID(id);
        uint8_t* ptr       = context->uploadBufferMappedPointers[stagingId];
        uint32_t offset    = context->uploadBufferOffsets[stagingId];
        uint32_t totalSize = context->uploadBufferSizes[stagingId];

        // If there's not enough space, wrap the offset back to the beginning.
        if ((totalSize - offset) < size)
        {
            context->uploadBufferOffsets[stagingId] = 0;
            offset                                  = 0;
        }

        ptr += offset;

        memcpy(ptr, data, size);

        context->uploadBufferOffsets[stagingId] += alignedSize == 0 ? size : alignedSize;

        return offset;
    }
    else
        return 0;
}

static void updateConstantBuffer(FfxBrixelizerRawContext_Private* context, uint32_t id, void* data)
{
    context->contextDescription.backendInterface.fpStageConstantBufferDataFunc(&context->contextDescription.backendInterface, data, cbSizes[id], &context->constantBuffers[id]);
}

static FfxErrorCode brixelizerCreate(FfxBrixelizerRawContext_Private* context, const FfxBrixelizerRawContextDescription* contextDescription)
{
    FFX_ASSERT(context);
    FFX_ASSERT(contextDescription);

    // Setup the data for implementation.
    memset(context, 0, sizeof(FfxBrixelizerRawContext_Private));
    context->device = contextDescription->backendInterface.device;

    memcpy(&context->contextDescription, contextDescription, sizeof(FfxBrixelizerRawContextDescription));

    // Check version info - make sure we are linked with the right backend version
    FfxVersionNumber version = context->contextDescription.backendInterface.fpGetSDKVersion(&context->contextDescription.backendInterface);
    FFX_RETURN_ON_ERROR(version == FFX_SDK_MAKE_VERSION(1, 1, 2), FFX_ERROR_INVALID_VERSION);

    // Specify bindless requirements.
    FfxEffectBindlessConfig bindlessConfig = {};
    bindlessConfig.maxBufferSrvs = FFX_BRIXELIZER_STATIC_CONFIG_MAX_VERTEX_BUFFERS;

    // Create the device.
    FfxErrorCode errorCode =
        context->contextDescription.backendInterface.fpCreateBackendContext(&context->contextDescription.backendInterface, FFX_EFFECT_BRIXELIZER, &bindlessConfig, &context->effectContextId);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    // call out for device caps.
    errorCode = context->contextDescription.backendInterface.fpGetDeviceCapabilities(&context->contextDescription.backendInterface, &context->deviceCapabilities);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    if (!context->deviceCapabilities.shaderStorageBufferArrayNonUniformIndexing)
        return FFX_ERROR_INVALID_ARGUMENT; // unsupported device

    errorCode = createPipelineStates(context);
    FFX_RETURN_ON_ERROR(errorCode == FFX_OK, errorCode);

    context->totalBricks             = FFX_BRIXELIZER_MAX_BRICKS_X8;
    context->frameIndex              = 0;
    context->doInit                  = true;
    context->numInstances            = 0;
    context->hostFreelistSize        = FFX_BRIXELIZER_MAX_INSTANCES;
    context->hostNewInstanceListSize = 0;
    context->bufferIndexFreeListSize = FFX_BRIXELIZER_MAX_INSTANCES;

    // Fill out Instance ID freelist.
    for (uint32_t i = 0; i < FFX_BRIXELIZER_MAX_INSTANCES; i++)
    {
        context->hostFreelist[i]        = (FFX_BRIXELIZER_MAX_INSTANCES - i) - 1;
        context->bufferIndexFreeList[i] = (FFX_BRIXELIZER_MAX_INSTANCES - i) - 1;
    }

    const FfxResourceInitData initData = {FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED, 0, nullptr};

    // Create GPU-local resources.
    {
        const FfxInternalResourceDescription internalSurfaceDesc[] = {
            {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INSTANCE_INFO_BUFFER,
             L"Brixelizer_InstanceBuffer",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             FFX_BRIXELIZER_MAX_INSTANCES * sizeof(FfxBrixelizerInstanceInfo),
             sizeof(FfxBrixelizerInstanceInfo),
             1,
             FFX_RESOURCE_FLAGS_NONE},
            {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INSTANCE_TRANSFORM_BUFFER,
             L"Brixelizer_TransformBuffer",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             FFX_BRIXELIZER_MAX_INSTANCES * sizeof(FfxFloat32x3x4),
             sizeof(FfxFloat32x4),
             1,
             FFX_RESOURCE_FLAGS_NONE},
            {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1,
             L"Brixelizer_IndirectArgs1",
             FFX_RESOURCE_TYPE_BUFFER,
             FfxResourceUsage(FFX_RESOURCE_USAGE_UAV | FFX_RESOURCE_USAGE_INDIRECT),
             FFX_SURFACE_FORMAT_R32_FLOAT,
             FFX_BRIXELIZER_NUM_INDIRECT_OFFSETS * FFX_BRIXELIZER_STATIC_CONFIG_INDIRECT_DISPATCH_STRIDE,
             sizeof(uint32_t),
             1,
             FFX_RESOURCE_FLAGS_NONE},
            {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_VOXEL_MAP,
             L"Brixelizer_BrickVoxelMap",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             context->totalBricks * sizeof(uint32_t),
             sizeof(uint32_t),
             1,
             FFX_RESOURCE_FLAGS_NONE},
            {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_FREE_LIST,
             L"Brixelizer_BrickFreeList",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             context->totalBricks * sizeof(uint32_t),
             sizeof(uint32_t),
             1,
             FFX_RESOURCE_FLAGS_NONE},
            {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_CLEAR_LIST,
             L"Brixelizer_BrickClearList",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             context->totalBricks * sizeof(uint32_t),
             sizeof(uint32_t),
             1,
             FFX_RESOURCE_FLAGS_NONE},
            {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_EIKONAL_LIST,
             L"Brixelizer_BrickEikonalList",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             context->totalBricks * sizeof(uint32_t),
             sizeof(uint32_t),
             1,
             FFX_RESOURCE_FLAGS_NONE},
            { FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_MERGE_LIST,
             L"Brixelizer_BrickMergeList",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             context->totalBricks * sizeof(uint32_t) * 2,
             sizeof(uint32_t),
             1,
             FFX_RESOURCE_FLAGS_NONE },
            { FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_EIKONAL_COUNTERS,
             L"Brixelizer_BrickEikonalCounters",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             context->totalBricks * sizeof(uint32_t),
             sizeof(uint32_t),
             1,
             FFX_RESOURCE_FLAGS_NONE },
            { FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS,
             L"Brixelizer_Counters",
             FFX_RESOURCE_TYPE_BUFFER,
             FFX_RESOURCE_USAGE_UAV,
             FFX_SURFACE_FORMAT_R32_FLOAT,
             FFX_BRIXELIZER_NUM_CONTEXT_COUNTERS * sizeof(uint32_t),
             sizeof(uint32_t),
             1,
             FFX_RESOURCE_FLAGS_NONE },
        };

        for (int32_t currentSurfaceIndex = 0; currentSurfaceIndex < FFX_ARRAY_ELEMENTS(internalSurfaceDesc); ++currentSurfaceIndex)
        {
            const FfxInternalResourceDescription* currentSurfaceDescription = &internalSurfaceDesc[currentSurfaceIndex];
            const FfxResourceType                 resourceType = FFX_RESOURCE_TYPE_BUFFER;
            const FfxResourceDescription          resourceDescription = {
                resourceType,
                currentSurfaceDescription->format,
                currentSurfaceDescription->width,
                currentSurfaceDescription->height,
                currentSurfaceDescription->mipCount,  // Width in the case of the SDF Atlas
                1,
                currentSurfaceDescription->flags,
                currentSurfaceDescription->usage};

            FfxResourceStates initialState = (currentSurfaceDescription->usage == FFX_RESOURCE_USAGE_READ_ONLY) ? FFX_RESOURCE_STATE_COMPUTE_READ : FFX_RESOURCE_STATE_UNORDERED_ACCESS;

            switch (internalSurfaceDesc[currentSurfaceIndex].id)
            {
            case FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INSTANCE_INFO_BUFFER:
            case FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INSTANCE_TRANSFORM_BUFFER:
            {
                initialState = FFX_RESOURCE_STATE_COPY_DEST;
                break;
            }
            case FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS:
            {
                initialState = FFX_RESOURCE_STATE_COPY_SRC;
                break;
            }
            default:
            {
                initialState = FFX_RESOURCE_STATE_UNORDERED_ACCESS;
                break;
            }
            }

            const FfxCreateResourceDescription createResourceDescription = { FFX_HEAP_TYPE_DEFAULT,
                                                                            resourceDescription,
                                                                            initialState,
                                                                            currentSurfaceDescription->name,
                                                                            currentSurfaceDescription->id, 
                                                                            initData};

            memset(&context->resources[currentSurfaceDescription->id], 0, sizeof(FfxResourceInternal));

            FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, 
                                                                                       &createResourceDescription, 
                                                                                       context->effectContextId, 
                                                                                       &context->resources[currentSurfaceDescription->id]));
        }
    }
    
    // Create readback resources.
    if (contextDescription->flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CONTEXT_READBACK_BUFFERS)
    {
        const uint32_t sizes[] = {
            FFX_BRIXELIZER_NUM_CONTEXT_COUNTERS * sizeof(uint32_t), FFX_BRIXELIZER_NUM_CONTEXT_COUNTERS * sizeof(uint32_t), FFX_BRIXELIZER_NUM_CONTEXT_COUNTERS * sizeof(uint32_t)};
        const uint32_t ids[]   = {FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_0,
                                FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_1,
                                FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_2};
        const wchar_t* names[] = {L"Brixelizer_CountersReadback0", L"Brixelizer_CountersReadback1", L"Brixelizer_CountersReadback2"};

        for (int32_t currentBufferIndex = 0; currentBufferIndex < FFX_ARRAY_ELEMENTS(ids); ++currentBufferIndex)
        {
            const FfxResourceType              resourceType              = FFX_RESOURCE_TYPE_BUFFER;
            const FfxResourceDescription       resourceDescription       = {resourceType, 
                                                                            FFX_SURFACE_FORMAT_R32_FLOAT, 
                                                                            sizes[currentBufferIndex], 
                                                                            1, 
                                                                            1, 
                                                                            1,
                                                                            FFX_RESOURCE_FLAGS_NONE,
                                                                            FFX_RESOURCE_USAGE_READ_ONLY};
            const FfxCreateResourceDescription createResourceDescription = {FFX_HEAP_TYPE_READBACK,
                                                                            resourceDescription,
                                                                            FFX_RESOURCE_STATE_COPY_DEST,
                                                                            names[currentBufferIndex],
                                                                            ids[currentBufferIndex],
                                                                            initData};

            memset(&context->resources[ids[currentBufferIndex]], 0, sizeof(FfxResourceInternal));

            FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface,
                                                                                       &createResourceDescription,
                                                                                       context->effectContextId,
                                                                                       &context->resources[ids[currentBufferIndex]]));

            FFX_VALIDATE(context->contextDescription.backendInterface.fpMapResource(&context->contextDescription.backendInterface,
                                                                                    context->resources[ids[currentBufferIndex]],
                                                                                    (void**)&context->readbackBufferMappedPointers[currentBufferIndex]));  
        } 
    }

    // Create upload resources.
    {
        for (int32_t currentBufferIndex = 0; currentBufferIndex < FFX_BRIXELIZER_NUM_UPLOAD_BUFFERS; ++currentBufferIndex)
        {
            const FfxBrixelizerUploadBufferMetaData *metaData = &uploadBufferMetaData[currentBufferIndex];

            if (!(contextDescription->flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_AABBS)
                && metaData->id == FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_DEBUG_INSTANCE_ID_BUFFER) {
                continue;
            }

            const FfxResourceType              resourceType              = FFX_RESOURCE_TYPE_BUFFER;
            const FfxResourceDescription     resourceDescription = {
                resourceType, 
                FFX_SURFACE_FORMAT_R32_FLOAT, 
                metaData->size, 
                metaData->stride, 
                1, 
                1, 
                FFX_RESOURCE_FLAGS_NONE, 
                metaData->usage};
            const FfxCreateResourceDescription createResourceDescription = {FFX_HEAP_TYPE_UPLOAD,
                                                                            resourceDescription,
                                                                            metaData->state,
                                                                            metaData->name,
                                                                            metaData->id, 
                                                                            initData};

            memset(&context->resources[metaData->id], 0, sizeof(FfxResourceInternal));

            FFX_VALIDATE(context->contextDescription.backendInterface.fpCreateResource(&context->contextDescription.backendInterface, 
                                                                                       &createResourceDescription, 
                                                                                       context->effectContextId, 
                                                                                       &context->resources[metaData->id]));

            FFX_VALIDATE(context->contextDescription.backendInterface.fpMapResource(&context->contextDescription.backendInterface,
                                                                       context->resources[metaData->id],
                                                                       (void**)& context->uploadBufferMappedPointers[currentBufferIndex]));

            context->uploadBufferSizes[currentBufferIndex] = metaData->size;
            context->uploadBufferOffsets[currentBufferIndex] = 0;
        }
    }

    return FFX_OK;
}

static FfxErrorCode brixelizerRelease(FfxBrixelizerRawContext_Private* context)
{
    FFX_ASSERT(context);

    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeMarkCascadeUninitialized, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextClearCounters, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextCollectClearBricks, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextPrepareClearBricks, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextClearBrick, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextCollectDirtyBricks, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextPrepareEikonalArgs, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextEikonal, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextMergeCascades, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextPrepareMergeBricksArgs, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineContextMergeBricks, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeClearBuildCounters, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeResetCascade, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeScrollCascade, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeClearRefCounters, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeClearJobCounter, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeInvalidateJobAreas, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeCoarseCulling, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeScanJobs, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeVoxelize, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeScanReferences, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeCompactReferences, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeClearBrickStorage, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeEmitSDF, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeCompressBrick, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeInitializeCascade, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeBuildTreeAABB, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineCascadeFreeCascade, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineDebugVisualization, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineDebugInstanceAABBs, context->effectContextId);
    ffxSafeReleasePipeline(&context->contextDescription.backendInterface, &context->pipelineDebugDrawAABBTree, context->effectContextId);

    // Unmap buffers.
    {
        for (int32_t currentBufferIndex = 0; currentBufferIndex < FFX_ARRAY_ELEMENTS(uploadBufferMetaData); ++currentBufferIndex) {
            const FfxBrixelizerUploadBufferMetaData *metaData = &uploadBufferMetaData[currentBufferIndex];
            if (!(context->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_AABBS)
                && metaData->id == FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_DEBUG_INSTANCE_ID_BUFFER) {
                continue;
            }
            context->contextDescription.backendInterface.fpUnmapResource(&context->contextDescription.backendInterface, context->resources[metaData->id]);
        }

        if (context->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CONTEXT_READBACK_BUFFERS)
        {
            context->contextDescription.backendInterface.fpUnmapResource(&context->contextDescription.backendInterface, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_0]);
            context->contextDescription.backendInterface.fpUnmapResource(&context->contextDescription.backendInterface, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_1]);
            context->contextDescription.backendInterface.fpUnmapResource(&context->contextDescription.backendInterface, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_2]);
        }

        if (context->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CASCADE_READBACK_BUFFERS) {
            for (uint32_t i = 0; i < FFX_BRIXELIZER_MAX_CASCADES; ++i) {
                if (context->cascades[i].isAllocated) {
                    for (uint32_t j = 0; j < 3; ++j) {
                        uint32_t readbackBufferResourceID = getCascadeReadbackBufferResourceID(i, j);
                        context->contextDescription.backendInterface.fpUnmapResource(&context->contextDescription.backendInterface, context->resources[readbackBufferResourceID]);
                    }
                }
            }
        }
    }

    // Release internal resources.
    for (int32_t currentResourceIndex = FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_AABB_TREE; currentResourceIndex < FFX_BRIXELIZER_RESOURCE_IDENTIFIER_COUNT; ++currentResourceIndex)
    {
        if (!isExternalResource(currentResourceIndex))
            ffxSafeReleaseResource(&context->contextDescription.backendInterface, context->resources[currentResourceIndex], context->effectContextId);
    }

    // Destroy the context.
    context->contextDescription.backendInterface.fpDestroyBackendContext(&context->contextDescription.backendInterface, context->effectContextId);

    return FFX_OK;
}

static FfxErrorCode brixelizerDispatchResetCascade(FfxBrixelizerRawContext_Private* context, uint32_t cascadeIndex)
{
    FfxBrixelizerCascade_Private *cascade = &context->cascades[cascadeIndex];

    if (!cascade->isAllocated)
        return FFX_ERROR_INVALID_ARGUMENT;

    FFX_RETURN_ON_ERROR(context->device, FFX_ERROR_NULL_DEVICE);

    FfxBrixelizerContextInfo contextInfo = getContextInfo(context);
    FfxBrixelizerCascadeInfo cascadeInfo = cascade->info;

    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CONTEXT_INFO, &contextInfo);
    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CASCADE_INFO, &cascadeInfo);

    scheduleDispatch(context, &context->pipelineCascadeFreeCascade, FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION / 64, 1, 1, cascadeInfo.index);

    return FFX_OK;
}

static FfxErrorCode brixelizerDispatchBegin(FfxBrixelizerRawContext_Private* context, FfxBrixelizerResources resources)
{
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface,
                                                                    &resources.sdfAtlas,
                                                                    context->effectContextId,
                                                                    &context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_SDF_ATLAS]);
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface,
                                                                    &resources.brickAABBs,
                                                                    context->effectContextId,
                                                                    &context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_AABB]);
    setUAVBindingInfo(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_BRICKS_AABB, 0, context->totalBricks * sizeof(uint32_t), sizeof(uint32_t));

    for (uint32_t i = 0; i < FFX_ARRAY_ELEMENTS(context->cascades); ++i) {
        FfxBrixelizerCascadeResources *cascade = &resources.cascadeResources[i];
        if (ffxBrixelizerRawResourceIsNull(cascade->aabbTree) || ffxBrixelizerRawResourceIsNull(cascade->brickMap)) {
            continue;
        }

        context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface,
                                                                        &cascade->aabbTree,
                                                                        context->effectContextId,
                                                                        &context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_AABB_TREE + i]);
        context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface,
                                                                        &cascade->brickMap,
                                                                        context->effectContextId,
                                                                        &context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_BRICK_MAP + i]);

        setUAVBindingInfo(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_AABB_TREE + i, 0, FFX_BRIXELIZER_CASCADE_AABB_TREE_SIZE, FFX_BRIXELIZER_CASCADE_AABB_TREE_STRIDE);
        setUAVBindingInfo(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CASCADE_BRICK_MAP + i, 0, FFX_BRIXELIZER_CASCADE_BRICK_MAP_SIZE, FFX_BRIXELIZER_CASCADE_BRICK_MAP_STRIDE);
    }
 
    context->frameIndex += 1;

    FfxBrixelizerContextInfo contextInfo = getContextInfo(context);

    FfxBrixelizerBuildInfo buildInfo  = {};
    buildInfo.do_initialization = context->doInit ? 1 : 0;

    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CONTEXT_INFO, &contextInfo);
    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, &buildInfo);

    scheduleDispatch(context, &context->pipelineContextClearCounters, 1, 1, 1, 0);

    if (context->totalBricks > 0)
        scheduleDispatch(context, &context->pipelineContextCollectClearBricks, (context->totalBricks + 63) / 64, 1, 1, 0);

    scheduleDispatch(context, &context->pipelineContextPrepareClearBricks, 1, 1, 1, 0);
    scheduleIndirectDispatch(context, &context->pipelineContextClearBrick, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1], FFX_BRIXELIZER_INDIRECT_OFFSETS_CLEAR_BRICKS, 0);

    for (uint32_t i = 0; i < FFX_ARRAY_ELEMENTS(context->cascades); ++i) {
        FfxBrixelizerCascade_Private *cascade = &context->cascades[i];
        if (!cascade->isAllocated || cascade->info.is_initialized) {
            continue;
        }
        updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CASCADE_INFO, &cascade->info);
        scheduleDispatch(context, &context->pipelineCascadeMarkCascadeUninitialized, (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION + 63) / 64, 1, 1, cascade->info.index);
        cascade->info.is_initialized = 1;
    }

    context->doInit = false;

    context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BUFFER].internalIndex = UINT32_MAX;

    return FFX_OK;
}

static FfxErrorCode brixelizerDispatchEnd(FfxBrixelizerRawContext_Private* context)
{
    FfxBrixelizerBuildInfo buildInfo = {};

    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, &buildInfo);

    scheduleDispatch(context, &context->pipelineContextCollectDirtyBricks, (context->totalBricks + 63) / 64, 1, 1, 0);
    scheduleDispatch(context, &context->pipelineContextPrepareEikonalArgs, 1, 1, 1, 0);
    scheduleIndirectDispatch(context, &context->pipelineContextEikonal, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1], FFX_BRIXELIZER_INDIRECT_OFFSETS_EIKONAL, 0);

    if (context->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CONTEXT_READBACK_BUFFERS)
    {
        FfxResourceInternal counterReadbackBuffers[3] = {
            context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_0],
            context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_1],
            context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS_READBACK_2]
        };

        // Readback counter data
        memcpy(&context->debugCounters, context->readbackBufferMappedPointers[context->frameIndex % 3], sizeof(FfxBrixelizerDebugCounters));

        scheduleCopy(context,
                     context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_CONTEXT_COUNTERS],
                     0,
                     counterReadbackBuffers[context->frameIndex % 3],
                     0,
                     sizeof(FfxBrixelizerDebugCounters),
                     L"Copy Debug Counters");
    }

    return FFX_OK;
}

static FfxErrorCode brixelizerSubmit(FfxBrixelizerRawContext_Private* context, FfxCommandList cmdList)
{
    // Execute jobs
    context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, cmdList, context->effectContextId);

    // Release dynamic resources
    context->contextDescription.backendInterface.fpUnregisterResources(&context->contextDescription.backendInterface, cmdList, context->effectContextId);
    for (uint32_t i = 0; i < FFX_ARRAY_ELEMENTS(context->cascades); ++i) {
        context->cascades[i].resourcesRegistered = false;
    }

    return FFX_OK;
}

static FfxErrorCode brixelizerDispatchUpdateCascade(FfxBrixelizerRawContext_Private* context, const FfxBrixelizerRawCascadeUpdateDescription* desc)
{
    FfxBrixelizerCascade_Private* cascade     = (FfxBrixelizerCascade_Private*)&context->cascades[desc->cascadeIndex];
    FfxBrixelizerContextInfo      contextinfo = getContextInfo(context);

    {  
        // Update cascade parameters
        FfxBrixelizerCascadeInfo cascadeInfo = cascade->info;

        int clipmapInvalidationOffset[3] = {
            (int32_t)desc->clipmapOffset[0] - (int32_t)cascadeInfo.ioffset[0],
            (int32_t)desc->clipmapOffset[1] - (int32_t)cascadeInfo.ioffset[1],
            (int32_t)desc->clipmapOffset[2] - (int32_t)cascadeInfo.ioffset[2],
        };

        cascade->info.flags                        = (uint32_t)desc->flags;
        cascade->info.clipmap_invalidation_offset[0] = clipmapInvalidationOffset[0];
        cascade->info.clipmap_invalidation_offset[1] = clipmapInvalidationOffset[1];
        cascade->info.clipmap_invalidation_offset[2] = clipmapInvalidationOffset[2];

        cascade->info.ioffset[0] = desc->clipmapOffset[0];
        cascade->info.ioffset[1] = desc->clipmapOffset[1];
        cascade->info.ioffset[2] = desc->clipmapOffset[2];

        cascade->info.clipmap_offset[0] = ((uint32_t)desc->clipmapOffset[0]) % FFX_BRIXELIZER_CASCADE_RESOLUTION;
        cascade->info.clipmap_offset[1] = ((uint32_t)desc->clipmapOffset[1]) % FFX_BRIXELIZER_CASCADE_RESOLUTION;
        cascade->info.clipmap_offset[2] = ((uint32_t)desc->clipmapOffset[2]) % FFX_BRIXELIZER_CASCADE_RESOLUTION;

        cascade->info.grid_min[0] = desc->cascadeMin[0];
        cascade->info.grid_min[1] = desc->cascadeMin[1];
        cascade->info.grid_min[2] = desc->cascadeMin[2];

        cascade->info.grid_max[0] = desc->cascadeMin[0] + cascadeInfo.voxel_size * FFX_BRIXELIZER_CASCADE_RESOLUTION;
        cascade->info.grid_max[1] = desc->cascadeMin[1] + cascadeInfo.voxel_size * FFX_BRIXELIZER_CASCADE_RESOLUTION;
        cascade->info.grid_max[2] = desc->cascadeMin[2] + cascadeInfo.voxel_size * FFX_BRIXELIZER_CASCADE_RESOLUTION;

        for (uint32_t i = 0; i < 3; i++)
            cascade->info.grid_mid[i] = (cascade->info.grid_max[i] + cascade->info.grid_min[i]) * 0.5f;
    }

    FfxBrixelizerCascadeInfo cascadeInfo = cascade->info;

    uint32_t numJobs    = 0;
    uint32_t voxelCount = 0;
    
    for (uint32_t i = 0; i < desc->numJobs; i++)
    {
        FfxBrixelizerRawJobDescription const& apiJob        = desc->jobs[i];
        FfxBrixelizerBrixelizationJob&        job           = context->jobs[numJobs];
        float                      inflationSize = cascadeInfo.voxel_size;

        if (                                                               // Out of bounds
            apiJob.aabbMax[0] < cascadeInfo.grid_min[0] - inflationSize ||  //
            apiJob.aabbMax[1] < cascadeInfo.grid_min[1] - inflationSize ||  //
            apiJob.aabbMax[2] < cascadeInfo.grid_min[2] - inflationSize ||  //
            apiJob.aabbMin[0] > cascadeInfo.grid_max[0] + inflationSize ||  //
            apiJob.aabbMin[1] > cascadeInfo.grid_max[1] + inflationSize ||  //
            apiJob.aabbMin[2] > cascadeInfo.grid_max[2] + inflationSize     //
        )
            continue;

        if (                                           // Zero volume
            apiJob.aabbMax[0] == apiJob.aabbMin[0]     //
            && apiJob.aabbMax[1] == apiJob.aabbMin[1]  //
            && apiJob.aabbMax[2] == apiJob.aabbMin[2]  //
        )
            continue;

        uint32_t aabbMin[3] = {};
        uint32_t aabbMax[3] = {};

        for(uint32_t j = 0; j < 3; j++)
            aabbMin[j] = uint32_t(ffxMax(0.0f, ffxMin((apiJob.aabbMin[j] - inflationSize - cascadeInfo.grid_min[j]) / cascadeInfo.voxel_size, (float)FFX_BRIXELIZER_CASCADE_RESOLUTION - 1)));

        for (uint32_t j = 0; j < 3; j++)
            aabbMax[j] = uint32_t(ffxMax(0.0f, ffxMin((apiJob.aabbMax[j] + inflationSize - cascadeInfo.grid_min[j]) / cascadeInfo.voxel_size, (float)FFX_BRIXELIZER_CASCADE_RESOLUTION - 1))) + 1u;
        
        for (uint32_t j = 0; j < 3; j++)
            job.aabbMin[j] = aabbMin[j];

        for (uint32_t j = 0; j < 3; j++)
            job.aabbMax[j] = aabbMax[j];

        job.flags = 0;
        job.flags |= (apiJob.flags & FFX_BRIXELIZER_RAW_JOB_FLAG_INVALIDATE) ? FFX_BRIXELIZER_JOB_FLAG_INVALIDATE : 0;

        job.instanceIdx = apiJob.instanceIdx;

        context->indexOffsets[numJobs] = voxelCount;
        numJobs++;
        
        int32_t dim[3] = {
            (int32_t)job.aabbMax[0] - (int32_t)job.aabbMin[0],
            (int32_t)job.aabbMax[1] - (int32_t)job.aabbMin[1],
            (int32_t)job.aabbMax[2] - (int32_t)job.aabbMin[2],
        };

        FFX_ASSERT(dim[0] > 0 && dim[1] > 0 && dim[2] > 0);

        voxelCount += (uint32_t)(dim[0] * dim[1] * dim[2]);
    }

    uint32_t jobBufferSize   = (numJobs ? numJobs : 1) * sizeof(FfxBrixelizerBrixelizationJob);
    uint32_t jobBufferOffset = copyToUploadBuffer(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_JOB_BUFFER, &context->jobs[0], jobBufferSize, alignUp(jobBufferSize));

    uint32_t jobIndexBufferSize = (numJobs ? numJobs : 1) * sizeof(uint32_t);
    uint32_t jobIndexBufferOffset = copyToUploadBuffer(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_JOB_INDEX_BUFFER, &context->indexOffsets[0], jobIndexBufferSize, alignUp(jobIndexBufferSize));

    setSRVBindingInfo(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_JOB_BUFFER, jobBufferOffset, jobBufferSize, sizeof(FfxBrixelizerBrixelizationJob));
    setSRVBindingInfo(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_JOB_INDEX_BUFFER, jobIndexBufferOffset, jobIndexBufferSize, sizeof(uint32_t));

    FfxBrixelizerScratchPartition scratchPartition = {};
    getScratchMemorySize(context, desc, &scratchPartition);

    for (uint32_t i = 0; i < FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES; i++)
    {
        uint32_t offset = scratchPartition.array[FFX_BRIXELIZER_NUM_SCRATCH_SPACE_RANGES + i];
        uint32_t size   = scratchPartition.array[i];
        uint32_t stride = sizeof(uint32_t);
        switch (FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_COUNTERS + i)
        {
        case FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_CR1_REFERENCES:
            stride = sizeof(FfxBrixelizerTriangleReference);
            break;
        case FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_DEBUG_AABBS:
            stride = sizeof(FfxBrixelizerDebugAABB);
            break;
        }

        setUAVBindingInfo(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_COUNTERS + i, offset, size, stride);
    }

    FFX_ASSERT(cascade->info.is_initialized);

    FfxBrixelizerBuildInfo buildInfo   = {};
    buildInfo.max_bricks_per_bake = desc->maxBricksPerBake;
    buildInfo.do_initialization   = cascade->info.is_initialized ? 0 : 1;
    buildInfo.build_flags         = (uint32_t)desc->flags;
    buildInfo.num_jobs            = numJobs;
    buildInfo.num_job_voxels      = voxelCount;
    buildInfo.cascade_index       = cascade->info.index;
    buildInfo.is_dynamic          = false;

    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CASCADE_INFO, &cascadeInfo);
    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, &buildInfo);

    cascade->info.is_initialized = 1;

    scheduleDispatch(context, &context->pipelineCascadeClearBuildCounters, 1, 1, 1, cascadeInfo.index);

    if (buildInfo.do_initialization || (desc->flags & FFX_BRIXELIZER_CASCADE_UPDATE_FLAG_RESET))
        scheduleDispatch(context, &context->pipelineCascadeResetCascade, (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION + 63) / 64, 1, 1, cascadeInfo.index);

    scheduleDispatch(context, &context->pipelineCascadeScrollCascade, (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION + 63) / 64, 1, 1, cascadeInfo.index);
    scheduleDispatch(context, &context->pipelineCascadeClearRefCounters, (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION + 63) / 64, 1, 1, cascadeInfo.index);

    if (voxelCount > 0)
    {
        scheduleDispatch(context, &context->pipelineCascadeClearJobCounter, (numJobs + 63) / 64, 1, 1, cascadeInfo.index);
        scheduleDispatch(context, &context->pipelineCascadeInvalidateJobAreas, (voxelCount + FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE, 1, 1, cascadeInfo.index);
        scheduleDispatch(context, &context->pipelineCascadeCoarseCulling, (voxelCount + FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_VOXELIZER_GROUP_SIZE, 1, 1, cascadeInfo.index);
        scheduleDispatch(context, &context->pipelineCascadeScanJobs, (numJobs + FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE, 1, 1, cascadeInfo.index);
        scheduleIndirectDispatch(context, &context->pipelineCascadeVoxelize, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1], FFX_BRIXELIZER_INDIRECT_OFFSETS_VOXELIZE, cascadeInfo.index);
        scheduleDispatch(context, &context->pipelineCascadeScanReferences, ((FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION) + FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE - 1) / FFX_BRIXELIZER_STATIC_CONFIG_SCAN_REFERENCES_GROUP_SIZE, 1, 1, cascadeInfo.index);
        scheduleIndirectDispatch(context, &context->pipelineCascadeCompactReferences, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1], FFX_BRIXELIZER_INDIRECT_OFFSETS_COMPACT_REFERENCES, cascadeInfo.index);
        scheduleIndirectDispatch(context, &context->pipelineCascadeClearBrickStorage, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1], FFX_BRIXELIZER_INDIRECT_OFFSETS_CLEAR_BRICKS, cascadeInfo.index);
        scheduleIndirectDispatch(context, &context->pipelineCascadeEmitSDF, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1], FFX_BRIXELIZER_INDIRECT_OFFSETS_EMIT_SDF, cascadeInfo.index);
        scheduleIndirectDispatch(context, &context->pipelineCascadeCompressBrick, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1], FFX_BRIXELIZER_INDIRECT_OFFSETS_COMPRESS, cascadeInfo.index);
    }
    else
    {
        scheduleDispatch(context, &context->pipelineCascadeInitializeCascade, (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION + 63) / 64, 1, 1, cascadeInfo.index);
    }

    uint32_t cascadeCounterPos = context->cascadeCounterPositions[cascade->info.index];

    if (context->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CASCADE_READBACK_BUFFERS)
    {
        uint32_t readbackBufferID = getCascadeReadbackBufferID(cascade->info.index, cascadeCounterPos);
        void *mappedBuffer = context->cascadeReadbackBufferMappedPointers[readbackBufferID];
        FFX_ASSERT(mappedBuffer);
        memcpy(&context->cascadeCounters[cascade->info.index], mappedBuffer, sizeof(context->cascadeCounters[cascade->info.index]));

        uint32_t readbackBufferResourceID = getCascadeReadbackBufferResourceID(cascade->info.index, cascadeCounterPos);

        scheduleCopy(context,
                     context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BUFFER],
                     0,
                     context->resources[readbackBufferResourceID],
                     0,
                     sizeof(FfxBrixelizerScratchCounters),
                     L"Copy Scratch Counters");

        context->cascadeCounterPositions[cascade->info.index] = (cascadeCounterPos + 1) % 3;
    }

    return FFX_OK;
}

static FfxErrorCode brixelizerDispatchMergeCascades(FfxBrixelizerRawContext_Private* context, uint32_t srcCascadeAIdx, uint32_t srcCascadeBIdx, uint32_t dstCascadeIdx)
{
    FfxBrixelizerCascade_Private& srcCascadeA = context->cascades[srcCascadeAIdx];
    FfxBrixelizerCascade_Private& srcCascadeB = context->cascades[srcCascadeBIdx];
    FfxBrixelizerCascade_Private& dstCascade   = context->cascades[dstCascadeIdx];

    FfxBrixelizerCascadeInfo tmp               = dstCascade.info;
    dstCascade.info                    = srcCascadeA.info;  // copy pretty much everything except for the index
    dstCascade.info.index              = tmp.index;

    FfxBrixelizerContextInfo contextinfo = getContextInfo(context);

    FfxBrixelizerBuildInfo buildInfo = {};
    buildInfo.dst_cascade     = dstCascadeIdx;
    buildInfo.src_cascade_A   = srcCascadeAIdx;
    buildInfo.src_cascade_B   = srcCascadeBIdx;

    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, &buildInfo);

    scheduleDispatch(context, &context->pipelineContextMergeCascades, (FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION + 63) / 64, 1, 1, 0);
    scheduleDispatch(context, &context->pipelineContextPrepareMergeBricksArgs, 1, 1, 1, 0);
    scheduleIndirectDispatch(context, &context->pipelineContextMergeBricks, context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INDIRECT_ARGS_1], FFX_BRIXELIZER_INDIRECT_OFFSETS_MERGE_BRICKS, 0);

    return FFX_OK;
}

static FfxErrorCode brixelizerDispatchBuildAABBTree(FfxBrixelizerRawContext_Private* context, uint32_t cascadeIdx)
{
    FfxBrixelizerCascade_Private *cascade = &context->cascades[cascadeIdx];
    FfxBrixelizerCascadeInfo cascadeInfo = cascade->info;

    FfxBrixelizerBuildInfo buildInfo = {};
    buildInfo.cascade_index   = cascadeInfo.index;

    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CASCADE_INFO, &cascadeInfo);
    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, &buildInfo);

    {
        // special case for 64: 3 level AABB BVH
        static_assert(FFX_BRIXELIZER_CASCADE_RESOLUTION == 64, "");
        buildInfo.tree_iteration = 0;  //  bottom level 16^16^16 of 4^4^4

        updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, &buildInfo);

        scheduleDispatch(context, &context->pipelineCascadeBuildTreeAABB, 16, 16, 16, cascadeIdx);

        buildInfo.tree_iteration = 1;  //  mid level 4^4^4 of 4^4^4

        updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, &buildInfo);

        scheduleDispatch(context, &context->pipelineCascadeBuildTreeAABB, 4, 4, 4, cascadeIdx);

        buildInfo.tree_iteration = 2;  //  top level 1 of 4^4^4

        updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_BUILD_INFO, &buildInfo);

        scheduleDispatch(context, &context->pipelineCascadeBuildTreeAABB, 1, 1, 1, cascadeIdx);
    }

    return FFX_OK;
}

static FfxErrorCode brixelizerDispatchDebugVisualization(FfxBrixelizerRawContext_Private* context, const FfxBrixelizerDebugVisualizationDescription* debugVisualizationDescription)
{
    context->contextDescription.backendInterface.fpRegisterResource(&context->contextDescription.backendInterface,
                                                                    &debugVisualizationDescription->output,
                                                                    context->effectContextId,
                                                                    &context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_DEBUG_OUTPUT]);
    
    FfxBrixelizerContextInfo contextInfo = getContextInfo(context);

    FfxBrixelizerDebugInfo debugInfo = {};

    if (context->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_AABBS) {
        debugInfo.max_aabbs = context->contextDescription.maxDebugAABBs;

        if (debugVisualizationDescription->numDebugAABBInstanceIDs > 0) {
            // XXX -- use debug_state to pass in number of instance IDs
            debugInfo.debug_state = debugVisualizationDescription->numDebugAABBInstanceIDs;
            updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_DEBUG_INFO, &debugInfo);

            size_t instanceIDBufferSize = debugVisualizationDescription->numDebugAABBInstanceIDs * sizeof(FfxBrixelizerInstanceID);
            uint32_t offset = copyToUploadBuffer(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_DEBUG_INSTANCE_ID_BUFFER, (void*)debugVisualizationDescription->debugAABBInstanceIDs, instanceIDBufferSize);

            setSRVBindingInfo(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_DEBUG_INSTANCE_ID_BUFFER, offset, instanceIDBufferSize, sizeof(FfxBrixelizerInstanceID));

            uint32_t dispatchWidth = (debugVisualizationDescription->numDebugAABBInstanceIDs + 63) / 64;
            scheduleDispatch(context, &context->pipelineDebugInstanceAABBs, dispatchWidth, 1, 1, 0);
        }

        for (uint32_t i = 0; i < FFX_ARRAY_ELEMENTS(debugVisualizationDescription->cascadeDebugAABB); ++i) {
            FfxBrixelizerCascadeDebugAABB cascadeDebugAABB = debugVisualizationDescription->cascadeDebugAABB[i];

            uint32_t dispatchWidth;
            switch (cascadeDebugAABB) {
            case FFX_BRIXELIZER_CASCADE_DEBUG_AABB_NONE: continue;
            case FFX_BRIXELIZER_CASCADE_DEBUG_AABB_BOUNDING_BOX:
                // XXX -- use debugInfo.debug_state to indicate only showing the bounding box
                debugInfo.debug_state = 1;
                dispatchWidth = 1;
                break;
            case FFX_BRIXELIZER_CASCADE_DEBUG_AABB_AABB_TREE:
                // XXX -- use debugInfo.debug_state to indicate showing the AABB tree
                debugInfo.debug_state = 0;
                dispatchWidth = (1 + 1 + 4*4*4 + 16*16*16 + 63) / 64;
                break;
            }

            updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_DEBUG_INFO, &debugInfo);
            updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CASCADE_INFO, &context->cascades[i].info);
            scheduleDispatch(context, &context->pipelineDebugDrawAABBTree, dispatchWidth, 1, 1, i);
        }
    }

    memcpy(debugInfo.inv_view, debugVisualizationDescription->inverseViewMatrix, sizeof(float) * 16);
    memcpy(debugInfo.inv_proj, debugVisualizationDescription->inverseProjectionMatrix, sizeof(float) * 16);

    debugInfo.t_min                 = debugVisualizationDescription->tMin;
    debugInfo.t_max                 = debugVisualizationDescription->tMax;
    debugInfo.preview_sdf_solve_eps = debugVisualizationDescription->sdfSolveEps;
    debugInfo.start_cascade_idx     = debugVisualizationDescription->startCascadeIndex;
    debugInfo.end_cascade_idx       = debugVisualizationDescription->endCascadeIndex;
    debugInfo.debug_state           = debugVisualizationDescription->debugState;

    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_CONTEXT_INFO, &contextInfo);
    updateConstantBuffer(context, FFX_BRIXELIZER_CONSTANTBUFFER_IDENTIFIER_DEBUG_INFO, &debugInfo);

    scheduleDispatch(context, &context->pipelineDebugVisualization, (debugVisualizationDescription->renderWidth + 7) / 8, (debugVisualizationDescription->renderHeight + 3) / 4, 1, 0);

    return FFX_OK;
}

static void brixelizerFlushInstances(FfxBrixelizerRawContext_Private* context, FfxCommandList cmdList)
{
    for (uint32_t i = 0; i < context->hostNewInstanceListSize; i++)
    {
        FfxBrixelizerInstanceID idx = context->hostNewInstanceList[i];

        FfxBrixelizerInstanceInfo instanceInfo = getFlatInstancePtr(context)[idx];
        FfxFloat32x3x4 transform;
        memcpy(&transform, getFlatTransformPtr(context) + idx, sizeof(transform));

        // Copy into mapped pointer of staging buffer
        uint32_t instanceInfoOffset =
            copyToUploadBuffer(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_INSTANCE_INFO_BUFFER, &instanceInfo, sizeof(instanceInfo));
        uint32_t instanceTransformOffset =
            copyToUploadBuffer(context, FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_INSTANCE_TRANSFORM_BUFFER, &transform, sizeof(transform));

        scheduleCopy(context,
                     context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_INSTANCE_INFO_BUFFER],
                     instanceInfoOffset,
                     context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INSTANCE_INFO_BUFFER],
                     idx * sizeof(FfxBrixelizerInstanceInfo),
                     sizeof(FfxBrixelizerInstanceInfo),
                     L"Instance Info");

        scheduleCopy(context,
                     context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_UPLOAD_INSTANCE_TRANSFORM_BUFFER],
                     instanceTransformOffset,
                     context->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_INSTANCE_TRANSFORM_BUFFER],
                     idx * sizeof(FfxFloat32x3x4),
                     sizeof(FfxFloat32x3x4),
                     L"Instance Transform");

        context->contextDescription.backendInterface.fpExecuteGpuJobs(&context->contextDescription.backendInterface, cmdList, context->effectContextId);
    }

    clearHostNewInstanceList(context);
}

FfxErrorCode ffxBrixelizerRawContextCreate(FfxBrixelizerRawContext* context, const FfxBrixelizerRawContextDescription* contextDescription)
{
    // zero context memory
    memset(context, 0, sizeof(FfxBrixelizerRawContext));

    // check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextDescription, FFX_ERROR_INVALID_POINTER);

    // validate that all callbacks are set for the interface
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetSDKVersion, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpGetDeviceCapabilities, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpCreateBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);
    FFX_RETURN_ON_ERROR(contextDescription->backendInterface.fpDestroyBackendContext, FFX_ERROR_INCOMPLETE_INTERFACE);

    // if a scratch buffer is declared, then we must have a size
    if (contextDescription->backendInterface.scratchBuffer)
    {
        FFX_RETURN_ON_ERROR(contextDescription->backendInterface.scratchBufferSize, FFX_ERROR_INCOMPLETE_INTERFACE);
    }

    // ensure the context is large enough for the internal context.
    FFX_STATIC_ASSERT(sizeof(FfxBrixelizerRawContext) >= sizeof(FfxBrixelizerRawContext_Private));

    // create the context.
    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);
    const FfxErrorCode    errorCode      = brixelizerCreate(contextPrivate, contextDescription);

    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextDestroy(FfxBrixelizerRawContext* context)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    // destroy the context.
    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);
    const FfxErrorCode    errorCode      = brixelizerRelease(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextGetInfo(FfxBrixelizerRawContext* context, FfxBrixelizerContextInfo* contextInfo)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(contextInfo, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    *contextInfo = getContextInfo(contextPrivate);

    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextCreateCascade(FfxBrixelizerRawContext* context, const FfxBrixelizerRawCascadeDescription* cascadeDescription)
{
    // check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(cascadeDescription, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    FFX_ASSERT(cascadeDescription->brickSize > 0.0f);
    FFX_ASSERT(contextPrivate->cascades[cascadeDescription->index].isAllocated == false);

    FfxBrixelizerCascade_Private* cascadePrivate = &contextPrivate->cascades[cascadeDescription->index];

    cascadePrivate->isAllocated = true;

    for (uint32_t i = 0; i < 3; i++)
        cascadePrivate->info.grid_min[i] = cascadeDescription->cascadeMin[i];

    for (uint32_t i = 0; i < 3; i++)
        cascadePrivate->info.grid_max[i] = cascadeDescription->cascadeMin[i] + cascadeDescription->brickSize * FFX_BRIXELIZER_CASCADE_RESOLUTION;
    
    for (uint32_t i = 0; i < 3; i++)
        cascadePrivate->info.grid_mid[i] = (cascadePrivate->info.grid_max[i] + cascadePrivate->info.grid_min[i]) * 0.5f;

    cascadePrivate->info.voxel_size         = cascadeDescription->brickSize;
    cascadePrivate->info.ivoxel_size        = 1.0f / cascadeDescription->brickSize;
    cascadePrivate->info.index              = cascadeDescription->index;
    cascadePrivate->info.is_enabled         = 1;
    cascadePrivate->info.is_initialized     = 0;

    if (contextPrivate->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CASCADE_READBACK_BUFFERS)
    {
        for (uint32_t i = 0; i < 3; ++i) {
            wchar_t readbackBufferName[64] = {};
            swprintf(readbackBufferName, FFX_ARRAY_ELEMENTS(readbackBufferName), L"Brixelizer_CascadeReadbackBuffer%u_%u", i, cascadePrivate->info.index);

            uint32_t readbackBufferID = getCascadeReadbackBufferID(cascadePrivate->info.index, i);
            uint32_t readbackBufferResourceID = getCascadeReadbackBufferResourceID(cascadePrivate->info.index, i);
            memset(&contextPrivate->resources[readbackBufferResourceID], 0, sizeof(contextPrivate->resources[readbackBufferResourceID]));

            FfxCreateResourceDescription desc = {};
            desc.heapType = FFX_HEAP_TYPE_READBACK;
            desc.resourceDescription.type = FFX_RESOURCE_TYPE_BUFFER;
            desc.resourceDescription.format = FFX_SURFACE_FORMAT_R32_UINT;
            desc.resourceDescription.size = sizeof(FfxBrixelizerScratchCounters);
            desc.resourceDescription.stride = sizeof(FfxBrixelizerScratchCounters);
            desc.resourceDescription.mipCount = 1;
            desc.resourceDescription.flags = FFX_RESOURCE_FLAGS_NONE;
            desc.resourceDescription.usage = FFX_RESOURCE_USAGE_READ_ONLY;
            desc.initialState = FFX_RESOURCE_STATE_COPY_DEST;
            desc.initData.type = FFX_RESOURCE_INIT_DATA_TYPE_UNINITIALIZED;
            desc.name = readbackBufferName;
            desc.id = readbackBufferResourceID;

            FFX_VALIDATE(contextPrivate->contextDescription.backendInterface.fpCreateResource(&contextPrivate->contextDescription.backendInterface, &desc, contextPrivate->effectContextId, &contextPrivate->resources[readbackBufferResourceID]));

            FFX_VALIDATE(contextPrivate->contextDescription.backendInterface.fpMapResource(&contextPrivate->contextDescription.backendInterface, contextPrivate->resources[readbackBufferResourceID], &contextPrivate->cascadeReadbackBufferMappedPointers[readbackBufferID]));
        }
    }

    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextDestroyCascade(FfxBrixelizerRawContext* context, uint32_t cascadeIndex)
{
    // check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    if (contextPrivate->contextDescription.flags & FFX_BRIXELIZER_CONTEXT_FLAG_DEBUG_CASCADE_READBACK_BUFFERS)
    {
        for (uint32_t i = 0; i < 3; ++i) {
            uint32_t readbackBufferID = getCascadeReadbackBufferID(cascadeIndex, i);
            uint32_t readbackBufferResourceID = getCascadeReadbackBufferResourceID(cascadeIndex, i);
            ffxSafeReleaseResource(&contextPrivate->contextDescription.backendInterface,
                                contextPrivate->resources[readbackBufferResourceID],
                                contextPrivate->effectContextId);
            contextPrivate->cascadeReadbackBufferMappedPointers[readbackBufferID] = NULL;
        }
    }

    contextPrivate->cascades[cascadeIndex].isAllocated = false;

    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextResetCascade(FfxBrixelizerRawContext* context, uint32_t cascadeIndex)
{
    // check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);
    // dispatch the Brixelizer passes.
    const FfxErrorCode errorCode = brixelizerDispatchResetCascade(contextPrivate, cascadeIndex);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextBegin(FfxBrixelizerRawContext* context, FfxBrixelizerResources resources)
{
    // check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Brixelizer passes.
    const FfxErrorCode errorCode = brixelizerDispatchBegin(contextPrivate, resources);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextEnd(FfxBrixelizerRawContext* context)
{
    // check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Brixelizer passes.
    const FfxErrorCode errorCode = brixelizerDispatchEnd(contextPrivate);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextSubmit(FfxBrixelizerRawContext* context, FfxCommandList cmdList)
{
    // check pointers are valid.
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Brixelizer passes.
    const FfxErrorCode errorCode = brixelizerSubmit(contextPrivate, cmdList);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextGetScratchMemorySize(FfxBrixelizerRawContext* context, const FfxBrixelizerRawCascadeUpdateDescription* cascadeUpdateDescription, size_t* size)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(cascadeUpdateDescription, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    if (size)
        *size = getScratchMemorySize(contextPrivate, cascadeUpdateDescription, NULL);

    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextUpdateCascade(FfxBrixelizerRawContext* context, const FfxBrixelizerRawCascadeUpdateDescription* cascadeUpdateDescription)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(cascadeUpdateDescription, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Brixelizer passes.
    const FfxErrorCode errorCode = brixelizerDispatchUpdateCascade(contextPrivate, cascadeUpdateDescription);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextMergeCascades(FfxBrixelizerRawContext* context, uint32_t srcCascadeAIdx, uint32_t srcCascadeBIdx, uint32_t dstCascadeIdx)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Brixelizer passes.
    const FfxErrorCode errorCode = brixelizerDispatchMergeCascades(contextPrivate, srcCascadeAIdx, srcCascadeBIdx, dstCascadeIdx);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextBuildAABBTree(FfxBrixelizerRawContext* context, uint32_t cascadeIndex)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private *contextPrivate = (FfxBrixelizerRawContext_Private*)context;

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    const FfxErrorCode errorCode = brixelizerDispatchBuildAABBTree(contextPrivate, cascadeIndex);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextDebugVisualization(FfxBrixelizerRawContext* context, const FfxBrixelizerDebugVisualizationDescription* debugVisualizationDescription)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(debugVisualizationDescription, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    FFX_RETURN_ON_ERROR(contextPrivate->device, FFX_ERROR_NULL_DEVICE);

    // dispatch the Brixelizer passes.
    const FfxErrorCode errorCode = brixelizerDispatchDebugVisualization(contextPrivate, debugVisualizationDescription);
    return errorCode;
}

FfxErrorCode ffxBrixelizerRawContextGetDebugCounters(FfxBrixelizerRawContext* context, FfxBrixelizerDebugCounters* debugCounters)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(debugCounters, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    *debugCounters = contextPrivate->debugCounters;
    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextGetCascadeCounters(FfxBrixelizerRawContext* context, uint32_t cascadeIndex, FfxBrixelizerScratchCounters* counters)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);
    FFX_RETURN_ON_ERROR(counters, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private *contextPrivate = (FfxBrixelizerRawContext_Private*)context;

    *counters = contextPrivate->cascadeCounters[cascadeIndex];
    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextCreateInstances(FfxBrixelizerRawContext* uncastContext, const FfxBrixelizerRawInstanceDescription* instanceDescriptions, uint32_t numInstanceDescriptions)
{
    FFX_RETURN_ON_ERROR(uncastContext, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* context = (FfxBrixelizerRawContext_Private*)(uncastContext);

    FFX_ASSERT(context->hostFreelistSize >= numInstanceDescriptions);

    context->hostFreelistSize -= numInstanceDescriptions;
    FfxBrixelizerInstanceID *instanceIDs = &context->hostFreelist[context->hostFreelistSize];
    memcpy(&context->hostNewInstanceList[context->hostNewInstanceListSize], instanceIDs, sizeof(*instanceIDs) * numInstanceDescriptions);
    context->hostNewInstanceListSize += numInstanceDescriptions;
    context->numInstances += numInstanceDescriptions;

    for (uint32_t i = 0; i < numInstanceDescriptions; ++i) {
        const FfxBrixelizerRawInstanceDescription *desc = &instanceDescriptions[i];
        FfxBrixelizerInstanceID instanceID = instanceIDs[i];
        FfxBrixelizerInstanceInfo *instanceInfo = &context->hostInstances[instanceID];
        FfxFloat32x3x4 *transform = &context->hostTransforms[instanceID];

        for (uint32_t i = 0; i < 3; i++)
        {
            instanceInfo->aabbMin[i]          = desc->aabbMin[i];
            instanceInfo->aabbMax[i]          = desc->aabbMax[i];
        }
        
        FFX_ASSERT(desc->aabbMax[0] >= desc->aabbMin[0] && desc->aabbMax[1] >= desc->aabbMin[1] && desc->aabbMax[2] >= desc->aabbMin[2]);
        
        instanceInfo->indexBufferOffset = (uint32_t)desc->indexBufferOffset;
        instanceInfo->triangleCount     = desc->triangleCount;
        
        uint32_t instanceFlags         = 0;
        instanceFlags |= (desc->indexFormat == FFX_INDEX_TYPE_UINT32) ? 0 : FFX_BRIXELIZER_INSTANCE_FLAG_USE_U16_INDEX;
        instanceFlags |= (desc->vertexFormat == FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT) ? FFX_BRIXELIZER_INSTANCE_FLAG_USE_RGBA16_VERTEX : 0;
        instanceFlags |= (desc->flags & FFX_BRIXELIZER_RAW_INSTANCE_FLAG_USE_INDEXLESS_QUAD_LIST) ? FFX_BRIXELIZER_INSTANCE_FLAG_USE_INDEXLESS_QUAD_LIST : 0;

        FFX_ASSERT(desc->vertexFormat == FFX_SURFACE_FORMAT_R16G16B16A16_FLOAT || desc->vertexFormat == FFX_SURFACE_FORMAT_R32G32B32_FLOAT);
        FFX_ASSERT(desc->indexFormat == FFX_INDEX_TYPE_UINT32 || desc->indexFormat == FFX_INDEX_TYPE_UINT16);

        instanceInfo->vertexBufferOffset = desc->vertexBufferOffset;
        instanceInfo->vertexCount        = desc->vertexCount;

        FFX_ASSERT(desc->indexBuffer < (1 << 16));
        FFX_ASSERT(desc->vertexBuffer < (1 << 16));
        instanceInfo->pack0 = ((desc->indexBuffer & 0xffffu) << 0) | ((desc->vertexBuffer & 0xffffu) << 16);
        instanceInfo->pack1 = ((desc->vertexStride & 0x3fu) << 26u) | ((instanceFlags & 0x3ffu) << 16u);

        instanceInfo->index = instanceID;

        memcpy(transform, &desc->transform, sizeof(*transform));

        *desc->outInstanceID = instanceID;
    }

    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextDestroyInstances(FfxBrixelizerRawContext* uncastContext, const FfxBrixelizerInstanceID* instanceIDs, uint32_t numInstanceIDs)
{
    FFX_RETURN_ON_ERROR(uncastContext, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* context = (FfxBrixelizerRawContext_Private*)(uncastContext);

    FFX_ASSERT(context->numInstances >= numInstanceIDs);
    for (uint32_t i = 0; i < numInstanceIDs; ++i) {
        FFX_ASSERT(instanceIDs[i] != FFX_BRIXELIZER_INVALID_ID);
    }

    memcpy(&context->hostFreelist[context->hostFreelistSize], instanceIDs, sizeof(*instanceIDs) * numInstanceIDs);
    context->hostFreelistSize += numInstanceIDs;
    context->numInstances -= numInstanceIDs;

    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextFlushInstances(FfxBrixelizerRawContext* context, FfxCommandList cmdList)
{
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    brixelizerFlushInstances(contextPrivate, cmdList);

    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextRegisterBuffers(FfxBrixelizerRawContext* uncastContext, const FfxBrixelizerBufferDescription* bufferDescs, uint32_t numBufferDescs)
{
    FFX_RETURN_ON_ERROR(uncastContext, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* context = (FfxBrixelizerRawContext_Private*)(uncastContext);

    FFX_ASSERT(context->bufferIndexFreeListSize >= numBufferDescs);

    context->bufferIndexFreeListSize -= numBufferDescs;
    uint32_t *bufferIndices = &context->bufferIndexFreeList[context->bufferIndexFreeListSize];

    for (uint32_t i = 0; i < numBufferDescs; ++i) {
        const FfxBrixelizerBufferDescription *bufferDesc = &bufferDescs[i];
        uint32_t bufferIndex = bufferIndices[i];

        FfxStaticResourceDescription staticResourceDesc = {};

        staticResourceDesc.resource        = &bufferDesc->buffer;
        staticResourceDesc.descriptorType  = FFX_DESCRIPTOR_BUFFER_SRV;
        staticResourceDesc.descriptorIndex = bufferIndex;
        staticResourceDesc.bufferOffset    = 0;
        staticResourceDesc.bufferSize      = 0;
        staticResourceDesc.bufferStride    = sizeof(uint32_t);

        FfxErrorCode errorCode = context->contextDescription.backendInterface.fpRegisterStaticResource(&context->contextDescription.backendInterface, 
                                                                                                       &staticResourceDesc, 
                                                                                                       context->effectContextId);

        FFX_ASSERT(errorCode == FFX_OK);
        FFX_ASSERT(bufferDesc->outIndex);

        *bufferDesc->outIndex = bufferIndex;
    }

    return FFX_OK;
}

FfxErrorCode ffxBrixelizerRawContextUnregisterBuffers(FfxBrixelizerRawContext* uncastContext, const uint32_t* indices, uint32_t numIndices)
{
    FFX_RETURN_ON_ERROR(uncastContext, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* context = (FfxBrixelizerRawContext_Private*)(uncastContext);

    FFX_ASSERT(context->bufferIndexFreeListSize + numIndices <= FFX_BRIXELIZER_MAX_INSTANCES);

    memcpy(&context->bufferIndexFreeList[context->bufferIndexFreeListSize], indices, sizeof(*indices) * numIndices);
    context->bufferIndexFreeListSize += numIndices;

    return FFX_OK;
}

 FfxErrorCode ffxBrixelizerRawContextRegisterScratchBuffer(FfxBrixelizerRawContext* context, FfxResource scratchBuffer)
 {
    FFX_RETURN_ON_ERROR(context, FFX_ERROR_INVALID_POINTER);

    FfxBrixelizerRawContext_Private* contextPrivate = (FfxBrixelizerRawContext_Private*)(context);

    if (!ffxBrixelizerRawResourceIsNull(scratchBuffer))
    {
        return contextPrivate->contextDescription.backendInterface.fpRegisterResource(&contextPrivate->contextDescription.backendInterface,
                                                                                      &scratchBuffer,
                                                                                      contextPrivate->effectContextId,
                                                                                      &contextPrivate->resources[FFX_BRIXELIZER_RESOURCE_IDENTIFIER_SCRATCH_BUFFER]);
    }
    else
        return FFX_ERROR_INVALID_ARGUMENT;
}

uint32_t ffxBrixelizerRawGetCascadeToUpdate(uint32_t frameIndex, uint32_t maxCascades)
{
    // variable rate update(first cascade is updated every other frame)
    uint32_t n = frameIndex & ((1 << maxCascades) - 1);
    n      = n - (n & n - 1);
    if (n == 0)
        n = ((1 << (maxCascades - 1)));
    n = (uint32_t)log2(double(n));
    FFX_ASSERT(n < maxCascades);
    return n;
}

bool ffxBrixelizerRawResourceIsNull(FfxResource resource)
 {
     return resource.resource == NULL;
 }

FFX_API FfxVersionNumber ffxBrixelizerGetEffectVersion()
{
    return FFX_SDK_MAKE_VERSION(FFX_BRIXELIZER_VERSION_MAJOR, FFX_BRIXELIZER_VERSION_MINOR, FFX_BRIXELIZER_VERSION_PATCH);
}
