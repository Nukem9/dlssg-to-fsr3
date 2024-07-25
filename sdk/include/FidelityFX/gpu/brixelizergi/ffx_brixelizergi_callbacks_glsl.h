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

#ifndef  FFX_BRIXELIZER_GI_CALLBACKS_GLSL_H
#define  FFX_BRIXELIZER_GI_CALLBACKS_GLSL_H

#include "../ffx_core.h"

#include "ffx_brixelizer_host_gpu_shared.h"
#include "ffx_brixelizergi_host_interface.h"

#if defined(FFX_GPU)
#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

FfxBrixelizerGIConstants GetGIConstants();
FfxUInt32x2 GetBufferDimensions();
FfxFloat32x2 GetBufferDimensionsF32();
FfxFloat32x2 GetProbeBufferDimensionsF32();
FfxUInt32 GetFrameIndex();
FfxUInt32x2 GetTileBufferDimensions();
FfxFloat32x3 GetCameraPosition();
FfxFloat32x4x4 GetViewMatrix();
FfxFloat32x4x4 GetViewProjectionMatrix();
FfxFloat32x4x4 GetInverseViewMatrix();
FfxFloat32x4x4 GetInverseProjectionMatrix();
FfxFloat32x4x4 GetInverseViewProjectionMatrix();
FfxFloat32x4x4 GetPreviousViewProjectionMatrix();
FfxFloat32x4x4 GetPreviousInverseViewMatrix();
FfxFloat32x4x4 GetPreviousInverseProjectionMatrix();
FfxFloat32 GetRoughnessThreshold();
FfxUInt32 GetRoughnessChannel();
FfxFloat32 GetEnvironmentMapIntensity();
FfxUInt32 GetTracingConstantsStartCascade();
FfxUInt32 GetTracingConstantsEndCascade();
FfxFloat32 GetTracingConstantsRayPushoff();
FfxFloat32 GetTracingConstantsTMin();
FfxFloat32 GetTracingConstantsTMax();
FfxFloat32 GetTracingConstantsSDFSolveEpsilon();
FfxFloat32 GetTracingConstantsSpecularRayPushoff();
FfxFloat32 GetTracingConstantsSpecularSDFSolveEpsilon();
FfxUInt32 GetPassConstantsCascadeIndex();
FfxFloat32 GetPassConstantsEnergyDecayK();
FfxBrixelizerGIScalingConstants GetScalingConstants();
FfxUInt32 GetScalingRoughnessChannel();
FfxUInt32 LoadTempSpawnMask(FfxUInt32x2 coord);
void StoreTempSpawnMask(FfxUInt32x2 coord, FfxUInt32 value);
FfxUInt32 LoadTempRandomSeed(FfxUInt32x2 coord);
void StoreTempRandomSeed(FfxUInt32x2 coord, FfxUInt32 value);
FfxUInt32x4 LoadTempSpecularPretraceTarget(FfxUInt32x2 coord);
void StoreTempSpecularPretraceTarget(FfxUInt32x2 coord, FfxUInt32x4 value);
FfxFloat32x4 LoadStaticScreenProbesStat(FfxUInt32x2 coord);
void StoreStaticScreenProbesStat(FfxUInt32x2 coord, FfxFloat32x4 value);
FfxFloat32x4 SampleSpecularTargetSRV(FfxFloat32x2 uv);
FfxFloat32x4 LoadSpecularTargetSRV(FfxUInt32x2 coord);
FfxFloat32x4 LoadSpecularTarget(FfxUInt32x2 coord);
void StoreSpecularTarget(FfxUInt32x2 coord, FfxFloat32x4 value);
FfxUInt32x4 LoadStaticProbeInfo(FfxUInt32 index);
void StoreStaticProbeInfo(FfxUInt32 index, FfxUInt32x4 value);
FfxUInt32x2 LoadStaticProbeSHBuffer(FfxUInt32 index);
void StoreStaticProbeSHBuffer(FfxUInt32 index, FfxUInt32x2 value);
FfxUInt32x4 LoadTempProbeInfo(FfxUInt32 index);
void StoreTempProbeInfo(FfxUInt32 index, FfxUInt32x4 info);
FfxUInt32x2 LoadTempProbeSHBuffer(FfxUInt32 index);
void StoreTempProbeSHBuffer(FfxUInt32 index, FfxUInt32x2 value);
FfxFloat32x3 SamplePrevLitOutput(FfxFloat32x2 uv);
FfxFloat32 LoadDepth(FfxUInt32x2 pixel_coordinate);
FfxFloat32 LoadRoughness(FfxUInt32x2 pixel_coordinate);
FfxFloat32 LoadPrevDepth(FfxUInt32x2 pixel_coordinate);
FfxFloat32 SampleDepth(FfxFloat32x2 uv);
FfxFloat32 SamplePrevDepth(FfxFloat32x2 uv);
FfxFloat32x3 LoadWorldNormal(FfxUInt32x2 pixel_coordinate);
FfxFloat32x3 LoadPrevWorldNormal(FfxUInt32x2 pixel_coordinate);
FfxFloat32x3 SampleWorldNormal(FfxFloat32x2 uv);
FfxFloat32x3 SamplePrevWorldNormal(FfxFloat32x2 uv);
FfxFloat32x2 SampleMotionVector(FfxFloat32x2 uv);
FfxFloat32x2 LoadMotionVector(FfxUInt32x2 pixel_coordinate);
FfxUInt32 LoadDisocclusionMask(FfxUInt32x2 pixel_coordinate);
void StoreDisocclusionMask(FfxUInt32x2 pixel_coordinate, float value);
FfxUInt32 LoadRaySwapIndirectArgs(FfxUInt32 elementIdx);
void StoreRaySwapIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value);
void IncrementRaySwapIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue);
FfxUInt32x2 LoadBricksDirectSH(FfxUInt32 elementIdx);
void StoreBricksDirectSH(FfxUInt32 elementIdx, FfxUInt32x2 value);
FfxUInt32x2 LoadBricksSH(FfxUInt32 elementIdx);
void StoreBricksSH(FfxUInt32 elementIdx, FfxUInt32x2 value);
FfxUInt32x4 LoadBricksSHState(FfxUInt32 elementIdx);
void StoreBricksSHState(FfxUInt32 elementIdx, FfxUInt32x4 value);
FfxUInt32 LoadTempSpecularRaySwap(FfxUInt32 elementIdx);
void StoreTempSpecularRaySwap(FfxUInt32 elementIdx, FfxUInt32 value);
FfxFloat32x2 SampleBlueNoise(FfxUInt32x2 pixel, FfxUInt32 sample_index, FfxUInt32 dimension_offset);
FfxFloat32x2 SampleBlueNoise(in FfxUInt32x2 pixel, in FfxUInt32 sample_index);
FfxFloat32x4 LoadStaticScreenProbesSRV(FfxUInt32x2 coord);
FfxFloat32x4 SampleStaticScreenProbesSRV(FfxFloat32x2 uv);
FfxFloat32x4 LoadStaticScreenProbes(FfxUInt32x2 coord);
void StoreStaticScreenProbes(FfxUInt32x2 coord, FfxFloat32x4 value);
FfxFloat32x4 SampleStaticGITargetSRV(FfxFloat32x2 uv);
FfxFloat32x4 LoadStaticGITargetSRV(FfxUInt32x2 coord);
void StoreStaticGITarget(FfxUInt32x2 coord, FfxFloat32x4 value);
FfxFloat32x4 LoadDebugTraget(FfxUInt32x2 coord);
void StoreDebugTarget(FfxUInt32x2 coord, FfxFloat32x4 value);
FfxFloat32x3 SampleEnvironmentMap(FfxFloat32x3 world_space_reflected_direction);
FfxFloat32x3 SampleRadianceCacheSRV(FfxFloat32x3 uvw);
FfxFloat32x3 LoadRadianceCache(FfxUInt32x3 coord);
void StoreRadianceCache(FfxUInt32x3 coord, FfxFloat32x3 value);
void StoreDebugVisualization(FfxUInt32x2 pixel_coordinate, FfxFloat32x4 value);

FfxFloat32   LoadSourceDepth(FfxUInt32x2 coord);
FfxFloat32x3 LoadSourceNormal(FfxUInt32x2 coord);
FfxFloat32x4 GatherSourceDepth(FfxFloat32x2 uv);
FfxFloat32x4 GatherSourcePrevDepth(FfxFloat32x2 uv);
FfxFloat32x3 SampleSourceNormal(FfxFloat32x2 uv);
FfxFloat32x3 SampleSourcePrevNormal(FfxFloat32x2 uv);
FfxFloat32   SampleSourceRoughness(FfxFloat32x2 uv);
FfxFloat32x2 SampleSourceMotionVector(FfxFloat32x2 uv);
FfxFloat32x3 SampleSourcePrevLitOutput(FfxFloat32x2 uv);
FfxFloat32x3 SampleDownsampledDiffuseGI(FfxFloat32x2 uv);
FfxFloat32x3 SampleDownsampledSpecularGI(FfxFloat32x2 uv);

void StoreDownsampledDepth(FfxUInt32x2 pixel_coordinate, FfxFloat32 value);
void StoreDownsampledPrevDepth(FfxUInt32x2 pixel_coordinate, FfxFloat32 value);
void StoreDownsampledNormal(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value);
void StoreDownsampledPrevNormal(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value);
void StoreDownsampledRoughness(FfxUInt32x2 pixel_coordinate, FfxFloat32 value);
void StoreDownsampledMotionVector(FfxUInt32x2 pixel_coordinate, FfxFloat32x2 value);
void StoreDownsampledPrevLitOutput(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value);
void StoreUpsampledDiffuseGI(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value);
void StoreUpsampledSpecularGI(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value);

FfxBrixelizerContextInfo GetContextInfo();
FfxBrixelizerCascadeInfo GetCascadeInfo(FfxUInt32 cascadeID);
FfxBrixelizerCascadeInfo GetCascadeInfoNonUniform(FfxUInt32 cascadeID);
FfxFloat32 SampleSDFAtlas(FfxFloat32x3 uvw);
FfxUInt32 LoadContextCounter(FfxUInt32 elementIdx);
FfxUInt32 LoadBricksClearList(FfxUInt32 elementIdx);
FfxFloat32x3 LoadCascadeAABBTreesFloat3(FfxUInt32 cascadeID, FfxUInt32 elementIndex);
FfxUInt32 LoadCascadeAABBTreesUInt(FfxUInt32 cascadeID, FfxUInt32 elementIndex);
FfxUInt32 LoadBricksVoxelMap(FfxUInt32 elementIndex);
FfxUInt32 LoadBricksAABB(FfxUInt32 elementIndex);
FfxUInt32 LoadCascadeBrickMapArrayUniform(FfxUInt32 cascadeID, FfxUInt32 elementIndex);
FfxUInt32 LoadCascadeBrickMapArrayNonUniform(FfxUInt32 cascadeID, FfxUInt32 elementIndex);

// CBVs

#if defined BRIXELIZER_GI_BIND_CB_SDFGI
    layout (set = 0, binding = BRIXELIZER_GI_BIND_CB_SDFGI, std140) uniform GIConstants_t
    {
		FfxBrixelizerGIConstants data;
	} g_sdfgi_constants;
#endif

#if defined BRIXELIZER_GI_BIND_CB_PASS_CONSTANTS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_CB_PASS_CONSTANTS, std140) uniform PassConstants_t
    {
		FfxBrixelizerGIPassConstants data;
	} g_pass_constants;
#endif

#if defined BRIXELIZER_GI_BIND_CB_SCALING_CONSTANTS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_CB_SCALING_CONSTANTS, std140) uniform ScalingConstants_t
    {
		FfxBrixelizerGIScalingConstants data;
	} g_scaling_constants;
#endif

// SRVs 

#if defined BRIXELIZER_GI_BIND_SRV_DISOCCLUSION_MASK
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_DISOCCLUSION_MASK)     uniform texture2D g_r_disocclusion_mask;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_STATIC_GI_TARGET
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_STATIC_GI_TARGET)      uniform texture2D g_sdfgi_r_static_gitarget;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_STATIC_SCREEN_PROBES
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_STATIC_SCREEN_PROBES)  uniform texture2D g_sdfgi_r_static_screen_probes;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SPECULAR_TARGET
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_SPECULAR_TARGET)       uniform texture2D g_sdfgi_r_specular_target;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_RADIANCE_CACHE
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_RADIANCE_CACHE)        uniform texture3D g_bctx_radiance_cache;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_PREV_LIT_OUTPUT
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_PREV_LIT_OUTPUT) uniform texture2D g_prev_lit_output;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH)           uniform texture2D g_depth;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH)   uniform texture2D g_history_depth;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL)          uniform texture2D g_normal;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL)  uniform texture2D g_history_normal;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_ROUGHNESS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_ROUGHNESS)       uniform texture2D g_roughness;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS)  uniform texture2D g_motion_vectors;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_ENVIRONMENT_MAP
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_ENVIRONMENT_MAP) uniform textureCube g_environment_map;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_BLUE_NOISE
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_INPUT_BLUE_NOISE)      uniform texture2D g_blue_noise;
#endif

// Downsampled SRVs

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_PREV_LIT_OUTPUT
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_SOURCE_PREV_LIT_OUTPUT)  uniform texture2D g_src_prev_lit_output;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_DEPTH
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_SOURCE_DEPTH)            uniform texture2D g_src_depth;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_DEPTH
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_DEPTH)    uniform texture2D g_src_history_depth;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_NORMAL
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_SOURCE_NORMAL)           uniform texture2D g_src_normal;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_NORMAL
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_NORMAL)   uniform texture2D g_src_history_normal;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_ROUGHNESS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_SOURCE_ROUGHNESS)        uniform texture2D g_src_roughness;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_MOTION_VECTORS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_SOURCE_MOTION_VECTORS)   uniform texture2D g_src_motion_vectors;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_DIFFUSE_GI
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_DIFFUSE_GI)  uniform texture2D g_downsampled_diffuse_gi;
#endif

#if defined BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_SPECULAR_GI
    layout (set = 0, binding = BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_SPECULAR_GI) uniform texture2D g_downsampled_specular_gi;
#endif

// UAVs 

#if defined BRIXELIZER_GI_BIND_UAV_DISOCCLUSION_MASK
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DISOCCLUSION_MASK, r8)                  uniform image2D g_rw_disocclusion_mask;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES, rgba16f)          uniform image2D g_sdfgi_rw_static_screen_probes;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_GI_TARGET
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_STATIC_GI_TARGET, rgba16f)              uniform image2D g_sdfgi_rw_static_gitarget;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DEBUG_TARGET
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DEBUG_TARGET, rgba16f)                  uniform image2D g_sdfgi_rw_debug_target;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RADIANCE_CACHE
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_RADIANCE_CACHE, r11f_g11f_b10f)         uniform image3D g_rw_bctx_radiance_cache;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPAWN_MASK
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_TEMP_SPAWN_MASK, r32ui)                 uniform uimage2D g_sdfgi_rw_temp_spawn_mask;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_RAND_SEED
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_TEMP_RAND_SEED, r8ui)                   uniform uimage2D g_sdfgi_rw_temp_rand_seed;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_PRETRACE_TARGET
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_PRETRACE_TARGET, rgba32ui) uniform uimage2D g_sdfgi_rw_temp_specular_pretrace_target;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES_STAT
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES_STAT, rgba16f)     uniform image2D g_sdfgi_rw_static_screen_probes_stat;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_SPECULAR_TARGET
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_SPECULAR_TARGET, rgba16f)               uniform image2D g_sdfgi_rw_specular_target;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DEBUG_VISUALIZATION
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DEBUG_VISUALIZATION, rgba16f)           uniform image2D g_rw_debug_visualization;
#endif

// Downsampled resources

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_PREV_LIT_OUTPUT
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_PREV_LIT_OUTPUT, rgba16f)   uniform image2D g_downsampled_prev_lit_output;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_DEPTH
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_DEPTH, r32f)                uniform image2D g_downsampled_depth;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_DEPTH
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_DEPTH, r32f)        uniform image2D g_downsampled_history_depth;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_NORMAL
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_NORMAL, rgba16f)            uniform image2D g_downsampled_normal;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_NORMAL
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_NORMAL, rgba16f)    uniform image2D g_downsampled_history_normal;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_ROUGHNESS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_ROUGHNESS, r8)              uniform image2D g_downsampled_roughness;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_MOTION_VECTORS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_MOTION_VECTORS, rg16f)      uniform image2D g_downsampled_motion_vectors;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_UPSAMPLED_DIFFUSE_GI
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_UPSAMPLED_DIFFUSE_GI, rgba16f)          uniform image2D g_upsampled_diffuse_gi;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_UPSAMPLED_SPECULAR_GI
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_UPSAMPLED_SPECULAR_GI, rgba16f)         uniform image2D g_upsampled_specular_gi;
#endif

// UAVs 

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_INFO
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_INFO, std430) buffer BrixelizerGIStaticProbeInfo_t
    {
		FfxUInt32x4 data[];
	} g_sdfgi_rw_static_probe_info;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_SH_BUFFER
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_SH_BUFFER, std430) buffer BrixelizerGIStaticProbeSH_t
    {
		FfxUInt32x2 data[];
	} g_sdfgi_rw_static_probe_sh_buffer;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_INFO
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_INFO, std430) buffer BrixelizerGITempProbeInfo_t
    {
		FfxUInt32x4 data[];
	} g_sdfgi_rw_temp_probe_info;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_SH_BUFFER
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_SH_BUFFER, std430) buffer BrixelizerGITempProbeSH_t
    {
		FfxUInt32x2 data[];
	} g_sdfgi_rw_temp_probe_sh_buffer;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS, std430) buffer BrixelizerGIRaySwapIndirectArgs_t
    {
		FfxUInt32 data[];
	} g_sdfgi_rw_ray_swap_indirect_args;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_DIRECT_SH
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_BRICKS_DIRECT_SH, std430) buffer BrixelizerGIBricksDirectSH_t
    {
		FfxUInt32x2 data[];
	} g_bctx_bricks_direct_sh;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_BRICKS_SH, std430) buffer BrixelizerGIBricksSH_t
    {
		FfxUInt32x2 data[];
	} g_bctx_bricks_sh;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH_STATE
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_BRICKS_SH_STATE, std430) buffer BrixelizerGIBricksSHState_t
    {
		FfxUInt32x4 data[];
	} g_bctx_bricks_sh_state;
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_RAY_SWAP
    layout (set = 0, binding = BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_RAY_SWAP, std430) buffer BrixelizerGITempSpecularRaySwap_t
    {
		FfxUInt32 data[];
	} g_sdfgi_rw_temp_specular_ray_swap;
#endif

// Brixelizer Resources

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
    layout (set = 0, binding = BRIXELIZER_BIND_CB_CONTEXT_INFO, std140) uniform BrixelizerContextInfo_t
    {
		FfxBrixelizerContextInfo data;
	} g_bx_context_info;
#endif

#if defined BRIXELIZER_BIND_SRV_SDF_ATLAS
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_SDF_ATLAS) uniform texture3D r_sdf_atlas;
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_AABB
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_AABB, std430) readonly buffer BrixelizerBricksAABB_t
    {
		FfxUInt32 data[];
	} r_bctx_bricks_aabb;
#endif 

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_VOXEL_MAP
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_VOXEL_MAP, std430) readonly buffer BrixelizerVoxelMap_t
    {
		FfxUInt32 data[];
	} r_bctx_bricks_voxel_map;
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_COUNTERS
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_CONTEXT_COUNTERS, std430) readonly buffer BrixelizerCounters_t
    {
		FfxUInt32 data[];
	} r_bctx_counters;
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_CLEAR_LIST
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_CLEAR_LIST, std430) readonly buffer BrixelizerBricksClearList_t
    {
		FfxUInt32 data[];
	} r_bctx_bricks_clear_list;
#endif

#if defined BRIXELIZER_BIND_SRV_CASCADE_AABB_TREES
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_CASCADE_AABB_TREES, std430) readonly buffer BrixelizerCascadeAABBTrees_t
    {
		FfxUInt32 data[];
	} r_cascade_aabbtrees[24];
#endif

#if defined BRIXELIZER_BIND_SRV_CASCADE_BRICK_MAPS
    layout (set = 0, binding = BRIXELIZER_BIND_SRV_CASCADE_BRICK_MAPS, std430) readonly buffer BrixelizerCascadeBrickMaps_t
    {
		FfxUInt32 data[];
	} r_cascade_brick_maps[24];
#endif 

layout (set = 0, binding = 1000) uniform sampler g_clamp_linear_sampler;
layout (set = 0, binding = 1001) uniform sampler g_clamp_nearest_sampler;
layout (set = 0, binding = 1002) uniform sampler g_wrap_linear_sampler;
layout (set = 0, binding = 1003) uniform sampler g_wrap_nearest_sampler;

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
FfxBrixelizerContextInfo GetContextInfo()
{
    return g_bx_context_info.data;
}

FfxBrixelizerCascadeInfo GetCascadeInfo(FfxUInt32 cascadeID)
{
    return g_bx_context_info.data.cascades[cascadeID];
}

FfxBrixelizerCascadeInfo GetCascadeInfoNonUniform(FfxUInt32 cascadeID)
{
    return g_bx_context_info.data.cascades[nonuniformEXT(cascadeID)];
}
#endif

#if defined BRIXELIZER_BIND_SRV_SDF_ATLAS
FfxFloat32 SampleSDFAtlas(FfxFloat32x3 uvw)
{
    return textureLod(sampler3D(r_sdf_atlas, g_wrap_linear_sampler), uvw, 0.0f).r;
}
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_COUNTERS
FfxUInt32 LoadContextCounter(FfxUInt32 elementIdx)
{
    return r_bctx_counters.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_CLEAR_LIST
FfxUInt32 LoadBricksClearList(FfxUInt32 elementIdx)
{
    return r_bctx_bricks_clear_list.data[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CASCADE_AABB_TREES
FfxFloat32x3 LoadCascadeAABBTreesFloat3(FfxUInt32 cascadeID, FfxUInt32 elementIndex)
{ 
    return FfxFloat32x3(uintBitsToFloat(r_cascade_aabbtrees[cascadeID].data[elementIndex + 0]),
                        uintBitsToFloat(r_cascade_aabbtrees[cascadeID].data[elementIndex + 1]),
                        uintBitsToFloat(r_cascade_aabbtrees[cascadeID].data[elementIndex + 2]));
}

FfxUInt32 LoadCascadeAABBTreesUInt(FfxUInt32 cascadeID, FfxUInt32 elementIndex)
{
	return r_cascade_aabbtrees[cascadeID].data[elementIndex];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_VOXEL_MAP
FfxUInt32 LoadBricksVoxelMap(FfxUInt32 elementIndex)
{
    return r_bctx_bricks_voxel_map.data[elementIndex];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_AABB
FfxUInt32 LoadBricksAABB(FfxUInt32 elementIndex)
{
    return r_bctx_bricks_aabb.data[elementIndex];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CASCADE_BRICK_MAPS
FfxUInt32 LoadCascadeBrickMapArrayUniform(FfxUInt32 cascadeID, FfxUInt32 elementIndex)
{
    return r_cascade_brick_maps[cascadeID].data[elementIndex];
}

FfxUInt32 LoadCascadeBrickMapArrayNonUniform(FfxUInt32 cascadeID, FfxUInt32 elementIndex)
{
    return r_cascade_brick_maps[nonuniformEXT(cascadeID)].data[elementIndex];
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_ENVIRONMENT_MAP
FfxFloat32x3 SampleEnvironmentMap(FfxFloat32x3 world_space_reflected_direction)
{
    return textureLod(samplerCube(g_environment_map, g_wrap_linear_sampler), world_space_reflected_direction, 0.0f).rgb * GetEnvironmentMapIntensity();
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_RADIANCE_CACHE
FfxFloat32x3 SampleRadianceCacheSRV(FfxFloat32x3 uvw)
{
    return textureLod(sampler3D(g_bctx_radiance_cache, g_clamp_nearest_sampler), uvw, 0.0f).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RADIANCE_CACHE
FfxFloat32x3 LoadRadianceCache(FfxUInt32x3 coord)
{
    return imageLoad(g_rw_bctx_radiance_cache, FfxInt32x3(coord)).rgb;
}

void StoreRadianceCache(FfxUInt32x3 coord, FfxFloat32x3 value)
{
    imageStore(g_rw_bctx_radiance_cache, FfxInt32x3(coord), FfxFloat32x4(value, 0.0f));
}
#endif

// GI Resources

#if defined BRIXELIZER_GI_BIND_CB_SDFGI
FfxBrixelizerGIConstants GetGIConstants()
{
    return g_sdfgi_constants.data;
}

FfxUInt32x2 GetBufferDimensions()
{
    return g_sdfgi_constants.data.buffer_dimensions;
}

FfxFloat32x2 GetBufferDimensionsF32()
{
    return g_sdfgi_constants.data.buffer_dimensions_f32;
}

FfxFloat32x2 GetProbeBufferDimensionsF32()
{
    return g_sdfgi_constants.data.probe_buffer_dimensions_f32;
}

FfxUInt32 GetFrameIndex()
{
    return g_sdfgi_constants.data.frame_index;
}

FfxUInt32x2 GetTileBufferDimensions()
{
    return g_sdfgi_constants.data.tile_buffer_dimensions;
}

FfxFloat32x3 GetCameraPosition()
{
    return g_sdfgi_constants.data.camera_position;
}

FfxFloat32x4x4 GetViewMatrix()
{
    return g_sdfgi_constants.data.view;
}

FfxFloat32x4x4 GetViewProjectionMatrix()
{
    return g_sdfgi_constants.data.view_proj;
}

FfxFloat32x4x4 GetInverseViewMatrix()
{
    return g_sdfgi_constants.data.inv_view;
}

FfxFloat32x4x4 GetInverseProjectionMatrix()
{
    return g_sdfgi_constants.data.inv_proj;
}

FfxFloat32x4x4 GetInverseViewProjectionMatrix()
{
    return g_sdfgi_constants.data.inv_view_proj;
}

FfxFloat32x4x4 GetPreviousViewProjectionMatrix()
{
    return g_sdfgi_constants.data.prev_view_proj;
}

FfxFloat32x4x4 GetPreviousInverseViewMatrix()
{
    return g_sdfgi_constants.data.prev_inv_view;
}

FfxFloat32x4x4 GetPreviousInverseProjectionMatrix()
{
    return g_sdfgi_constants.data.prev_inv_proj;
}

FfxFloat32 GetRoughnessThreshold()
{
    return g_sdfgi_constants.data.roughnessThreshold;
}

FfxUInt32 GetRoughnessChannel()
{
    return g_sdfgi_constants.data.roughnessChannel;
}

FfxFloat32 GetEnvironmentMapIntensity()
{
    return g_sdfgi_constants.data.environmentMapIntensity;
}

FfxUInt32 GetTracingConstantsStartCascade()
{
    return g_sdfgi_constants.data.tracing_constants.start_cascade;
}

FfxUInt32 GetTracingConstantsEndCascade()
{
    return g_sdfgi_constants.data.tracing_constants.end_cascade;
}

FfxFloat32 GetTracingConstantsRayPushoff()
{
    return g_sdfgi_constants.data.tracing_constants.ray_pushoff;
}

FfxFloat32 GetTracingConstantsTMin()
{
    return g_sdfgi_constants.data.tracing_constants.t_min;
}

FfxFloat32 GetTracingConstantsTMax()
{
    return g_sdfgi_constants.data.tracing_constants.t_max;
}

FfxFloat32 GetTracingConstantsSDFSolveEpsilon()
{
    return g_sdfgi_constants.data.tracing_constants.sdf_solve_eps;
}

FfxFloat32 GetTracingConstantsSpecularRayPushoff()
{
    return g_sdfgi_constants.data.tracing_constants.specular_ray_pushoff;
}

FfxFloat32 GetTracingConstantsSpecularSDFSolveEpsilon()
{
    return g_sdfgi_constants.data.tracing_constants.specular_sdf_solve_eps;
}
#endif

#if defined BRIXELIZER_GI_BIND_CB_PASS_CONSTANTS
FfxUInt32 GetPassConstantsCascadeIndex()
{
    return g_pass_constants.data.cascade_idx;
}

FfxFloat32 GetPassConstantsEnergyDecayK()
{
    return g_pass_constants.data.energy_decay_k;
}
#endif

#if defined BRIXELIZER_GI_BIND_CB_SCALING_CONSTANTS
FfxBrixelizerGIScalingConstants GetScalingConstants()
{
    return g_scaling_constants.data;
}

FfxUInt32 GetScalingRoughnessChannel()
{
    return g_scaling_constants.data.roughnessChannel;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPAWN_MASK
FfxUInt32 LoadTempSpawnMask(FfxUInt32x2 coord)
{
    return imageLoad(g_sdfgi_rw_temp_spawn_mask, FfxInt32x2(coord)).r;
}

void StoreTempSpawnMask(FfxUInt32x2 coord, FfxUInt32 value)
{
    imageStore(g_sdfgi_rw_temp_spawn_mask, FfxInt32x2(coord), FfxUInt32x4(value, 0, 0, 0));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_RAND_SEED
FfxUInt32 LoadTempRandomSeed(FfxUInt32x2 coord)
{
    return imageLoad(g_sdfgi_rw_temp_rand_seed, FfxInt32x2(coord)).r;
}

void StoreTempRandomSeed(FfxUInt32x2 coord, FfxUInt32 value)
{
    imageStore(g_sdfgi_rw_temp_rand_seed, FfxInt32x2(coord), FfxUInt32x4(value, 0, 0, 0));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_PRETRACE_TARGET
FfxUInt32x4 LoadTempSpecularPretraceTarget(FfxUInt32x2 coord)
{
    return imageLoad(g_sdfgi_rw_temp_specular_pretrace_target, FfxInt32x2(coord));
}

void StoreTempSpecularPretraceTarget(FfxUInt32x2 coord, FfxUInt32x4 value)
{
    imageStore(g_sdfgi_rw_temp_specular_pretrace_target, FfxInt32x2(coord), value);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES_STAT
FfxFloat32x4 LoadStaticScreenProbesStat(FfxUInt32x2 coord)
{
    return imageLoad(g_sdfgi_rw_static_screen_probes_stat, FfxInt32x2(coord));
}

void StoreStaticScreenProbesStat(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    imageStore(g_sdfgi_rw_static_screen_probes_stat, FfxInt32x2(coord), value);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SPECULAR_TARGET
FfxFloat32x4 SampleSpecularTargetSRV(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_sdfgi_r_specular_target, g_clamp_nearest_sampler), uv, 0.0f);
}

FfxFloat32x4 LoadSpecularTargetSRV(FfxUInt32x2 coord)
{
    return texelFetch(g_sdfgi_r_specular_target, FfxInt32x2(coord), 0);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_SPECULAR_TARGET
FfxFloat32x4 LoadSpecularTarget(FfxUInt32x2 coord)
{
    return imageLoad(g_sdfgi_rw_specular_target, FfxInt32x2(coord));
}

void StoreSpecularTarget(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    imageStore(g_sdfgi_rw_specular_target, FfxInt32x2(coord), value);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_INFO
FfxUInt32x4 LoadStaticProbeInfo(FfxUInt32 index)
{
    return g_sdfgi_rw_static_probe_info.data[index];
}

void StoreStaticProbeInfo(FfxUInt32 index, FfxUInt32x4 value)
{
    g_sdfgi_rw_static_probe_info.data[index] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_SH_BUFFER
FfxUInt32x2 LoadStaticProbeSHBuffer(FfxUInt32 index)
{
    return g_sdfgi_rw_static_probe_sh_buffer.data[index];
}

void StoreStaticProbeSHBuffer(FfxUInt32 index, FfxUInt32x2 value)
{
    g_sdfgi_rw_static_probe_sh_buffer.data[index] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_INFO
FfxUInt32x4 LoadTempProbeInfo(FfxUInt32 index)
{
    return g_sdfgi_rw_temp_probe_info.data[index];
}

void StoreTempProbeInfo(FfxUInt32 index, FfxUInt32x4 info)
{
    g_sdfgi_rw_temp_probe_info.data[index] = info;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_SH_BUFFER
FfxUInt32x2 LoadTempProbeSHBuffer(FfxUInt32 index)
{
    return g_sdfgi_rw_temp_probe_sh_buffer.data[index];
}

void StoreTempProbeSHBuffer(FfxUInt32 index, FfxUInt32x2 value)
{
    g_sdfgi_rw_temp_probe_sh_buffer.data[index] = value;

}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_PREV_LIT_OUTPUT
FfxFloat32x3 SamplePrevLitOutput(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_prev_lit_output, g_clamp_nearest_sampler), uv, FfxFloat32(0.0f)).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH
FfxFloat32 LoadDepth(FfxUInt32x2 pixel_coordinate)
{
    return texelFetch(g_depth, FfxInt32x2(pixel_coordinate), 0).x;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_ROUGHNESS
FfxFloat32 LoadRoughness(FfxUInt32x2 pixel_coordinate)
{
    FfxFloat32 roughness = texelFetch(g_roughness, FfxInt32x2(pixel_coordinate), 0)[GetRoughnessChannel()];

    if (g_sdfgi_constants.data.isRoughnessPerceptual == 1)
        roughness *= roughness;

    return roughness;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH
FfxFloat32 LoadPrevDepth(FfxUInt32x2 pixel_coordinate)
{
    return texelFetch(g_history_depth, FfxInt32x2(pixel_coordinate), 0).x;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH
FfxFloat32 SampleDepth(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_depth, g_clamp_nearest_sampler), uv, 0.0f).x;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH
FfxFloat32 SamplePrevDepth(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_history_depth, g_clamp_nearest_sampler), uv, 0.0f).x;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL
FfxFloat32x3 LoadWorldNormal(FfxUInt32x2 pixel_coordinate)
{
    return normalize(texelFetch(g_normal, FfxInt32x2(pixel_coordinate), 0).xyz * g_sdfgi_constants.data.normalsUnpackMul + g_sdfgi_constants.data.normalsUnpackAdd);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL
FfxFloat32x3 LoadPrevWorldNormal(FfxUInt32x2 pixel_coordinate)
{
    return normalize(texelFetch(g_history_normal, FfxInt32x2(pixel_coordinate), 0).xyz * g_sdfgi_constants.data.normalsUnpackMul + g_sdfgi_constants.data.normalsUnpackAdd);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL
FfxFloat32x3 SampleWorldNormal(FfxFloat32x2 uv)
{
    return normalize(textureLod(sampler2D(g_normal, g_clamp_nearest_sampler), uv, 0.0f).xyz * g_sdfgi_constants.data.normalsUnpackMul + g_sdfgi_constants.data.normalsUnpackAdd);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL
FfxFloat32x3 SamplePrevWorldNormal(FfxFloat32x2 uv)
{
    return normalize(textureLod(sampler2D(g_history_normal, g_clamp_nearest_sampler), uv, 0.0f).xyz * g_sdfgi_constants.data.normalsUnpackMul + g_sdfgi_constants.data.normalsUnpackAdd);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS
FfxFloat32x2 SampleMotionVector(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_motion_vectors, g_clamp_nearest_sampler), uv, 0.0f).xy * g_sdfgi_constants.data.motionVectorScale;
}

FfxFloat32x2 LoadMotionVector(FfxUInt32x2 pixel_coordinate)
{
    return texelFetch(g_motion_vectors, FfxInt32x2(pixel_coordinate), 0).xy * g_sdfgi_constants.data.motionVectorScale;
}
#endif // BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS

#if defined BRIXELIZER_GI_BIND_SRV_DISOCCLUSION_MASK
FfxUInt32 LoadDisocclusionMask(FfxUInt32x2 pixel_coordinate)
{
    return FfxUInt32(texelFetch(g_r_disocclusion_mask, FfxInt32x2(pixel_coordinate), 0).x);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DISOCCLUSION_MASK
void StoreDisocclusionMask(FfxUInt32x2 pixel_coordinate, FfxFloat32 value)
{
    imageStore(g_rw_disocclusion_mask, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 0.0f, 0.0f, 0.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS
FfxUInt32 LoadRaySwapIndirectArgs(FfxUInt32 elementIdx)
{
    return g_sdfgi_rw_ray_swap_indirect_args.data[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS
void StoreRaySwapIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value)
{
    g_sdfgi_rw_ray_swap_indirect_args.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS
void IncrementRaySwapIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue)
{
    originalValue = atomicAdd(g_sdfgi_rw_ray_swap_indirect_args.data[elementIdx], value);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_DIRECT_SH
FfxUInt32x2 LoadBricksDirectSH(FfxUInt32 elementIdx)
{
    return g_bctx_bricks_direct_sh.data[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_DIRECT_SH
void StoreBricksDirectSH(FfxUInt32 elementIdx, FfxUInt32x2 value)
{
    g_bctx_bricks_direct_sh.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH
FfxUInt32x2 LoadBricksSH(FfxUInt32 elementIdx)
{
    return g_bctx_bricks_sh.data[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH
void StoreBricksSH(FfxUInt32 elementIdx, FfxUInt32x2 value)
{
    g_bctx_bricks_sh.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH_STATE
FfxUInt32x4 LoadBricksSHState(FfxUInt32 elementIdx)
{
    return g_bctx_bricks_sh_state.data[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH_STATE
void StoreBricksSHState(FfxUInt32 elementIdx, FfxUInt32x4 value)
{
    g_bctx_bricks_sh_state.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_RAY_SWAP
FfxUInt32 LoadTempSpecularRaySwap(FfxUInt32 elementIdx)
{
    return g_sdfgi_rw_temp_specular_ray_swap.data[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_RAY_SWAP
void StoreTempSpecularRaySwap(FfxUInt32 elementIdx, FfxUInt32 value)
{
    g_sdfgi_rw_temp_specular_ray_swap.data[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_BLUE_NOISE
FfxFloat32x2 SampleBlueNoise(FfxUInt32x2 pixel, FfxUInt32 sample_index, FfxUInt32 dimension_offset)
{
    FfxInt32x2   coord = FfxInt32x2(pixel.x & 127u, pixel.y & 127u);
    FfxFloat32x2 xi    = texelFetch(g_blue_noise, FfxInt32x2(coord), 0).xy;
    // xi.x         = frac(xi.x + FfxFloat32((pixel.x >> 7) & 0xFFu) * GOLDEN_RATIO);
    // xi.y         = frac(xi.y + FfxFloat32((pixel.y >> 7) & 0xFFu) * GOLDEN_RATIO);
    // return fmod(xi + (sample_index & 255) * GOLDEN_RATIO, 1.0f);
    return xi;
}
#endif

FfxFloat32x2 SampleBlueNoise(in FfxUInt32x2 pixel, in FfxUInt32 sample_index)
{
    return SampleBlueNoise(pixel, sample_index, 0);
}

#if defined BRIXELIZER_GI_BIND_SRV_STATIC_SCREEN_PROBES
FfxFloat32x4 LoadStaticScreenProbesSRV(FfxUInt32x2 coord)
{
    return texelFetch(g_sdfgi_r_static_screen_probes, FfxInt32x2(coord), 0);
}

FfxFloat32x4 SampleStaticScreenProbesSRV(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_sdfgi_r_static_screen_probes, g_clamp_nearest_sampler), uv, 0.0f);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES
FfxFloat32x4 LoadStaticScreenProbes(FfxUInt32x2 coord)
{
    return imageLoad(g_sdfgi_rw_static_screen_probes, FfxInt32x2(coord));
}

void StoreStaticScreenProbes(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    imageStore(g_sdfgi_rw_static_screen_probes, FfxInt32x2(coord), value);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_STATIC_GI_TARGET
FfxFloat32x4 SampleStaticGITargetSRV(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_sdfgi_r_static_gitarget, g_clamp_nearest_sampler), uv, 0.0f);
}

FfxFloat32x4 LoadStaticGITargetSRV(FfxUInt32x2 coord)
{
    return texelFetch(g_sdfgi_r_static_gitarget, FfxInt32x2(coord), 0);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_GI_TARGET
void StoreStaticGITarget(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    imageStore(g_sdfgi_rw_static_gitarget, FfxInt32x2(coord), value);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DEBUG_TARGET
FfxFloat32x4 LoadDebugTraget(FfxUInt32x2 coord)
{
    return imageLoad(g_sdfgi_rw_debug_target, FfxInt32x2(coord));
}

void StoreDebugTarget(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    imageStore(g_sdfgi_rw_debug_target, FfxInt32x2(coord), value);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DEBUG_VISUALIZATION
void StoreDebugVisualization(FfxUInt32x2 pixel_coordinate, FfxFloat32x4 value)
{
    imageStore(g_rw_debug_visualization, FfxInt32x2(pixel_coordinate), value);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_DEPTH
FfxFloat32 LoadSourceDepth(FfxUInt32x2 coord)
{
    return texelFetch(g_src_depth, FfxInt32x2(coord), 0).r;
}

FfxFloat32x4 GatherSourceDepth(FfxFloat32x2 uv)
{
    return textureGather(sampler2D(g_src_depth, g_clamp_nearest_sampler), uv, 0);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_DEPTH
FfxFloat32x4 GatherSourcePrevDepth(FfxFloat32x2 uv)
{
    return textureGather(sampler2D(g_src_history_depth, g_clamp_nearest_sampler), uv, 0);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_NORMAL
FfxFloat32x3 LoadSourceNormal(FfxUInt32x2 coord)
{
    return normalize(texelFetch(g_src_normal, FfxInt32x2(coord), 0).xyz * g_sdfgi_constants.data.normalsUnpackMul + g_sdfgi_constants.data.normalsUnpackAdd);
}

FfxFloat32x3 SampleSourceNormal(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_src_normal, g_clamp_nearest_sampler), uv, 0.0f).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_NORMAL
FfxFloat32x3 SampleSourcePrevNormal(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_src_history_normal, g_clamp_nearest_sampler), uv, 0.0f).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_ROUGHNESS
FfxFloat32 SampleSourceRoughness(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_src_roughness, g_clamp_nearest_sampler), uv, 0.0f)[GetScalingRoughnessChannel()];
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_MOTION_VECTORS
FfxFloat32x2 SampleSourceMotionVector(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_src_motion_vectors, g_clamp_nearest_sampler), uv, 0.0f).xy;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_PREV_LIT_OUTPUT
FfxFloat32x3 SampleSourcePrevLitOutput(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_src_prev_lit_output, g_clamp_nearest_sampler), uv, 0.0f).xyz;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_DIFFUSE_GI
FfxFloat32x3 SampleDownsampledDiffuseGI(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_downsampled_diffuse_gi, g_clamp_nearest_sampler), uv, 0.0f).xyz;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_SPECULAR_GI
FfxFloat32x3 SampleDownsampledSpecularGI(FfxFloat32x2 uv)
{
    return textureLod(sampler2D(g_downsampled_specular_gi, g_clamp_nearest_sampler), uv, 0.0f).xyz;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_DEPTH
void StoreDownsampledDepth(FfxUInt32x2 pixel_coordinate, FfxFloat32 value)
{
    imageStore(g_downsampled_depth, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 0.0f, 0.0f, 0.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_DEPTH
void StoreDownsampledPrevDepth(FfxUInt32x2 pixel_coordinate, FfxFloat32 value)
{
    imageStore(g_downsampled_history_depth, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 0.0f, 0.0f, 0.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_NORMAL
void StoreDownsampledNormal(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    imageStore(g_downsampled_normal, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 0.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_NORMAL
void StoreDownsampledPrevNormal(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    imageStore(g_downsampled_history_normal, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 0.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_ROUGHNESS
void StoreDownsampledRoughness(FfxUInt32x2 pixel_coordinate, FfxFloat32 value)
{
    imageStore(g_downsampled_roughness, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 0.0f, 0.0f, 0.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_MOTION_VECTORS
void StoreDownsampledMotionVector(FfxUInt32x2 pixel_coordinate, FfxFloat32x2 value)
{
    imageStore(g_downsampled_motion_vectors, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 0.0f, 0.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_PREV_LIT_OUTPUT
void StoreDownsampledPrevLitOutput(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    imageStore(g_downsampled_prev_lit_output, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 0.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_UPSAMPLED_DIFFUSE_GI
void StoreUpsampledDiffuseGI(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    imageStore(g_upsampled_diffuse_gi, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 1.0f));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_UPSAMPLED_SPECULAR_GI
void StoreUpsampledSpecularGI(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    imageStore(g_upsampled_specular_gi, FfxInt32x2(pixel_coordinate), FfxFloat32x4(value, 1.0f));
}
#endif

#endif // #if defined(FFX_GPU)

#endif // FFX_BRIXELIZER_GI_CALLBACKS_GLSL_H
