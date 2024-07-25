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

#ifndef  FFX_BRIXELIZER_CALLBACKS_GLSL_H
#define  FFX_BRIXELIZER_CALLBACKS_GLSL_H

#include "ffx_brixelizer_resources.h"
#include "ffx_brixelizer_host_gpu_shared.h"
#include "ffx_brixelizer_host_gpu_shared_private.h"

#if defined(FFX_GPU)
#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#define FFX_BRIXELIZER_SIZEOF_UINT 4

// CBVs

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
    layout (set = 0, binding = BRIXELIZER_BIND_CB_CASCADE_INFO, std140) uniform BrixelizerCascadeInfo_t
    {
		FfxBrixelizerCascadeInfo data;
	} cbBrixelizerCascadeInfo;
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
    layout (set = 0, binding = BRIXELIZER_BIND_CB_CONTEXT_INFO, std140) uniform BrixelizerContextInfo_t
    {
		FfxBrixelizerContextInfo data;
	} cbBrixelizerContextInfo;
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
    layout (set = 0, binding = BRIXELIZER_BIND_CB_BUILD_INFO, std140) uniform BrixelizerBuildInfo_t
    {
		FfxBrixelizerBuildInfo data;
	} cbBrixelizerBuildInfo;
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
    layout (set = 0, binding = BRIXELIZER_BIND_CB_DEBUG_INFO, std140) uniform BrixelizerDebugInfo_t
    {
		FfxBrixelizerDebugInfo data;
	} cbBrixelizerDebugInfo;
#endif

// SRVs

#if defined BRIXELIZER_BIND_SRV_DEBUG_INSTANCE_ID_BUFFER
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_DEBUG_INSTANCE_ID_BUFFER, std430) readonly buffer BrixelizerDebugInstanceID_t
    {
		FfxUInt32 data[];
	} r_debug_instance_id;
#endif

#if defined BRIXELIZER_BIND_SRV_JOB_BUFFER
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_JOB_BUFFER, std430) readonly buffer BrixelizerJob_t
    {
		FfxBrixelizerBrixelizationJob data[];
	} r_job_buffer;
#endif

#if defined BRIXELIZER_BIND_SRV_JOB_INDEX_BUFFER
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_JOB_INDEX_BUFFER, std430) readonly buffer BrixelizerJobIndex_t
    {
		FfxUInt32 data[];
	} r_job_index_buffer;
#endif

#if defined BRIXELIZER_BIND_SRV_INSTANCE_INFO_BUFFER
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_INSTANCE_INFO_BUFFER, std430) readonly buffer BrixelizerInstanceInfo_t
    {
		FfxBrixelizerInstanceInfo data[];
	} r_instance_info_buffer;
#endif

#if defined BRIXELIZER_BIND_SRV_INSTANCE_TRANSFORM_BUFFER
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_INSTANCE_TRANSFORM_BUFFER, std430) readonly buffer BrixelizerInstanceTransform_t
    {
		FfxFloat32x4 data[];
	} r_instance_transform_buffer;
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
    layout (set = 1, binding = BRIXELIZER_BIND_SRV_VERTEX_BUFFERS, std430) readonly buffer BrixelizerVertexBuffer_t
    {
		FfxUInt32 data[];
	} r_vertex_buffers[FFX_BRIXELIZER_STATIC_CONFIG_MAX_VERTEX_BUFFERS];
#endif

#if defined BRIXELIZER_BIND_SRV_SDF_ATLAS
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_SDF_ATLAS) uniform texture3D  r_sdf_atlas;
#endif

// UAVs

#if defined BRIXELIZER_BIND_UAV_DEBUG_AABBS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_DEBUG_AABBS, std430) buffer BrixelizerDebugAABBs_t
    {
		FfxBrixelizerDebugAABB data[];
	} rw_debug_aabbs;
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE, std430) buffer BrixelizerCascadeAABBTree_t
    {
		FfxUInt32 data[];
	} rw_cascade_aabbtree;
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP, std430) buffer BrixelizerCascadeBrickMap_t
    {
		FfxUInt32 data[];
	} rw_cascade_brick_map;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS, std430) buffer BrixelizerScratchCounters_t
    {
		FfxUInt32 data[];
	} rw_scratch_counters;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP, std430) buffer BrixelizerScratchIndexSwap_t
    {
		FfxUInt32 data[];
	} rw_scratch_index_swap;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER, std430) buffer BrixelizerScratchVoxelAllocationFailCounter_t
    {
		FfxUInt32 data[];
	} rw_scratch_voxel_allocation_fail_counter;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE, std430) buffer BrixelizerScratchBricksStorage_t
    {
		FfxUInt32 data[];
	} rw_scratch_bricks_storage;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS, std430) buffer BrixelizerScratchBricksStorageOffsets_t
    {
		FfxUInt32 data[];
	} rw_scratch_bricks_storage_offsets;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST, std430) buffer BrixelizerScratchBricksCompressionList_t
    {
		FfxUInt32 data[];
	} rw_scratch_bricks_compression_list;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST, std430) buffer BrixelizerScratchBricksClearList_t
    {
		FfxUInt32 data[];
	} rw_scratch_bricks_clear_list;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS, std430) buffer BrixelizerScratchJobCounters_t
    {
		FfxUInt32 data[];
	} rw_scratch_job_counters;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS_SCAN
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS_SCAN, std430) buffer BrixelizerScratchJobCountersScan_t
    {
		FfxUInt32 data[];
	} rw_scratch_job_counters_scan;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN, std430) buffer BrixelizerScratchJobGlobalCountersScan_t
    {
		FfxUInt32 data[];
	} rw_scratch_job_global_counters_scan;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES, std430) buffer BrixelizerScratchCR1References_t
    {
		FfxBrixelizerTriangleReference data[];
	} rw_scratch_cr1_references;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_COMPACTED_REFERENCES
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_CR1_COMPACTED_REFERENCES, std430) buffer BrixelizerScratchCR1CompactedReferences_t
    {
		FfxUInt32 data[];
	} rw_scratch_cr1_compacted_references;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS, std430) buffer BrixelizerScratchCR1RefCounters_t
    {
		FfxUInt32 data[];
	} rw_scratch_cr1_ref_counters;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_INSTANCE_REF_COUNTERS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_CR1_INSTANCE_REF_COUNTERS, std430) buffer BrixelizerScratchCR1InstanceRefCounters_t
    {
		FfxUInt32 data[];
	} rw_scratch_cr1_instance_ref_counters;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN, std430) buffer BrixelizerScratchCR1RefCounterScan_t
    {
		FfxUInt32 data[];
	} rw_scratch_cr1_ref_counter_scan;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN, std430) buffer BrixelizerScratchCR1RefGlobalScan_t
    {
		FfxUInt32 data[];
	} rw_scratch_cr1_ref_global_scan;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN, std430) buffer BrixelizerScratchCR1StampScan_t
    {
		FfxUInt32 data[];
	} rw_scratch_cr1_stamp_scan;
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN, std430) buffer BrixelizerScratchCR1StampGlobalScan_t
    {
		FfxUInt32 data[];
	} rw_scratch_cr1_stamp_global_scan;
#endif

#if defined BRIXELIZER_BIND_UAV_INDIRECT_ARGS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_INDIRECT_ARGS, std430) buffer BrixelizerIndirectArgs_t
    {
		FfxUInt32 data[];
	} rw_indirect_args_1;
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP, std430) buffer BrixelizerContextBricksVoxelMap_t
    {
		FfxUInt32 data[];
	} rw_bctx_bricks_voxel_map;
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_AABB
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_AABB, std430) buffer BrixelizerContextBricksAABB_t
    {
		FfxUInt32 data[];
	} rw_bctx_bricks_aabb;
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST, std430) buffer BrixelizerContextBricksFreeList_t
    {
		FfxUInt32 data[];
	} rw_bctx_bricks_free_list;
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_CLEAR_LIST
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_CLEAR_LIST, std430) buffer BrixelizerContextBricksClearList_t
    {
		FfxUInt32 data[];
	} rw_bctx_bricks_clear_list;
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_LIST
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_LIST, std430) buffer BrixelizerContextBricksEikonalList_t
    {
		FfxUInt32 data[];
	} rw_bctx_bricks_eikonal_list;
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_MERGE_LIST
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_MERGE_LIST, std430) buffer BrixelizerContextBricksMergeList_t
    {
		FfxUInt32 data[];
	} rw_bctx_bricks_merge_list;
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_COUNTERS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_COUNTERS, std430) buffer BrixelizerContextBricksEikonalCounters_t
    {
		FfxUInt32 data[];
	} rw_bctx_bricks_eikonal_counters;
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS, std430) buffer BrixelizerContextCounters_t
    {
		FfxUInt32 data[];
	} rw_bctx_counters;
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAPS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAPS, std430) buffer BrixelizerCascadeBrickMaps_t
    {
		FfxUInt32 data[];
	} rw_cascade_brick_maps[24];
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREES
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_CASCADE_AABB_TREES, std430) buffer BrixelizerCascadeAABBTrees_t
    {
		FfxUInt32 data[];
	} rw_cascade_aabbtrees[24];
#endif

#if defined BRIXELIZER_BIND_UAV_SDF_ATLAS
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_SDF_ATLAS, r8) uniform image3D rw_sdf_atlas;
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_OUTPUT
    layout (set = 0, binding = BRIXELIZER_BIND_UAV_DEBUG_OUTPUT, rgba16f) uniform image2D rw_debug_output;
#endif

layout (set = 0, binding = 1000) uniform sampler g_wrap_linear_sampler;

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
void StoreDebugOutput(FfxUInt32x2 coord, FfxFloat32x3 outputValue);
void GetDebugOutputDimensions(inout FfxUInt32 width, inout FfxUInt32 height);

#if defined BRIXELIZER_BIND_UAV_DEBUG_AABBS
FfxBrixelizerDebugAABB GetDebugAABB(FfxUInt32 idx)
{
    return rw_debug_aabbs.data[idx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_AABBS
void StoreDebugAABB(FfxUInt32 idx, FfxBrixelizerDebugAABB aabb)
{
    rw_debug_aabbs.data[idx] = aabb;
}
#endif

#if defined BRIXELIZER_BIND_SRV_DEBUG_INSTANCE_ID_BUFFER
FfxUInt32 GetDebugInstanceID(FfxUInt32 idx)
{
    return r_debug_instance_id.data[idx];
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxBrixelizerCascadeInfo GetCascadeInfo()
{
    return cbBrixelizerCascadeInfo.data;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxUInt32x3 GetCascadeInfoClipmapOffset()
{
    return cbBrixelizerCascadeInfo.data.clipmap_offset;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxUInt32 GetCascadeInfoIndex()
{
    return cbBrixelizerCascadeInfo.data.index;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxFloat32x3 GetCascadeInfoGridMin()
{
    return cbBrixelizerCascadeInfo.data.grid_min;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxFloat32x3 GetCascadeInfoGridMax()
{
    return cbBrixelizerCascadeInfo.data.grid_max;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxFloat32 GetCascadeInfoVoxelSize()
{
    return cbBrixelizerCascadeInfo.data.voxel_size;
}
#endif

#if defined BRIXELIZER_BIND_CB_CASCADE_INFO
FfxInt32x3 GetCascadeInfoClipmapInvalidationOffset()
{
    return cbBrixelizerCascadeInfo.data.clipmap_invalidation_offset;
}
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
FfxBrixelizerCascadeInfo GetCascadeInfo(FfxUInt32 idx)
{
    return cbBrixelizerContextInfo.data.cascades[idx];
}
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
FfxUInt32 GetContextInfoNumBricks()
{
    return cbBrixelizerContextInfo.data.num_bricks;
}
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
FfxUInt32x3 GetContextInfoCascadeClipmapOffset(FfxUInt32 cascade_index)
{
    return cbBrixelizerContextInfo.data.cascades[cascade_index].clipmap_offset;
}
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
FfxBrixelizerBuildInfo GetBuildInfo()
{
    return cbBrixelizerBuildInfo.data;
}
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
FfxUInt32 GetBuildInfoDoInitialization()
{
    return cbBrixelizerBuildInfo.data.do_initialization;
}
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
FfxInt32 GetBuildInfoTreeIteration()
{
    return cbBrixelizerBuildInfo.data.tree_iteration;
}
#endif

#if defined BRIXELIZER_BIND_CB_BUILD_INFO
FfxUInt32 GetBuildInfoNumJobs()
{
    return cbBrixelizerBuildInfo.data.num_jobs;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxUInt32 GetDebugInfoMaxAABBs()
{
    return cbBrixelizerDebugInfo.data.max_aabbs;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32x4x4 GetDebugInfoInvView()
{
    return cbBrixelizerDebugInfo.data.inv_view;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32x4x4 GetDebugInfoInvProj()
{
    return cbBrixelizerDebugInfo.data.inv_proj;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32 GetDebugInfoPreviewSDFSolveEpsilon()
{
    return cbBrixelizerDebugInfo.data.preview_sdf_solve_eps;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxUInt32 GetDebugInfoStartCascadeIndex()
{
    return cbBrixelizerDebugInfo.data.start_cascade_idx;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxUInt32 GetDebugInfoEndCascadeIndex()
{
    return cbBrixelizerDebugInfo.data.end_cascade_idx;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32 GetDebugInfoTMin()
{
    return cbBrixelizerDebugInfo.data.t_min;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxFloat32 GetDebugInfoTMax()
{
    return cbBrixelizerDebugInfo.data.t_max;
}
#endif

#if defined BRIXELIZER_BIND_CB_DEBUG_INFO
FfxUInt32 GetDebugInfoDebugState()
{
    return cbBrixelizerDebugInfo.data.debug_state;
}
#endif

#if defined BRIXELIZER_BIND_SRV_JOB_BUFFER
FfxBrixelizerBrixelizationJob LoadBrixelizationJob(FfxUInt32 idx)
{
    return r_job_buffer.data[idx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_JOB_INDEX_BUFFER
FfxUInt32 LoadJobIndex(FfxUInt32 idx)
{
    return r_job_index_buffer.data[idx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_INSTANCE_INFO_BUFFER
FfxBrixelizerInstanceInfo LoadInstanceInfo(FfxUInt32 index)
{
    return r_instance_info_buffer.data[index];
}
#endif

#if defined BRIXELIZER_BIND_SRV_INSTANCE_TRANSFORM_BUFFER
FfxFloat32x3x4 LoadInstanceTransform(FfxUInt32 idx)
{
    // Instance transforms as stored in rows, so load in the 3 rows for this matrix.
    FfxFloat32x4 M0 = r_instance_transform_buffer.data[idx * FfxUInt32(3) + FfxUInt32(0)];
    FfxFloat32x4 M1 = r_instance_transform_buffer.data[idx * FfxUInt32(3) + FfxUInt32(1)];
    FfxFloat32x4 M2 = r_instance_transform_buffer.data[idx * FfxUInt32(3) + FfxUInt32(2)];

    // A GLSL mat4x3 is constructed using 4xvec3 columns so we need to reshuffle the 3 rows that we loaded into 4 columns.  

    return FfxFloat32x3x4(FfxFloat32x3(M0.x, M1.x, M2.x), 
                                 FfxFloat32x3(M0.y, M1.y, M2.y), 
                                 FfxFloat32x3(M0.z, M1.z, M2.z),
                                 FfxFloat32x3(M0.w, M1.w, M2.w));
}
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxUInt32 LoadVertexBufferUInt(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxUInt32x2 LoadVertexBufferUInt2(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxUInt32x2(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 0], 
                       r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 1]);
}
#endif

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxUInt32x3 LoadVertexBufferUInt3(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxUInt32x3(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 0], 
                       r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 1],
                       r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 2]);
}
#endif  

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxFloat32x2 LoadVertexBufferFloat2(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxFloat32x2(uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 0]), 
                        uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 1]));
}
#endif  

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxFloat32x3 LoadVertexBufferFloat3(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxFloat32x3(uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 0]), 
                        uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 1]),
                        uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 2]));
}
#endif   

#if defined BRIXELIZER_BIND_SRV_VERTEX_BUFFERS
FfxFloat32x4 LoadVertexBufferFloat4(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return FfxFloat32x4(uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 0]), 
                        uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 1]),
                        uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 2]),
                        uintBitsToFloat(r_vertex_buffers[nonuniformEXT(bufferIdx)].data[elementIdx + 3]));
}
#endif    

#if defined(BRIXELIZER_BIND_SRV_SDF_ATLAS)
FfxFloat32 SampleSDFAtlas(FfxFloat32x3 uvw)
{
    return textureLod(sampler3D(r_sdf_atlas, g_wrap_linear_sampler), uvw, FfxFloat32(0.0f)).r;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP
FfxUInt32 LoadCascadeBrickMap(FfxUInt32 elementIdx)
{
    return rw_cascade_brick_map.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAP
void StoreCascadeBrickMap(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_cascade_brick_map.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAPS
FfxUInt32 LoadCascadeBrickMapArrayUniform(FfxUInt32 bufferIdx, FfxUInt32 elementIdx)
{
    return rw_cascade_brick_maps[bufferIdx].data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_BRICK_MAPS
void StoreCascadeBrickMapArrayUniform(FfxUInt32 bufferIdx, FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_cascade_brick_maps[bufferIdx].data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS
FfxUInt32 LoadScratchCounter(FfxUInt32 counter)
{
    return rw_scratch_counters.data[counter];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS
void StoreScratchCounter(FfxUInt32 counter, FfxUInt32 value)
{
    rw_scratch_counters.data[counter] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_COUNTERS
void IncrementScratchCounter(FfxUInt32 counter, FfxUInt32 value, inout FfxUInt32 originalValue)
{
    originalValue = atomicAdd(rw_scratch_counters.data[counter], value);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
FfxUInt32x2 LoadScratchIndexSwapUInt2(FfxUInt32 elementIdx)
{
    return FfxUInt32x2(rw_scratch_index_swap.data[elementIdx + 0],
                       rw_scratch_index_swap.data[elementIdx + 1]);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
FfxFloat32x3 LoadScratchIndexSwapFloat3(FfxUInt32 elementIdx)
{
    return FfxFloat32x3(uintBitsToFloat(rw_scratch_index_swap.data[elementIdx + 0]),
                        uintBitsToFloat(rw_scratch_index_swap.data[elementIdx + 1]),
                        uintBitsToFloat(rw_scratch_index_swap.data[elementIdx + 2]));
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
void StoreScratchIndexSwapUInt2(FfxUInt32 elementIdx, FfxUInt32x2 value)
{
    rw_scratch_index_swap.data[elementIdx + 0] = value.x;
    rw_scratch_index_swap.data[elementIdx + 1] = value.y;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
void StoreScratchIndexSwapFloat3(FfxUInt32 elementIdx, FfxFloat32x3 value)
{
    rw_scratch_index_swap.data[elementIdx + 0] = floatBitsToUint(value.x);
    rw_scratch_index_swap.data[elementIdx + 1] = floatBitsToUint(value.y);
    rw_scratch_index_swap.data[elementIdx + 2] = floatBitsToUint(value.z);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_INDEX_SWAP
void GetScratchIndexSwapDimensions(inout FfxUInt32 size)
{
    FfxUInt32 numStructs = rw_scratch_index_swap.data.length();
    FfxUInt32 stride     = 4;
    size = numStructs * stride;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER
FfxUInt32 LoadScratchVoxelAllocationFailCounter(FfxUInt32 elementIdx)
{
    return rw_scratch_voxel_allocation_fail_counter.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_VOXEL_ALLOCATION_FAIL_COUNTER
void StoreScratchVoxelAllocationFailCounter(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_voxel_allocation_fail_counter.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
FfxUInt32 LoadScratchBricksStorage(FfxUInt32 elementIdx)
{
    return rw_scratch_bricks_storage.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
void StoreScratchBricksStorage(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_bricks_storage.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
void GetScratchBricksStorageDimensions(inout FfxUInt32 size)
{
    FfxUInt32 numStructs = rw_scratch_bricks_storage.data.length();
    FfxUInt32 stride     = 4;
    size = numStructs * stride;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE
void MinScratchBricksStorage(FfxUInt32 elementIdx, FfxUInt32 value)
{
    atomicMin(rw_scratch_bricks_storage.data[elementIdx], value);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS
FfxUInt32 LoadScratchBricksStorageOffsets(FfxUInt32 elementIdx)
{
    return rw_scratch_bricks_storage_offsets.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_STORAGE_OFFSETS
void StoreScratchBricksStorageOffsets(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_bricks_storage_offsets.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST
FfxUInt32 LoadScratchBricksCompressionList(FfxUInt32 elementIdx)
{
    return rw_scratch_bricks_compression_list.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_COMPRESSION_LIST
void StoreScratchBricksCompressionList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_bricks_compression_list.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST
FfxUInt32 LoadScratchBricksClearList(FfxUInt32 elementIdx)
{
    return rw_scratch_bricks_clear_list.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_BRICKS_CLEAR_LIST
void StoreScratchBricksClearList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_bricks_clear_list.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS
FfxUInt32 LoadScratchJobCounter(FfxUInt32 counterIdx)
{
    return rw_scratch_job_counters.data[counterIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS
void StoreScratchJobCounter(FfxUInt32 counterIdx, FfxUInt32 value)
{
    rw_scratch_job_counters.data[counterIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS
void IncrementScratchJobCounter(FfxUInt32 counterIdx, FfxUInt32 value)
{
    atomicAdd(rw_scratch_job_counters.data[counterIdx], value);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS_SCAN
FfxUInt32 LoadScratchJobCountersScan(FfxUInt32 elementIdx)
{
    return rw_scratch_job_counters_scan.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_COUNTERS_SCAN
void StoreScratchJobCountersScan(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_job_counters_scan.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN
FfxUInt32 LoadGlobalJobTriangleCounterScan(FfxUInt32 elementIdx)
{
    return rw_scratch_job_global_counters_scan.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_JOB_GLOBAL_COUNTERS_SCAN
void StoreGlobalJobTriangleCounterScan(FfxUInt32 elementIdx, FfxUInt32 scan)
{
    rw_scratch_job_global_counters_scan.data[elementIdx] = scan;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES
FfxBrixelizerTriangleReference LoadScratchCR1Reference(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_references.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES
void StoreScratchCR1Reference(FfxUInt32 elementIdx, FfxBrixelizerTriangleReference ref)
{
    rw_scratch_cr1_references.data[elementIdx] = ref;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REFERENCES
void GetScratchMaxReferences(inout FfxUInt32 size)
{
    size = rw_scratch_cr1_references.data.length();
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_COMPACTED_REFERENCES
FfxUInt32 LoadScratchCR1CompactedReferences(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_compacted_references.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_COMPACTED_REFERENCES
void StoreScratchCR1CompactedReferences(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_compacted_references.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS
FfxUInt32 LoadScratchCR1RefCounter(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_ref_counters.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS
void StoreScratchCR1RefCounter(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_ref_counters.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTERS
void IncrementScratchCR1RefCounter(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue)
{
    originalValue = atomicAdd(rw_scratch_cr1_ref_counters.data[elementIdx], value);
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN
FfxUInt32 LoadScratchCR1RefCounterScan(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_ref_counter_scan.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_COUNTER_SCAN
void StoreScratchCR1RefCounterScan(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_ref_counter_scan.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN
FfxUInt32 LoadVoxelReferenceGroupSum(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_ref_global_scan.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_REF_GLOBAL_SCAN
void StoreVoxelReferenceGroupSum(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_ref_global_scan.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN
FfxUInt32 LoadScratchCR1StampScan(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_stamp_scan.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_SCAN
void StoreScratchCR1StampScan(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_stamp_scan.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN
FfxUInt32 LoadStampGroupSum(FfxUInt32 elementIdx)
{
    return rw_scratch_cr1_stamp_global_scan.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SCRATCH_CR1_STAMP_GLOBAL_SCAN
void StoreStampGroupSum(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_scratch_cr1_stamp_global_scan.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_INDIRECT_ARGS
void StoreIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_indirect_args_1.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP
FfxUInt32 LoadBricksVoxelMap(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_voxel_map.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_VOXEL_MAP
void StoreBricksVoxelMap(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_voxel_map.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_AABB
FfxUInt32 LoadBricksAABB(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_aabb.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_AABB
void StoreBricksAABB(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_aabb.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST
FfxUInt32 LoadBricksFreeList(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_free_list.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_FREE_LIST
void StoreBricksFreeList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_free_list.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_CLEAR_LIST
FfxUInt32 LoadBricksClearList(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_clear_list.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_CLEAR_LIST
void StoreBricksClearList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_clear_list.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_LIST
FfxUInt32 LoadBricksDirtyList(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_eikonal_list.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_LIST
void StoreBricksDirtyList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_eikonal_list.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_MERGE_LIST
FfxUInt32 LoadBricksMergeList(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_merge_list.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_MERGE_LIST
void StoreBricksMergeList(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_merge_list.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_COUNTERS
FfxUInt32 LoadBricksEikonalCounters(FfxUInt32 elementIdx)
{
    return rw_bctx_bricks_eikonal_counters.data[elementIdx & FFX_BRIXELIZER_BRICK_ID_MASK];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_BRICKS_EIKONAL_COUNTERS
void StoreBricksEikonalCounters(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_bricks_eikonal_counters.data[elementIdx & FFX_BRIXELIZER_BRICK_ID_MASK] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS
FfxUInt32 LoadContextCounter(FfxUInt32 elementIdx)
{
    return rw_bctx_counters.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS
void StoreContextCounter(FfxUInt32 elementIdx, FfxUInt32 value)
{
    rw_bctx_counters.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CONTEXT_COUNTERS
void IncrementContextCounter(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue)
{
    originalValue = atomicAdd(rw_bctx_counters.data[elementIdx], value);
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
FfxFloat32x3 LoadCascadeAABBTreeFloat3(FfxUInt32 elementIdx)
{
	return FfxFloat32x3(uintBitsToFloat(rw_cascade_aabbtree.data[elementIdx + 0]),
                        uintBitsToFloat(rw_cascade_aabbtree.data[elementIdx + 1]),
                        uintBitsToFloat(rw_cascade_aabbtree.data[elementIdx + 2]));
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
FfxUInt32 LoadCascadeAABBTreeUInt(FfxUInt32 elementIdx)
{
	return rw_cascade_aabbtree.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
void StoreCascadeAABBTreeUInt(FfxUInt32 elementIdx, FfxUInt32 value)
{
	rw_cascade_aabbtree.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREE
void StoreCascadeAABBTreeFloat3(FfxUInt32 elementIdx, FfxFloat32x3 value)
{
    rw_cascade_aabbtree.data[elementIdx + 0] = floatBitsToUint(value.x);
    rw_cascade_aabbtree.data[elementIdx + 1] = floatBitsToUint(value.y);
    rw_cascade_aabbtree.data[elementIdx + 2] = floatBitsToUint(value.z);
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREES
FfxFloat32x3 LoadCascadeAABBTreesFloat3(FfxUInt32 cascadeID, FfxUInt32 elementIdx)
{
    return FfxFloat32x3(uintBitsToFloat(rw_cascade_aabbtrees[cascadeID].data[elementIdx + 0]),
                        uintBitsToFloat(rw_cascade_aabbtrees[cascadeID].data[elementIdx + 1]),
                        uintBitsToFloat(rw_cascade_aabbtrees[cascadeID].data[elementIdx + 2]));
}
#endif

#if defined BRIXELIZER_BIND_UAV_CASCADE_AABB_TREES
FfxUInt32 LoadCascadeAABBTreesUInt(FfxUInt32 cascadeID, FfxUInt32 elementIdx)
{
	return rw_cascade_aabbtrees[cascadeID].data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_UAV_SDF_ATLAS
FfxFloat32 LoadSDFAtlas(FfxUInt32x3 coord)
{
	return imageLoad(rw_sdf_atlas, FfxInt32x3(coord)).x;
}
#endif

#if defined BRIXELIZER_BIND_UAV_SDF_ATLAS
void StoreSDFAtlas(FfxUInt32x3 coord, FfxFloat32 value)
{
    imageStore(rw_sdf_atlas, FfxInt32x3(coord), FfxFloat32x4(value, 0.0f, 0.0f, 0.0f));
}
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_OUTPUT
void StoreDebugOutput(FfxUInt32x2 coord, FfxFloat32x3 outputValue)
{
    imageStore(rw_debug_output, FfxInt32x2(coord), FfxFloat32x4(outputValue, 1.0f));
} 
#endif

#if defined BRIXELIZER_BIND_UAV_DEBUG_OUTPUT
void GetDebugOutputDimensions(inout FfxUInt32 width, inout FfxUInt32 height)
{
    FfxInt32x2 size = imageSize(rw_debug_output);
    width = FfxUInt32(size.x);
    height = FfxUInt32(size.y);
}
#endif

#endif // #if defined(FFX_GPU)

#endif // FFX_BRIXELIZER_CALLBACKS_GLSL_H