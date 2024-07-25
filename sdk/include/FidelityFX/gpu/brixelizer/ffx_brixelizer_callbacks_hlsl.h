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

#ifndef  FFX_BRIXELIZER_CALLBACKS_HLSL_H
#define  FFX_BRIXELIZER_CALLBACKS_HLSL_H

#include "ffx_brixelizer_resources.h"
#include "ffx_brixelizer_host_gpu_shared.h"
#include "ffx_brixelizer_host_gpu_shared_private.h"

#if defined(FFX_GPU)
#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#define FFX_BRIXELIZER_SIZEOF_UINT 4

#define DECLARE_SRV_REGISTER(regIndex)      t##regIndex
#define DECLARE_UAV_REGISTER(regIndex)      u##regIndex
#define DECLARE_CB_REGISTER(regIndex)       b##regIndex
#define FFX_BRIXELIZER_DECLARE_SRV(regIndex)        register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_BRIXELIZER_DECLARE_UAV(regIndex)        register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_BRIXELIZER_DECLARE_CB(regIndex)         register(DECLARE_CB_REGISTER(regIndex))
#define FFX_BRIXELIZER_DECLARE_STATIC_SRV(regIndex) register(DECLARE_SRV_REGISTER(regIndex), space1)

#define FFX_BRIXELIZER_ROOTSIG_STRINGIFY(p) FFX_BRIXELIZER_ROOTSIG_STR(p)
#define FFX_BRIXELIZER_ROOTSIG_STR(p) #p
#define FFX_BRIXELIZER_ROOTSIG [RootSignature("DescriptorTable(UAV(u0, numDescriptors = " FFX_BRIXELIZER_ROOTSIG_STRINGIFY(FFX_BRIXELIZER_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                              "DescriptorTable(SRV(t0, numDescriptors = " FFX_BRIXELIZER_ROOTSIG_STRINGIFY(FFX_BRIXELIZER_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                              "DescriptorTable(Sampler(s0)), " \
                                              "CBV(b0), " \
                                              "CBV(b1), " \
                                              "CBV(b2), " \
                                              "CBV(b3)")]
#define FFX_BRIXELIZER_VOXELIZE_ROOTSIG [RootSignature("DescriptorTable(UAV(u0, numDescriptors = " FFX_BRIXELIZER_ROOTSIG_STRINGIFY(FFX_BRIXELIZER_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                              "DescriptorTable(SRV(t0, numDescriptors = " FFX_BRIXELIZER_ROOTSIG_STRINGIFY(FFX_BRIXELIZER_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                              "DescriptorTable(SRV(t0, space = 1, numDescriptors = unbounded)), " \
                                              "DescriptorTable(Sampler(s0)), " \
                                              "CBV(b0), " \
                                              "CBV(b1), " \
                                              "CBV(b2), " \
                                              "CBV(b3)")]

#if defined(FFX_BRIXELIZER_EMBED_ROOTSIG)
#define FFX_BRIXELIZER_EMBED_ROOTSIG_CONTENT FFX_BRIXELIZER_ROOTSIG
#define FFX_BRIXELIZER_EMBED_VOXELIZE_ROOTSIG_CONTENT FFX_BRIXELIZER_VOXELIZE_ROOTSIG
#else
#define FFX_BRIXELIZER_EMBED_ROOTSIG_CONTENT
#define FFX_BRIXELIZER_EMBED_VOXELIZE_ROOTSIG_CONTENT
#endif // #if FFX_BRIXELIZER_EMBED_ROOTSIG

// CBVs

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
    ConstantBuffer<FfxBrixelizerCascadeInfo> cbBrixelizerCascadeInfo : FFX_BRIXELIZER_DECLARE_CB(BRIXELIZER_BIND_CB_CASCADE_INFO);
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
    ConstantBuffer<FfxBrixelizerContextInfo> cbBrixelizerContextInfo : FFX_BRIXELIZER_DECLARE_CB(BRIXELIZER_BIND_CB_CONTEXT_INFO);
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
    ConstantBuffer<FfxBrixelizerBuildInfo>   cbBrixelizerBuildInfo : FFX_BRIXELIZER_DECLARE_CB(BRIXELIZER_BIND_CB_BUILD_INFO);
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
    ConstantBuffer<FfxBrixelizerDebugInfo>   cbBrixelizerDebugInfo : FFX_BRIXELIZER_DECLARE_CB(BRIXELIZER_BIND_CB_DEBUG_INFO);
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_AABBS
    RWStructuredBuffer<FfxBrixelizerDebugAABB> rw_debug_aabbs : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_DEBUG_AABBS);
#endif

#if defined BRIXELIZER_BIND_SRV_DEBUG_INSTANCE_ID_BUFFER
    StructuredBuffer<FfxUInt32> r_debug_instance_id : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_DEBUG_INSTANCE_ID_BUFFER);
#endif

// SRVs

#if defined BRIXELIZER_BIND_SRV_JOB_BUFFER
    StructuredBuffer<FfxBrixelizerBrixelizationJob> r_job_buffer : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_JOB_BUFFER);
#endif

#if defined BRIXELIZER_BIND_SRV_JOB_INDEX_BUFFER
    StructuredBuffer<FfxUInt32>        r_job_index_buffer : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_JOB_INDEX_BUFFER);
#endif

#if defined BRIXELIZER_BIND_SRV_INSTANCE_INFO_BUFFER
    StructuredBuffer<FfxBrixelizerInstanceInfo> r_instance_info_buffer : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_INSTANCE_INFO_BUFFER);
#endif

#if defined BRIXELIZER_BIND_SRV_INSTANCE_TRANSFORM_BUFFER
    StructuredBuffer<FfxFloat32x4>     r_instance_transform_buffer : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_INSTANCE_TRANSFORM_BUFFER);
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
    StructuredBuffer<FfxUInt32>        r_vertex_buffers[FFX_BRIXELIZER_STATIC_CONFIG_MAX_VERTEX_BUFFERS] : FFX_BRIXELIZER_DECLARE_STATIC_SRV(BRIXELIZER_BIND_SRV_VERTEX_BUFFERS);
#endif

#if defined BRIXELIZER_BIND_SRV_SDF_ATLAS
    Texture3D<FfxFloat32>              r_sdf_atlas : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_SDF_ATLAS);
#endif

// UAVs

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
    RWStructuredBuffer<FfxUInt32>       rw_cascade_aabbtree : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE);
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP
    RWStructuredBuffer<FfxUInt32>       rw_cascade_brick_map : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS
    RWStructuredBuffer<FfxUInt32>       rw_scratch_counters : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
    RWStructuredBuffer<FfxUInt32>       rw_scratch_index_swap : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER
    RWStructuredBuffer<FfxUInt32>       rw_scratch_voxel_allocation_fail_counter : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
    RWStructuredBuffer<FfxUInt32>       rw_scratch_bricks_storage : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS
    RWStructuredBuffer<FfxUInt32>       rw_scratch_bricks_storage_offsets : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST
    RWStructuredBuffer<FfxUInt32>       rw_scratch_bricks_compression_list : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST
    RWStructuredBuffer<FfxUInt32>       rw_scratch_bricks_clear_list : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS
    RWStructuredBuffer<FfxUInt32>       rw_scratch_job_counters : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS_SCAN
    RWStructuredBuffer<FfxUInt32>       rw_scratch_job_counters_scan : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS_SCAN);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN
    RWStructuredBuffer<FfxUInt32>       rw_scratch_job_global_counters_scan : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES
    RWStructuredBuffer<FfxBrixelizerTriangleReference> rw_scratch_cr1_references : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_COMPACTED_REFERENCES
    RWStructuredBuffer<FfxUInt32>       rw_scratch_cr1_compacted_references : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_CR1_COMPACTED_REFERENCES);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS
    RWStructuredBuffer<FfxUInt32>       rw_scratch_cr1_ref_counters : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN
    RWStructuredBuffer<FfxUInt32>       rw_scratch_cr1_ref_counter_scan : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN
    RWStructuredBuffer<FfxUInt32>       rw_scratch_cr1_ref_global_scan : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN
    RWStructuredBuffer<FfxUInt32>       rw_scratch_cr1_stamp_scan : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN);
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN
    RWStructuredBuffer<FfxUInt32>       rw_scratch_cr1_stamp_global_scan : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN);
#endif

#if defined BRIXELIZER_BIND_UAV_INDIRECT_ARGS
    RWStructuredBuffer<FfxUInt32>       rw_indirect_args_1 : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_INDIRECT_ARGS);
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP
    RWStructuredBuffer<FfxUInt32>       rw_bctx_bricks_voxel_map : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP);
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_AABB
    RWStructuredBuffer<FfxUInt32>       rw_bctx_bricks_aabb : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_AABB);
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST
    RWStructuredBuffer<FfxUInt32>       rw_bctx_bricks_free_list : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST);
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_CLEAR_LIST
    RWStructuredBuffer<FfxUInt32>       rw_bctx_bricks_clear_list : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_CLEAR_LIST);
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_LIST
    RWStructuredBuffer<FfxUInt32>       rw_bctx_bricks_eikonal_list : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_LIST);
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_MERGE_LIST
    RWStructuredBuffer<FfxUInt32>       rw_bctx_bricks_merge_list : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_MERGE_LIST);
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_COUNTERS
    RWStructuredBuffer<FfxUInt32>       rw_bctx_bricks_eikonal_counters : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_COUNTERS);
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS
    RWStructuredBuffer<FfxUInt32>       rw_bctx_counters : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS);
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAPS
    RWStructuredBuffer<FfxUInt32>       rw_cascade_brick_maps[24] : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAPS);
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREES
    RWStructuredBuffer<FfxUInt32>       rw_cascade_aabbtrees[24]  : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_CASCADE_AABB_TREES);
#endif

#if defined BRIXELIZER_BIND_UAV_SDF_ATLAS
    RWTexture3D<FfxFloat32>             rw_sdf_atlas : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_SDF_ATLAS);
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_OUTPUT
    RWTexture2D<FfxFloat32x4>           rw_debug_output : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_BIND_UAV_DEBUG_OUTPUT);
#endif

SamplerState g_wrap_linear_sampler : register(s0);

FfxBrixelizerDebugAABB GetDebugAABB(FfxUInt32 idx);
void StoreDebugAABB(FfxUInt32 idx, FfxBrixelizerDebugAABB aabb);
FfxUInt32 GetDebugInstanceID(FfxUInt32 idx);
FfxBrixelizerCascadeInfo GetCascadeInfo();
FfxUInt32x3 GetCascadeInfoClipmapOffset();
FfxUInt32 GetCascadeInfoIndex();
FfxFloat32x3 GetCascadeInfoGridMin();
FfxFloat32x3 GetCascadeInfoGridMax();
FfxFloat32 GetCascadeInfoVoxelSize();
FfxInt32x3 GetCascadeInfoClipmapInvalidationOffset();
FfxBrixelizerCascadeInfo GetCascadeInfo(FfxUInt32 idx);
FfxUInt32 GetContextInfoNumBricks();
FfxUInt32x3 GetContextInfoCascadeClipmapOffset(FfxUInt32 cascade_index);
FfxBrixelizerBuildInfo GetBuildInfo();
FfxUInt32 GetBuildInfoDoInitialization();
FfxInt32 GetBuildInfoTreeIteration();
FfxUInt32 GetBuildInfoNumJobs();
FfxUInt32 GetDebugInfoMaxAABBs();
FfxFloat32x4x4 GetDebugInfoInvView();
FfxFloat32x4x4 GetDebugInfoInvProj();
FfxFloat32 GetDebugInfoPreviewSDFSolveEpsilon();
FfxUInt32 GetDebugInfoStartCascadeIndex();
FfxUInt32 GetDebugInfoEndCascadeIndex();
FfxFloat32 GetDebugInfoTMin();
FfxFloat32 GetDebugInfoTMax();
FfxUInt32 GetDebugInfoDebugState();
FfxBrixelizerBrixelizationJob LoadBrixelizationJob(FfxUInt32 idx);
FfxUInt32 LoadJobIndex(FfxUInt32 idx);
FfxBrixelizerInstanceInfo LoadInstanceInfo(FfxUInt32 index);
FfxFloat32x3x4 LoadInstanceTransform(FfxUInt32 idx);
FfxUInt32 LoadVertexBufferUInt(FfxUInt32 bufferIdx, FfxUInt32 elementIdx);
FfxUInt32x2 LoadVertexBufferUInt2(FfxUInt32 bufferIdx, FfxUInt32 elementIdx);
FfxUInt32x3 LoadVertexBufferUInt3(FfxUInt32 bufferIdx, FfxUInt32 elementIdx);
FfxFloat32x2 LoadVertexBufferFloat2(FfxUInt32 bufferIdx, FfxUInt32 elementIdx);
FfxFloat32x3 LoadVertexBufferFloat3(FfxUInt32 bufferIdx, FfxUInt32 elementIdx);
FfxFloat32x4 LoadVertexBufferFloat4(FfxUInt32 bufferIdx, FfxUInt32 elementIdx);
FfxFloat32 SampleSDFAtlas(FfxFloat32x3 uvw);
FfxUInt32 LoadCascadeBrickMap(FfxUInt32 elementIdx);
void StoreCascadeBrickMap(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadCascadeBrickMapArrayUniform(FfxUInt32 bufferIdx, FfxUInt32 elementIdx);
void StoreCascadeBrickMapArrayUniform(FfxUInt32 bufferIdx, FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadScratchCounter(FfxUInt32 counter);
void StoreScratchCounter(FfxUInt32 counter, FfxUInt32 value);
void IncrementScratchCounter(FfxUInt32 counter, FfxUInt32 value, inout FfxUInt32 originalValue);
FfxUInt32x2 LoadScratchIndexSwapUInt2(FfxUInt32 elementIdx);
FfxFloat32x3 LoadScratchIndexSwapFloat3(FfxUInt32 elementIdx);
void StoreScratchIndexSwapUInt2(FfxUInt32 elementIdx, FfxUInt32x2 value);
void StoreScratchIndexSwapFloat3(FfxUInt32 elementIdx, FfxFloat32x3 value);
void GetScratchIndexSwapDimensions(inout FfxUInt32 size);
FfxUInt32 LoadScratchVoxelAllocationFailCounter(FfxUInt32 elementIdx);
void StoreScratchVoxelAllocationFailCounter(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadScratchBricksStorage(FfxUInt32 elementIdx);
void StoreScratchBricksStorage(FfxUInt32 elementIdx, FfxUInt32 value);
void GetScratchBricksStorageDimensions(inout FfxUInt32 size);
void MinScratchBricksStorage(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadScratchBricksStorageOffsets(FfxUInt32 elementIdx);
void StoreScratchBricksStorageOffsets(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadScratchBricksCompressionList(FfxUInt32 elementIdx);
void StoreScratchBricksCompressionList(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadScratchBricksClearList(FfxUInt32 elementIdx);
void StoreScratchBricksClearList(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadScratchJobCounter(FfxUInt32 counterIdx);
void StoreScratchJobCounter(FfxUInt32 counterIdx, FfxUInt32 value);
void IncrementScratchJobCounter(FfxUInt32 counterIdx, FfxUInt32 value);
FfxUInt32 LoadScratchJobCountersScan(FfxUInt32 elementIdx);
void StoreScratchJobCountersScan(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadGlobalJobTriangleCounterScan(FfxUInt32 elementIdx);
void StoreGlobalJobTriangleCounterScan(FfxUInt32 elementIdx, FfxUInt32 scan);
FfxBrixelizerTriangleReference LoadScratchCR1Reference(FfxUInt32 elementIdx);
void StoreScratchCR1Reference(FfxUInt32 elementIdx, FfxBrixelizerTriangleReference ref);
void GetScratchMaxReferences(inout FfxUInt32 size);
FfxUInt32 LoadScratchCR1CompactedReferences(FfxUInt32 elementIdx);
void StoreScratchCR1CompactedReferences(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadScratchCR1RefCounter(FfxUInt32 elementIdx);
void StoreScratchCR1RefCounter(FfxUInt32 elementIdx, FfxUInt32 value);
void IncrementScratchCR1RefCounter(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue);
FfxUInt32 LoadScratchCR1RefCounterScan(FfxUInt32 elementIdx);
void StoreScratchCR1RefCounterScan(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadVoxelReferenceGroupSum(FfxUInt32 elementIdx);
void StoreVoxelReferenceGroupSum(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadScratchCR1StampScan(FfxUInt32 elementIdx);
void StoreScratchCR1StampScan(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadStampGroupSum(FfxUInt32 elementIdx);
void StoreStampGroupSum(FfxUInt32 elementIdx, FfxUInt32 value);
void StoreIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadBricksVoxelMap(FfxUInt32 elementIdx);
void StoreBricksVoxelMap(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadBricksAABB(FfxUInt32 elementIdx);
void StoreBricksAABB(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadBricksFreeList(FfxUInt32 elementIdx);
void StoreBricksFreeList(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadBricksClearList(FfxUInt32 elementIdx);
void StoreBricksClearList(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadBricksDirtyList(FfxUInt32 elementIdx);
void StoreBricksDirtyList(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadBricksMergeList(FfxUInt32 elementIdx);
void StoreBricksMergeList(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadBricksEikonalCounters(FfxUInt32 elementIdx);
void StoreBricksEikonalCounters(FfxUInt32 elementIdx, FfxUInt32 value);
FfxUInt32 LoadContextCounter(FfxUInt32 elementIdx);
void StoreContextCounter(FfxUInt32 elementIdx, FfxUInt32 value);
void IncrementContextCounter(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue);
FfxFloat32x3 LoadCascadeAABBTreeFloat3(FfxUInt32 elementIdx);
FfxUInt32 LoadCascadeAABBTreeUInt(FfxUInt32 elementIdx);
void StoreCascadeAABBTreeUInt(FfxUInt32 elementIdx, FfxUInt32 value);
void StoreCascadeAABBTreeFloat3(FfxUInt32 elementIdx, FfxFloat32x3 value);
FfxFloat32x3 LoadCascadeAABBTreesFloat3(FfxUInt32 cascadeID, FfxUInt32 elementIdx);
FfxUInt32 LoadCascadeAABBTreesUInt(FfxUInt32 cascadeID, FfxUInt32 elementIdx);
FfxFloat32 LoadSDFAtlas(FfxUInt32x3 coord);
void StoreSDFAtlas(FfxUInt32x3 coord, FfxFloat32 value);
void StoreDebugOutput(FfxUInt32x2 coord, FfxFloat32x3 output);
void GetDebugOutputDimensions(inout FfxUInt32 width, inout FfxUInt32 height);

#if defined BRIXELIZER_BIND_UAV_DEBUG_AABBS
FfxBrixelizerDebugAABB GetDebugAABB(FfxUInt32 idx)
{
    return rw_debug_aabbs[idx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_AABBS
void StoreDebugAABB(FfxUInt32 idx, FfxBrixelizerDebugAABB aabb)
{
    rw_debug_aabbs[idx] = aabb;
}
#endif

#if defined BRIXELIZER_BIND_SRV_DEBUG_INSTANCE_ID_BUFFER
FfxUInt32 GetDebugInstanceID(FfxUInt32 idx)
{
    return r_debug_instance_id[idx];
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxBrixelizerCascadeInfo GetCascadeInfo()
{
    return cbBrixelizerCascadeInfo;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxUInt32x3 GetCascadeInfoClipmapOffset()
{
    return cbBrixelizerCascadeInfo.clipmap_offset;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxUInt32 GetCascadeInfoIndex()
{
    return cbBrixelizerCascadeInfo.index;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxFloat32x3 GetCascadeInfoGridMin()
{
    return cbBrixelizerCascadeInfo.grid_min;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxFloat32x3 GetCascadeInfoGridMax()
{
    return cbBrixelizerCascadeInfo.grid_max;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxFloat32 GetCascadeInfoVoxelSize()
{
    return cbBrixelizerCascadeInfo.voxel_size;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxInt32x3 GetCascadeInfoClipmapInvalidationOffset()
{
    return cbBrixelizerCascadeInfo.clipmap_invalidation_offset;
}
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
FfxBrixelizerCascadeInfo GetCascadeInfo(FfxUInt32 idx)
{
    return cbBrixelizerContextInfo.cascades[idx];
}
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
FfxUInt32 GetContextInfoNumBricks()
{
    return cbBrixelizerContextInfo.num_bricks;
}
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
FfxUInt32x3 GetContextInfoCascadeClipmapOffset(FfxUInt32 cascade_index)
{
    return cbBrixelizerContextInfo.cascades[cascade_index].clipmap_offset;
}
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
FfxBrixelizerBuildInfo GetBuildInfo()
{
    return cbBrixelizerBuildInfo;
}
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
FfxUInt32 GetBuildInfoDoInitialization()
{
    return cbBrixelizerBuildInfo.do_initialization;
}
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
FfxInt32 GetBuildInfoTreeIteration()
{
    return cbBrixelizerBuildInfo.tree_iteration;
}
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
FfxUInt32 GetBuildInfoNumJobs()
{
    return cbBrixelizerBuildInfo.num_jobs;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxUInt32 GetDebugInfoMaxAABBs()
{
    return cbBrixelizerDebugInfo.max_aabbs;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32x4x4 GetDebugInfoInvView()
{
    return cbBrixelizerDebugInfo.inv_view;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32x4x4 GetDebugInfoInvProj()
{
    return cbBrixelizerDebugInfo.inv_proj;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32 GetDebugInfoPreviewSDFSolveEpsilon()
{
    return cbBrixelizerDebugInfo.preview_sdf_solve_eps;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxUInt32 GetDebugInfoStartCascadeIndex()
{
    return cbBrixelizerDebugInfo.start_cascade_idx;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxUInt32 GetDebugInfoEndCascadeIndex()
{
    return cbBrixelizerDebugInfo.end_cascade_idx;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32 GetDebugInfoTMin()
{
    return cbBrixelizerDebugInfo.t_min;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32 GetDebugInfoTMax()
{
    return cbBrixelizerDebugInfo.t_max;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxUInt32 GetDebugInfoDebugState()
{
    return cbBrixelizerDebugInfo.debug_state;
}
#endif

#if defined BRIXELIZER_BIND_SRV_JOB_BUFFER
FfxBrixelizerBrixelizationJob LoadBrixelizationJob(FfxUInt32 idx)
{
    return r_job_buffer[idx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_JOB_INDEX_BUFFER
FfxUInt32 LoadJobIndex(FfxUInt32 idx)
{
    return r_job_index_buffer[idx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_INSTANCE_INFO_BUFFER
FfxBrixelizerInstanceInfo LoadInstanceInfo(FfxUInt32 index)
{
    return r_instance_info_buffer[index];
}
#endif

#if defined BRIXELIZER_BIND_SRV_INSTANCE_TRANSFORM_BUFFER
FfxFloat32x3x4 LoadInstanceTransform(FfxUInt32 idx)
{
    FfxFloat32x4 M0 = r_instance_transform_buffer[idx * FfxUInt32(3) + FfxUInt32(0)];
    FfxFloat32x4 M1 = r_instance_transform_buffer[idx * FfxUInt32(3) + FfxUInt32(1)];
    FfxFloat32x4 M2 = r_instance_transform_buffer[idx * FfxUInt32(3) + FfxUInt32(2)];

    return FfxFloat32x3x4(M0, M1, M2);
}
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxUInt32 LoadVertexBufferUInt(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxUInt32x2 LoadVertexBufferUInt2(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxUInt32x2(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 0], 
                       r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 1]);
}
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxUInt32x3 LoadVertexBufferUInt3(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxUInt32x3(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 0], 
                       r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 1],
                       r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 2]);
}
#endif  

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxFloat32x2 LoadVertexBufferFloat2(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxFloat32x2(asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 0]), 
                        asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 1]));
}
#endif  

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxFloat32x3 LoadVertexBufferFloat3(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxFloat32x3(asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 0]), 
                        asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 1]),
                        asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 2]));
}
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxFloat32x4 LoadVertexBufferFloat4(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxFloat32x4(asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 0]), 
                        asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 1]),
                        asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 2]),
                        asfloat(r_vertex_buffers[NonUniformResourceIndex(bufferIdx)][elementIdx + 3]));
}
#endif

#if defined(BRIXELIZER_BIND_SRV_SDF_ATLAS)
FfxFloat32 SampleSDFAtlas(FfxFloat32x3 uvw)
{
    return r_sdf_atlas.SampleLevel(g_wrap_linear_sampler, uvw, FfxFloat32(0.0));
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP
FfxUInt32 LoadCascadeBrickMap(FfxUInt32 elementIdx)
{
    return rw_cascade_brick_map[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP
void StoreCascadeBrickMap(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_cascade_brick_map[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAPS
FfxUInt32 LoadCascadeBrickMapArrayUniform(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return rw_cascade_brick_maps[bufferIdx][elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAPS
void StoreCascadeBrickMapArrayUniform(FfxUInt32 bufferIdx, FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_cascade_brick_maps[bufferIdx][elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS
FfxUInt32 LoadScratchCounter(FfxUInt32 counter)
{
    return rw_scratch_counters[counter];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS
void StoreScratchCounter(FfxUInt32 counter, FfxUInt32 value)
{
    rw_scratch_counters[counter] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS
void IncrementScratchCounter(FfxUInt32 counter, FfxUInt32 value, inout FfxUInt32 originalValue)
{
    InterlockedAdd(rw_scratch_counters[counter], value, originalValue);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
FfxUInt32x2 LoadScratchIndexSwapUInt2(FfxUInt32 elementIdx)
{
    return FfxUInt32x2(rw_scratch_index_swap[elementIdx + 0],
                       rw_scratch_index_swap[elementIdx + 1]);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
FfxFloat32x3 LoadScratchIndexSwapFloat3(FfxUInt32 elementIdx)
{
    return FfxFloat32x3(asfloat(rw_scratch_index_swap[elementIdx + 0]),
                        asfloat(rw_scratch_index_swap[elementIdx + 1]),
                        asfloat(rw_scratch_index_swap[elementIdx + 2]));
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
void StoreScratchIndexSwapUInt2(FfxUInt32 elementIdx, FfxUInt32x2 value)
{
    rw_scratch_index_swap[elementIdx + 0] = value.x;
    rw_scratch_index_swap[elementIdx + 1] = value.y;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
void StoreScratchIndexSwapFloat3(FfxUInt32 elementIdx, FfxFloat32x3 value)
{
    rw_scratch_index_swap[elementIdx + 0] = asuint(value.x);
    rw_scratch_index_swap[elementIdx + 1] = asuint(value.y);
    rw_scratch_index_swap[elementIdx + 2] = asuint(value.z);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
void GetScratchIndexSwapDimensions(inout FfxUInt32 size)
{
    FfxUInt32 numStructs = 0;
    FfxUInt32 stride     = 0;
    rw_scratch_index_swap.GetDimensions(numStructs, stride);
    size = numStructs * stride;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER
FfxUInt32 LoadScratchVoxelAllocationFailCounter(FfxUInt32 elementIdx)
{
    return rw_scratch_voxel_allocation_fail_counter[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER
void StoreScratchVoxelAllocationFailCounter(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_voxel_allocation_fail_counter[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
FfxUInt32 LoadScratchBricksStorage(FfxUInt32 elementIdx)
{
    return rw_scratch_bricks_storage[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
void StoreScratchBricksStorage(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_bricks_storage[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
void GetScratchBricksStorageDimensions(inout FfxUInt32 size)
{
    FfxUInt32 numStructs = 0;
    FfxUInt32 stride     = 0;
    rw_scratch_bricks_storage.GetDimensions(numStructs, stride);
    size = numStructs * stride;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
void MinScratchBricksStorage(FfxUInt32 elementIdx, FfxUInt32 value)
{
    InterlockedMin(rw_scratch_bricks_storage[elementIdx], value);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS
FfxUInt32 LoadScratchBricksStorageOffsets(FfxUInt32 elementIdx)
{
    return rw_scratch_bricks_storage_offsets[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS
void StoreScratchBricksStorageOffsets(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_bricks_storage_offsets[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST
FfxUInt32 LoadScratchBricksCompressionList(FfxUInt32 elementIdx)
{
    return rw_scratch_bricks_compression_list[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST
void StoreScratchBricksCompressionList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_bricks_compression_list[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST
FfxUInt32 LoadScratchBricksClearList(FfxUInt32 elementIdx)
{
    return rw_scratch_bricks_clear_list[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST
void StoreScratchBricksClearList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_bricks_clear_list[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS
FfxUInt32 LoadScratchJobCounter(FfxUInt32 counterIdx)
{
    return rw_scratch_job_counters[counterIdx];
}
#endif

void StoreScratchJobCounter(FfxUInt32 counterIdx, FfxUInt32 value)
{
#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS
    rw_scratch_job_counters[counterIdx] = value;
#endif
}

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS
void IncrementScratchJobCounter(FfxUInt32 counterIdx, FfxUInt32 value)
{
    InterlockedAdd(rw_scratch_job_counters[counterIdx], value);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS_SCAN
FfxUInt32 LoadScratchJobCountersScan(FfxUInt32 elementIdx)
{
    return rw_scratch_job_counters_scan[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS_SCAN
void StoreScratchJobCountersScan(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_job_counters_scan[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN
FfxUInt32 LoadGlobalJobTriangleCounterScan(FfxUInt32 elementIdx)
{
    return rw_scratch_job_global_counters_scan[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN
void StoreGlobalJobTriangleCounterScan(FfxUInt32 elementIdx, FfxUInt32 scan)
{
    rw_scratch_job_global_counters_scan[elementIdx] = scan;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES
FfxBrixelizerTriangleReference LoadScratchCR1Reference(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_references[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES
void StoreScratchCR1Reference(FfxUInt32 elementIdx, FfxBrixelizerTriangleReference ref)
{
    rw_scratch_cr1_references[elementIdx] = ref;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES
void GetScratchMaxReferences(inout FfxUInt32 size)
{
    FfxUInt32 numStructs = 0;
    FfxUInt32 stride     = 0;
    rw_scratch_cr1_references.GetDimensions(numStructs, stride);
    size = numStructs;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_COMPACTED_REFERENCES
FfxUInt32 LoadScratchCR1CompactedReferences(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_compacted_references[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_COMPACTED_REFERENCES
void StoreScratchCR1CompactedReferences(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_compacted_references[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS
FfxUInt32 LoadScratchCR1RefCounter(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_ref_counters[elementIdx];
}
#endif

void StoreScratchCR1RefCounter(FfxUInt32 elementIdx, FfxUInt32 value)
{
#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS
    rw_scratch_cr1_ref_counters[elementIdx] = value;
#endif
}

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS
void IncrementScratchCR1RefCounter(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue)
{
    InterlockedAdd(rw_scratch_cr1_ref_counters[elementIdx], value, originalValue);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN
FfxUInt32 LoadScratchCR1RefCounterScan(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_ref_counter_scan[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN
void StoreScratchCR1RefCounterScan(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_ref_counter_scan[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN
FfxUInt32 LoadVoxelReferenceGroupSum(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_ref_global_scan[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN
void StoreVoxelReferenceGroupSum(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_ref_global_scan[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN
FfxUInt32 LoadScratchCR1StampScan(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_stamp_scan[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN
void StoreScratchCR1StampScan(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_stamp_scan[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN
FfxUInt32 LoadStampGroupSum(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_stamp_global_scan[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN
void StoreStampGroupSum(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_stamp_global_scan[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_INDIRECT_ARGS
void StoreIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_indirect_args_1[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP
FfxUInt32 LoadBricksVoxelMap(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_voxel_map[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP
void StoreBricksVoxelMap(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_voxel_map[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_AABB
FfxUInt32 LoadBricksAABB(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_aabb[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_AABB
void StoreBricksAABB(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_aabb[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST
FfxUInt32 LoadBricksFreeList(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_free_list[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST
void StoreBricksFreeList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_free_list[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_CLEAR_LIST
FfxUInt32 LoadBricksClearList(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_clear_list[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_CLEAR_LIST
void StoreBricksClearList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_clear_list[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_LIST
FfxUInt32 LoadBricksDirtyList(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_eikonal_list[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_LIST
void StoreBricksDirtyList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_eikonal_list[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_MERGE_LIST
FfxUInt32 LoadBricksMergeList(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_merge_list[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_MERGE_LIST
void StoreBricksMergeList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_merge_list[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_COUNTERS
FfxUInt32 LoadBricksEikonalCounters(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_eikonal_counters[elementIdx & FFX_BRIXELIZER_BRICK_ID_MASK];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_COUNTERS
void StoreBricksEikonalCounters(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_eikonal_counters[elementIdx & FFX_BRIXELIZER_BRICK_ID_MASK] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS
FfxUInt32 LoadContextCounter(FfxUInt32 elementIdx)
{
    return rw_bctx_counters[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS
void StoreContextCounter(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_counters[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS
void IncrementContextCounter(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue)
{
    InterlockedAdd(rw_bctx_counters[elementIdx], value, originalValue);
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
FfxFloat32x3 LoadCascadeAABBTreeFloat3(FfxUInt32 elementIdx)
{
	return FfxFloat32x3(asfloat(rw_cascade_aabbtree[elementIdx + 0]),
                        asfloat(rw_cascade_aabbtree[elementIdx + 1]),
                        asfloat(rw_cascade_aabbtree[elementIdx + 2]));
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
FfxUInt32 LoadCascadeAABBTreeUInt(FfxUInt32 elementIdx)
{
	return rw_cascade_aabbtree[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
void StoreCascadeAABBTreeUInt(FfxUInt32 elementIdx, FfxUInt32 value)
{
	rw_cascade_aabbtree[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
void StoreCascadeAABBTreeFloat3(FfxUInt32 elementIdx, FfxFloat32x3 value)
{
    rw_cascade_aabbtree[elementIdx + 0] = asuint(value.x);
    rw_cascade_aabbtree[elementIdx + 1] = asuint(value.y);
    rw_cascade_aabbtree[elementIdx + 2] = asuint(value.z);
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREES
FfxFloat32x3 LoadCascadeAABBTreesFloat3(FfxUInt32 cascadeID, FfxUInt32 elementIdx)
{
    return FfxFloat32x3(asfloat(rw_cascade_aabbtrees[cascadeID][elementIdx + 0]),
                        asfloat(rw_cascade_aabbtrees[cascadeID][elementIdx + 1]),
                        asfloat(rw_cascade_aabbtrees[cascadeID][elementIdx + 2]));
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREES
FfxUInt32 LoadCascadeAABBTreesUInt(FfxUInt32 cascadeID, FfxUInt32 elementIdx)
{
	return rw_cascade_aabbtrees[cascadeID][elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SDF_ATLAS
FfxFloat32 LoadSDFAtlas(FfxUInt32x3 coord)
{
	return rw_sdf_atlas[coord];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SDF_ATLAS
void StoreSDFAtlas(FfxUInt32x3 coord, FfxFloat32 value)
{
	rw_sdf_atlas[coord] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_OUTPUT
void StoreDebugOutput(FfxUInt32x2 coord, FfxFloat32x3 output)
{
    rw_debug_output[coord] = FfxFloat32x4(output, 1.0f);
} 
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_OUTPUT
void GetDebugOutputDimensions(inout FfxUInt32 width, inout FfxUInt32 height)
{
    rw_debug_output.GetDimensions(width, height);
}
#endif

#endif // #if defined(FFX_GPU)

#endif // FFX_BRIXELIZER_CALLBACKS_HLSL_H
