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

#ifndef  FFX_BRIXELIZER_GI_CALLBACKS_HLSL_H
#define  FFX_BRIXELIZER_GI_CALLBACKS_HLSL_H

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

#define DECLARE_SRV_REGISTER(regIndex)      t##regIndex
#define DECLARE_UAV_REGISTER(regIndex)      u##regIndex
#define DECLARE_CB_REGISTER(regIndex)       b##regIndex
#define DECLARE_SAMPLER_REGISTER(regIndex)  s##regIndex
#define FFX_BRIXELIZER_DECLARE_SRV(regIndex)        register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_BRIXELIZER_DECLARE_UAV(regIndex)        register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_BRIXELIZER_DECLARE_CB(regIndex)         register(DECLARE_CB_REGISTER(regIndex))
#define FFX_BRIXELIZER_DECLARE_SAMPLER(regIndex)    register(DECLARE_SAMPLER_REGISTER(regIndex))

// CBVs

#if defined BRIXELIZER_GI_BIND_CB_SDFGI
    ConstantBuffer<FfxBrixelizerGIConstants>         g_sdfgi_constants                        : FFX_BRIXELIZER_DECLARE_CB(BRIXELIZER_GI_BIND_CB_SDFGI);
#endif

#if defined BRIXELIZER_GI_BIND_CB_PASS_CONSTANTS
    ConstantBuffer<FfxBrixelizerGIPassConstants>       g_pass_constants                         : FFX_BRIXELIZER_DECLARE_CB(BRIXELIZER_GI_BIND_CB_PASS_CONSTANTS);
#endif

#if defined BRIXELIZER_GI_BIND_CB_SCALING_CONSTANTS
    ConstantBuffer<FfxBrixelizerGIScalingConstants>    g_scaling_constants                      : FFX_BRIXELIZER_DECLARE_CB(BRIXELIZER_GI_BIND_CB_SCALING_CONSTANTS);
#endif

// SRVs 

#if defined BRIXELIZER_GI_BIND_SRV_DISOCCLUSION_MASK
    Texture2D<FfxUInt32>                g_r_disocclusion_mask                    : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_DISOCCLUSION_MASK);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_STATIC_GI_TARGET
    Texture2D<FfxFloat32x4>             g_sdfgi_r_static_gitarget                : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_STATIC_GI_TARGET);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_STATIC_SCREEN_PROBES
    Texture2D<FfxFloat32x4>             g_sdfgi_r_static_screen_probes           : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_STATIC_SCREEN_PROBES);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SPECULAR_TARGET
    Texture2D<FfxFloat32x4>             g_sdfgi_r_specular_target                : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_SPECULAR_TARGET);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_RADIANCE_CACHE
    Texture3D<FfxFloat32x3>             g_bctx_radiance_cache                    : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_RADIANCE_CACHE);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_PREV_LIT_OUTPUT
    Texture2D<FfxFloat32x4>             g_prev_lit_output                        : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_PREV_LIT_OUTPUT);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH
    Texture2D<FfxFloat32x4>             g_depth                                  : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH
    Texture2D<FfxFloat32x4>             g_history_depth                          : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL
    Texture2D<FfxFloat32x4>             g_normal                                 : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL
    Texture2D<FfxFloat32x4>             g_history_normal                         : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_ROUGHNESS
    Texture2D<FfxFloat32x4>             g_roughness                              : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_ROUGHNESS);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS
    Texture2D<FfxFloat32x4>             g_motion_vectors                         : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_ENVIRONMENT_MAP
    TextureCube<FfxFloat32x3>           g_environment_map                        : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_ENVIRONMENT_MAP);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_BLUE_NOISE
    Texture2D<FfxFloat32x2>             g_blue_noise                             : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_INPUT_BLUE_NOISE);
#endif

// Downsampled SRVs

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_PREV_LIT_OUTPUT
    Texture2D<FfxFloat32x4>             g_src_prev_lit_output                    : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_SOURCE_PREV_LIT_OUTPUT);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_DEPTH
    Texture2D<FfxFloat32x4>             g_src_depth                              : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_SOURCE_DEPTH);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_DEPTH
    Texture2D<FfxFloat32x4>             g_src_history_depth                      : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_DEPTH);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_NORMAL
    Texture2D<FfxFloat32x4>             g_src_normal                             : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_SOURCE_NORMAL);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_NORMAL
    Texture2D<FfxFloat32x4>             g_src_history_normal                     : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_NORMAL);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_ROUGHNESS
    Texture2D<FfxFloat32x4>             g_src_roughness                          : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_SOURCE_ROUGHNESS);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_MOTION_VECTORS
    Texture2D<FfxFloat32x4>             g_src_motion_vectors                     : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_SOURCE_MOTION_VECTORS);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_DIFFUSE_GI
    Texture2D<FfxFloat32x4>             g_downsampled_diffuse_gi                 : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_DIFFUSE_GI);
#endif

#if defined BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_SPECULAR_GI
    Texture2D<FfxFloat32x4>             g_downsampled_specular_gi                : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_SPECULAR_GI);
#endif


// UAVs 

#if defined BRIXELIZER_GI_BIND_UAV_DISOCCLUSION_MASK
    RWTexture2D<FfxFloat32>             g_rw_disocclusion_mask                   : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DISOCCLUSION_MASK);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES
    RWTexture2D<FfxFloat32x4>           g_sdfgi_rw_static_screen_probes          : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PUSH_OFF_MAP
    RWTexture2D<FfxFloat32x3>           g_sdfgi_rw_static_pushoff_map            : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_STATIC_PUSH_OFF_MAP);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_GI_TARGET
    RWTexture2D<FfxFloat32x4>           g_sdfgi_rw_static_gitarget               : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_STATIC_GI_TARGET);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DEBUG_TARGET
    RWTexture2D<FfxFloat32x4>           g_sdfgi_rw_debug_target                  : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DEBUG_TARGET);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RADIANCE_CACHE
    RWTexture3D<FfxFloat32x3>           g_rw_bctx_radiance_cache                 : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_RADIANCE_CACHE);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPAWN_MASK
    RWTexture2D<FfxUInt32>              g_sdfgi_rw_temp_spawn_mask               : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_TEMP_SPAWN_MASK);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_RAND_SEED
    RWTexture2D<FfxUInt32>              g_sdfgi_rw_temp_rand_seed                : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_TEMP_RAND_SEED);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_PRETRACE_TARGET
    RWTexture2D<FfxUInt32x4>            g_sdfgi_rw_temp_specular_pretrace_target : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_PRETRACE_TARGET);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_BLUR_MASK
    RWTexture2D<FfxFloat32>             g_sdfgi_rw_temp_blur_mask                : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_TEMP_BLUR_MASK);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES_STAT
    RWTexture2D<FfxFloat32x4>           g_sdfgi_rw_static_screen_probes_stat     : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES_STAT);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_SPECULAR_TARGET
    RWTexture2D<FfxFloat32x4>           g_sdfgi_rw_specular_target               : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_SPECULAR_TARGET);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_SCREEN_PROBES_OUTPUT_COPY
    RWTexture2D<FfxFloat32x4>           g_rw_screen_probes_output_copy           : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_SCREEN_PROBES_OUTPUT_COPY);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DEBUG_VISUALIZATION
    RWTexture2D<FfxFloat32x4>           g_rw_debug_visualization                 : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DEBUG_VISUALIZATION);
#endif

// Downsampled resources

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_PREV_LIT_OUTPUT
    RWTexture2D<FfxFloat32x4>           g_downsampled_prev_lit_output            : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_PREV_LIT_OUTPUT);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_DEPTH
    RWTexture2D<FfxFloat32>             g_downsampled_depth                      : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_DEPTH);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_DEPTH
    RWTexture2D<FfxFloat32>             g_downsampled_history_depth              : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_DEPTH);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_NORMAL
    RWTexture2D<FfxFloat32x4>           g_downsampled_normal                     : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_NORMAL);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_NORMAL
    RWTexture2D<FfxFloat32x4>           g_downsampled_history_normal             : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_NORMAL);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_ROUGHNESS
    RWTexture2D<FfxFloat32>             g_downsampled_roughness                  : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_ROUGHNESS);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_MOTION_VECTORS
    RWTexture2D<FfxFloat32x4>           g_downsampled_motion_vectors             : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_MOTION_VECTORS);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_UPSAMPLED_DIFFUSE_GI
    RWTexture2D<FfxFloat32x4>           g_upsampled_diffuse_gi                   : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_UPSAMPLED_DIFFUSE_GI);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_UPSAMPLED_SPECULAR_GI
    RWTexture2D<FfxFloat32x4>           g_upsampled_specular_gi                  : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_UPSAMPLED_SPECULAR_GI);
#endif

// UAVs 

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_INFO
    RWStructuredBuffer<FfxUInt32x4>     g_sdfgi_rw_static_probe_info             : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_INFO);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_SH_BUFFER
    RWStructuredBuffer<FfxUInt32x2>     g_sdfgi_rw_static_probe_sh_buffer        : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_SH_BUFFER);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_INFO
    RWStructuredBuffer<FfxUInt32x4>     g_sdfgi_rw_temp_probe_info               : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_INFO);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_SH_BUFFER
    RWStructuredBuffer<FfxUInt32x2>     g_sdfgi_rw_temp_probe_sh_buffer          : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_SH_BUFFER);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS
    RWStructuredBuffer<FfxUInt32>       g_sdfgi_rw_ray_swap_indirect_args        : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_DIRECT_SH
    RWStructuredBuffer<FfxUInt32x2>     g_bctx_bricks_direct_sh                  : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_BRICKS_DIRECT_SH);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH
    RWStructuredBuffer<FfxUInt32x2>     g_bctx_bricks_sh                         : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_BRICKS_SH);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH_STATE
    RWStructuredBuffer<FfxUInt32x4>     g_bctx_bricks_sh_state                   : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_BRICKS_SH_STATE);
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_RAY_SWAP
    RWStructuredBuffer<FfxUInt32>       g_sdfgi_rw_temp_specular_ray_swap        : FFX_BRIXELIZER_DECLARE_UAV(BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_RAY_SWAP);
#endif

// Brixelizer Resources

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
    ConstantBuffer<FfxBrixelizerContextInfo>   g_bx_context_info : FFX_BRIXELIZER_DECLARE_CB(BRIXELIZER_BIND_CB_CONTEXT_INFO);
#endif

#if defined BRIXELIZER_BIND_SRV_SDF_ATLAS
    Texture3D<FfxFloat32>              r_sdf_atlas : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_SDF_ATLAS);
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_AABB
    StructuredBuffer<FfxUInt32>        r_bctx_bricks_aabb : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_AABB);
#endif 

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_VOXEL_MAP
    StructuredBuffer<FfxUInt32>        r_bctx_bricks_voxel_map : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_VOXEL_MAP);
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_COUNTERS
    StructuredBuffer<FfxUInt32>       r_bctx_counters : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_CONTEXT_COUNTERS);
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_CLEAR_LIST
    StructuredBuffer<FfxUInt32>       r_bctx_bricks_clear_list : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_CLEAR_LIST);
#endif

#if defined BRIXELIZER_BIND_SRV_CASCADE_AABB_TREES
    StructuredBuffer<FfxUInt32>        r_cascade_aabbtrees[24] : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_CASCADE_AABB_TREES);
#endif

#if defined BRIXELIZER_BIND_SRV_CASCADE_BRICK_MAPS
    StructuredBuffer<FfxUInt32>        r_cascade_brick_maps[24] : FFX_BRIXELIZER_DECLARE_SRV(BRIXELIZER_BIND_SRV_CASCADE_BRICK_MAPS);
#endif 

#if defined BRIXELIZER_BIND_SAMPLER_CLAMP_LINEAR_SAMPLER
    SamplerState g_clamp_linear_sampler   : FFX_BRIXELIZER_DECLARE_SAMPLER(BRIXELIZER_BIND_SAMPLER_CLAMP_LINEAR_SAMPLER);
#endif

#if defined BRIXELIZER_BIND_SAMPLER_CLAMP_NEAREST_SAMPLER
    SamplerState g_clamp_nearest_sampler   : FFX_BRIXELIZER_DECLARE_SAMPLER(BRIXELIZER_BIND_SAMPLER_CLAMP_NEAREST_SAMPLER);
#endif

#if defined BRIXELIZER_BIND_SAMPLER_WRAP_LINEAR_SAMPLER
    SamplerState g_wrap_linear_sampler    : FFX_BRIXELIZER_DECLARE_SAMPLER(BRIXELIZER_BIND_SAMPLER_WRAP_LINEAR_SAMPLER);
#endif

#if defined BRIXELIZER_BIND_SAMPLER_WRAP_NEAREST_SAMPLER
    SamplerState g_wrap_nearest_sampler   : FFX_BRIXELIZER_DECLARE_SAMPLER(BRIXELIZER_BIND_SAMPLER_WRAP_NEAREST_SAMPLER);
#endif

#if defined BRIXELIZER_BIND_CB_CONTEXT_INFO
FfxBrixelizerContextInfo GetContextInfo()
{
    return g_bx_context_info;
}

FfxBrixelizerCascadeInfo GetCascadeInfo(FfxUInt32 cascadeID)
{
    return g_bx_context_info.cascades[cascadeID];
}

FfxBrixelizerCascadeInfo GetCascadeInfoNonUniform(FfxUInt32 cascadeID)
{
    return g_bx_context_info.cascades[NonUniformResourceIndex(cascadeID)];
}
#endif

#if defined BRIXELIZER_BIND_SRV_SDF_ATLAS
FfxFloat32 SampleSDFAtlas(FfxFloat32x3 uvw)
{
    return r_sdf_atlas.SampleLevel(g_wrap_linear_sampler, uvw, 0.0f);
}
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_COUNTERS
FfxUInt32 LoadContextCounter(FfxUInt32 elementIdx)
{
    return r_bctx_counters[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_CLEAR_LIST
FfxUInt32 LoadBricksClearList(FfxUInt32 elementIdx)
{
    return r_bctx_bricks_clear_list[elementIdx];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CASCADE_AABB_TREES
FfxFloat32x3 LoadCascadeAABBTreesFloat3(FfxUInt32 cascadeID, FfxUInt32 elementIndex)
{ 
    return FfxFloat32x3(asfloat(r_cascade_aabbtrees[cascadeID][elementIndex + 0]),
                        asfloat(r_cascade_aabbtrees[cascadeID][elementIndex + 1]),
                        asfloat(r_cascade_aabbtrees[cascadeID][elementIndex + 2]));
}

FfxUInt32 LoadCascadeAABBTreesUInt(FfxUInt32 cascadeID, FfxUInt32 elementIndex)
{
	return r_cascade_aabbtrees[cascadeID][elementIndex];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_VOXEL_MAP
FfxUInt32 LoadBricksVoxelMap(FfxUInt32 elementIndex)
{
    return r_bctx_bricks_voxel_map[elementIndex];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CONTEXT_BRICKS_AABB
FfxUInt32 LoadBricksAABB(FfxUInt32 elementIndex)
{
    return r_bctx_bricks_aabb[elementIndex];
}
#endif

#if defined BRIXELIZER_BIND_SRV_CASCADE_BRICK_MAPS
FfxUInt32 LoadCascadeBrickMapArrayUniform(FfxUInt32 cascadeID, FfxUInt32 elementIndex)
{
    return r_cascade_brick_maps[cascadeID][elementIndex];
}

FfxUInt32 LoadCascadeBrickMapArrayNonUniform(FfxUInt32 cascadeID, FfxUInt32 elementIndex)
{
    return r_cascade_brick_maps[NonUniformResourceIndex(cascadeID)][elementIndex];
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_ENVIRONMENT_MAP
FfxFloat32x3 SampleEnvironmentMap(FfxFloat32x3 world_space_reflected_direction)
{
    return g_environment_map.SampleLevel(g_wrap_linear_sampler, world_space_reflected_direction, 0.0).xyz * GetEnvironmentMapIntensity();
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_RADIANCE_CACHE
FfxFloat32x3 SampleRadianceCacheSRV(FfxFloat32x3 uvw)
{
    return g_bctx_radiance_cache.SampleLevel(g_clamp_nearest_sampler, uvw, 0.0f);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RADIANCE_CACHE
FfxFloat32x3 LoadRadianceCache(FfxUInt32x3 coord)
{
    return g_rw_bctx_radiance_cache[coord];
}

void StoreRadianceCache(FfxUInt32x3 coord, FfxFloat32x3 value)
{
    g_rw_bctx_radiance_cache[coord] = value;
}
#endif

// GI Resources

#if defined BRIXELIZER_GI_BIND_CB_SDFGI
FfxBrixelizerGIConstants GetGIConstants()
{
    return g_sdfgi_constants;
}

FfxUInt32x2 GetBufferDimensions()
{
    return g_sdfgi_constants.buffer_dimensions;
}

FfxFloat32x2 GetBufferDimensionsF32()
{
    return g_sdfgi_constants.buffer_dimensions_f32;
}

FfxFloat32x2 GetProbeBufferDimensionsF32()
{
    return g_sdfgi_constants.probe_buffer_dimensions_f32;
}

FfxUInt32 GetFrameIndex()
{
    return g_sdfgi_constants.frame_index;
}

FfxUInt32x2 GetTileBufferDimensions()
{
    return g_sdfgi_constants.tile_buffer_dimensions;
}

FfxFloat32x3 GetCameraPosition()
{
    return g_sdfgi_constants.camera_position;
}

FfxFloat32x4x4 GetViewMatrix()
{
    return g_sdfgi_constants.view;
}

FfxFloat32x4x4 GetViewProjectionMatrix()
{
    return g_sdfgi_constants.view_proj;
}

FfxFloat32x4x4 GetInverseViewMatrix()
{
    return g_sdfgi_constants.inv_view;
}

FfxFloat32x4x4 GetInverseProjectionMatrix()
{
    return g_sdfgi_constants.inv_proj;
}

FfxFloat32x4x4 GetInverseViewProjectionMatrix()
{
    return g_sdfgi_constants.inv_view_proj;
}

FfxFloat32x4x4 GetPreviousViewProjectionMatrix()
{
    return g_sdfgi_constants.prev_view_proj;
}

FfxFloat32x4x4 GetPreviousInverseViewMatrix()
{
    return g_sdfgi_constants.prev_inv_view;
}

FfxFloat32x4x4 GetPreviousInverseProjectionMatrix()
{
    return g_sdfgi_constants.prev_inv_proj;
}

FfxFloat32 GetRoughnessThreshold()
{
    return g_sdfgi_constants.roughnessThreshold;
}

FfxUInt32 GetRoughnessChannel()
{
    return g_sdfgi_constants.roughnessChannel;
}

FfxFloat32 GetEnvironmentMapIntensity()
{
    return g_sdfgi_constants.environmentMapIntensity;
}

FfxUInt32 GetTracingConstantsStartCascade()
{
    return g_sdfgi_constants.tracing_constants.start_cascade;
}

FfxUInt32 GetTracingConstantsEndCascade()
{
    return g_sdfgi_constants.tracing_constants.end_cascade;
}

FfxFloat32 GetTracingConstantsRayPushoff()
{
    return g_sdfgi_constants.tracing_constants.ray_pushoff;
}

FfxFloat32 GetTracingConstantsTMin()
{
    return g_sdfgi_constants.tracing_constants.t_min;
}

FfxFloat32 GetTracingConstantsTMax()
{
    return g_sdfgi_constants.tracing_constants.t_max;
}

FfxFloat32 GetTracingConstantsSDFSolveEpsilon()
{
    return g_sdfgi_constants.tracing_constants.sdf_solve_eps;
}

FfxFloat32 GetTracingConstantsSpecularRayPushoff()
{
    return g_sdfgi_constants.tracing_constants.specular_ray_pushoff;
}

FfxFloat32 GetTracingConstantsSpecularSDFSolveEpsilon()
{
    return g_sdfgi_constants.tracing_constants.specular_sdf_solve_eps;
}
#endif

#if defined BRIXELIZER_GI_BIND_CB_PASS_CONSTANTS
FfxUInt32 GetPassConstantsCascadeIndex()
{
    return g_pass_constants.cascade_idx;
}

FfxFloat32 GetPassConstantsEnergyDecayK()
{
    return g_pass_constants.energy_decay_k;
}
#endif

#if defined BRIXELIZER_GI_BIND_CB_SCALING_CONSTANTS
FfxBrixelizerGIScalingConstants GetScalingConstants()
{
    return g_scaling_constants;
}

FfxUInt32 GetScalingRoughnessChannel()
{
    return g_scaling_constants.roughnessChannel;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPAWN_MASK
FfxUInt32 LoadTempSpawnMask(FfxUInt32x2 coord)
{
    return g_sdfgi_rw_temp_spawn_mask[coord];
}

void StoreTempSpawnMask(FfxUInt32x2 coord, FfxUInt32 value)
{
    g_sdfgi_rw_temp_spawn_mask[coord] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_RAND_SEED
FfxUInt32 LoadTempRandomSeed(FfxUInt32x2 coord)
{
    return g_sdfgi_rw_temp_rand_seed[coord];
}

void StoreTempRandomSeed(FfxUInt32x2 coord, FfxUInt32 value)
{
    g_sdfgi_rw_temp_rand_seed[coord] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_PRETRACE_TARGET
FfxUInt32x4 LoadTempSpecularPretraceTarget(FfxUInt32x2 coord)
{
    return g_sdfgi_rw_temp_specular_pretrace_target[coord];
}

void StoreTempSpecularPretraceTarget(FfxUInt32x2 coord, FfxUInt32x4 value)
{
    g_sdfgi_rw_temp_specular_pretrace_target[coord] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES_STAT
FfxFloat32x4 LoadStaticScreenProbesStat(FfxUInt32x2 coord)
{
    return g_sdfgi_rw_static_screen_probes_stat[coord];
}

void StoreStaticScreenProbesStat(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    g_sdfgi_rw_static_screen_probes_stat[coord] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SPECULAR_TARGET
FfxFloat32x4 SampleSpecularTargetSRV(FfxFloat32x2 uv)
{
    return g_sdfgi_r_specular_target.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f);
}

FfxFloat32x4 LoadSpecularTargetSRV(FfxUInt32x2 coord)
{
    return g_sdfgi_r_specular_target[coord];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_SPECULAR_TARGET
FfxFloat32x4 LoadSpecularTarget(FfxUInt32x2 coord)
{
    return g_sdfgi_rw_specular_target[coord];
}

void StoreSpecularTarget(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    g_sdfgi_rw_specular_target[coord] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_INFO
FfxUInt32x4 LoadStaticProbeInfo(FfxUInt32 index)
{
    return g_sdfgi_rw_static_probe_info[index];
}

void StoreStaticProbeInfo(FfxUInt32 index, FfxUInt32x4 value)
{
    g_sdfgi_rw_static_probe_info[index] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_PROBE_SH_BUFFER
FfxUInt32x2 LoadStaticProbeSHBuffer(FfxUInt32 index)
{
    return g_sdfgi_rw_static_probe_sh_buffer[index];
}

void StoreStaticProbeSHBuffer(FfxUInt32 index, FfxUInt32x2 value)
{
    g_sdfgi_rw_static_probe_sh_buffer[index] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_INFO
FfxUInt32x4 LoadTempProbeInfo(FfxUInt32 index)
{
    return g_sdfgi_rw_temp_probe_info[index];
}

void StoreTempProbeInfo(FfxUInt32 index, FfxUInt32x4 info)
{
    g_sdfgi_rw_temp_probe_info[index] = info;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_PROBE_SH_BUFFER
FfxUInt32x2 LoadTempProbeSHBuffer(FfxUInt32 index)
{
    return g_sdfgi_rw_temp_probe_sh_buffer[index];
}

void StoreTempProbeSHBuffer(FfxUInt32 index, FfxUInt32x2 value)
{
    g_sdfgi_rw_temp_probe_sh_buffer[index] = value;

}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_PREV_LIT_OUTPUT
FfxFloat32x3 SamplePrevLitOutput(FfxFloat32x2 uv)
{
    return g_prev_lit_output.SampleLevel(g_clamp_nearest_sampler, uv, 0.0).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH
FfxFloat32 LoadDepth(FfxUInt32x2 pixel_coordinate)
{
    return g_depth[pixel_coordinate].x;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_ROUGHNESS
FfxFloat32 LoadRoughness(FfxUInt32x2 pixel_coordinate)
{
    FfxFloat32 roughness = g_roughness[pixel_coordinate][GetRoughnessChannel()];

    if (g_sdfgi_constants.isRoughnessPerceptual == 1)
        roughness *= roughness;

    return roughness;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH
FfxFloat32 LoadPrevDepth(FfxUInt32x2 pixel_coordinate)
{
    return g_history_depth[pixel_coordinate].x;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_DEPTH
FfxFloat32 SampleDepth(FfxFloat32x2 uv)
{
    return g_depth.SampleLevel(g_clamp_nearest_sampler, uv, 0.0).x;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_DEPTH
FfxFloat32 SamplePrevDepth(FfxFloat32x2 uv)
{
    return g_history_depth.SampleLevel(g_clamp_nearest_sampler, uv, 0.0).x;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL
FfxFloat32x3 LoadWorldNormal(FfxUInt32x2 pixel_coordinate)
{
    return normalize(g_normal.Load(FfxInt32x3(pixel_coordinate, 0)).xyz * g_sdfgi_constants.normalsUnpackMul + g_sdfgi_constants.normalsUnpackAdd);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL
FfxFloat32x3 LoadPrevWorldNormal(FfxUInt32x2 pixel_coordinate)
{
    return normalize(g_history_normal.Load(FfxInt32x3(pixel_coordinate, 0)).xyz * g_sdfgi_constants.normalsUnpackMul + g_sdfgi_constants.normalsUnpackAdd);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_NORMAL
FfxFloat32x3 SampleWorldNormal(FfxFloat32x2 uv)
{
    return normalize(g_normal.SampleLevel(g_clamp_nearest_sampler, uv, 0.0).xyz * g_sdfgi_constants.normalsUnpackMul + g_sdfgi_constants.normalsUnpackAdd);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_HISTORY_NORMAL
FfxFloat32x3 SamplePrevWorldNormal(FfxFloat32x2 uv)
{
    return normalize(g_history_normal.SampleLevel(g_clamp_nearest_sampler, uv, 0.0).xyz * g_sdfgi_constants.normalsUnpackMul + g_sdfgi_constants.normalsUnpackAdd);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS
FfxFloat32x2 SampleMotionVector(FfxFloat32x2 uv)
{
    return g_motion_vectors.SampleLevel(g_clamp_nearest_sampler, uv, 0.0).xy * g_sdfgi_constants.motionVectorScale;
}

FfxFloat32x2 LoadMotionVector(FfxUInt32x2 pixel_coordinate)
{
    return g_motion_vectors[pixel_coordinate].xy * g_sdfgi_constants.motionVectorScale;
}
#endif // BRIXELIZER_GI_BIND_SRV_INPUT_MOTION_VECTORS

#if defined BRIXELIZER_GI_BIND_SRV_DISOCCLUSION_MASK
FfxUInt32 LoadDisocclusionMask(FfxUInt32x2 pixel_coordinate)
{
    return g_r_disocclusion_mask[pixel_coordinate].x;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DISOCCLUSION_MASK
void StoreDisocclusionMask(FfxUInt32x2 pixel_coordinate, float value)
{
    g_rw_disocclusion_mask[pixel_coordinate] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS
FfxUInt32 LoadRaySwapIndirectArgs(FfxUInt32 elementIdx)
{
    return g_sdfgi_rw_ray_swap_indirect_args[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS
void StoreRaySwapIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value)
{
    g_sdfgi_rw_ray_swap_indirect_args[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_RAY_SWAP_INDIRECT_ARGS
void IncrementRaySwapIndirectArgs(FfxUInt32 elementIdx, FfxUInt32 value, inout FfxUInt32 originalValue)
{
    InterlockedAdd(g_sdfgi_rw_ray_swap_indirect_args[elementIdx], value, originalValue);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_DIRECT_SH
FfxUInt32x2 LoadBricksDirectSH(FfxUInt32 elementIdx)
{
    return g_bctx_bricks_direct_sh[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_DIRECT_SH
void StoreBricksDirectSH(FfxUInt32 elementIdx, FfxUInt32x2 value)
{
    g_bctx_bricks_direct_sh[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH
FfxUInt32x2 LoadBricksSH(FfxUInt32 elementIdx)
{
    return g_bctx_bricks_sh[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH
void StoreBricksSH(FfxUInt32 elementIdx, FfxUInt32x2 value)
{
    g_bctx_bricks_sh[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH_STATE
FfxUInt32x4 LoadBricksSHState(FfxUInt32 elementIdx)
{
    return g_bctx_bricks_sh_state[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_BRICKS_SH_STATE
void StoreBricksSHState(FfxUInt32 elementIdx, FfxUInt32x4 value)
{
    g_bctx_bricks_sh_state[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_RAY_SWAP
FfxUInt32 LoadTempSpecularRaySwap(FfxUInt32 elementIdx)
{
    return g_sdfgi_rw_temp_specular_ray_swap[elementIdx];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_TEMP_SPECULAR_RAY_SWAP
void StoreTempSpecularRaySwap(FfxUInt32 elementIdx, FfxUInt32 value)
{
    g_sdfgi_rw_temp_specular_ray_swap[elementIdx] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_INPUT_BLUE_NOISE
FfxFloat32x2 SampleBlueNoise(FfxUInt32x2 pixel, FfxUInt32 sample_index, FfxUInt32 dimension_offset)
{
    FfxInt32x2   coord = FfxInt32x2(pixel.x & 127u, pixel.y & 127u);
    FfxFloat32x2 xi    = g_blue_noise[coord].xy;
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
    return g_sdfgi_r_static_screen_probes[coord];
}

FfxFloat32x4 SampleStaticScreenProbesSRV(FfxFloat32x2 uv)
{
    return g_sdfgi_r_static_screen_probes.SampleLevel(g_clamp_nearest_sampler, uv, FfxFloat32(0.0));
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_SCREEN_PROBES
FfxFloat32x4 LoadStaticScreenProbes(FfxUInt32x2 coord)
{
    return g_sdfgi_rw_static_screen_probes[coord];
}

void StoreStaticScreenProbes(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    g_sdfgi_rw_static_screen_probes[coord] = value;

}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_STATIC_GI_TARGET
FfxFloat32x4 SampleStaticGITargetSRV(FfxFloat32x2 uv)
{
    return g_sdfgi_r_static_gitarget.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f);
}

FfxFloat32x4 LoadStaticGITargetSRV(FfxUInt32x2 coord)
{
    return g_sdfgi_r_static_gitarget[coord];
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_STATIC_GI_TARGET
void StoreStaticGITarget(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    g_sdfgi_rw_static_gitarget[coord] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DEBUG_TARGET
FfxFloat32x4 LoadDebugTraget(FfxUInt32x2 coord)
{
    return g_sdfgi_rw_debug_target[coord];
}

void StoreDebugTarget(FfxUInt32x2 coord, FfxFloat32x4 value)
{
    g_sdfgi_rw_debug_target[coord] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DEBUG_VISUALIZATION
void StoreDebugVisualization(FfxUInt32x2 pixel_coordinate, FfxFloat32x4 value)
{
    g_rw_debug_visualization[pixel_coordinate] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_DEPTH
FfxFloat32 LoadSourceDepth(FfxUInt32x2 coord)
{
    return g_src_depth[coord].r;
}

FfxFloat32x4 GatherSourceDepth(FfxFloat32x2 uv)
{
    return g_src_depth.Gather(g_clamp_nearest_sampler, uv);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_DEPTH
FfxFloat32x4 GatherSourcePrevDepth(FfxFloat32x2 uv)
{
    return g_src_history_depth.Gather(g_clamp_nearest_sampler, uv);
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_NORMAL
FfxFloat32x3 LoadSourceNormal(FfxUInt32x2 coord)
{
    return normalize(g_src_normal.Load(FfxInt32x3(coord, 0)).xyz * g_sdfgi_constants.normalsUnpackMul + g_sdfgi_constants.normalsUnpackAdd);
}

FfxFloat32x3 SampleSourceNormal(FfxFloat32x2 uv)
{
    return g_src_normal.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_HISTORY_NORMAL
FfxFloat32x3 SampleSourcePrevNormal(FfxFloat32x2 uv)
{
    return g_src_history_normal.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_ROUGHNESS
FfxFloat32 SampleSourceRoughness(FfxFloat32x2 uv)
{
    return g_src_roughness.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f)[GetScalingRoughnessChannel()];
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_MOTION_VECTORS
FfxFloat32x2 SampleSourceMotionVector(FfxFloat32x2 uv)
{
    return g_src_motion_vectors.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f).xy;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_SOURCE_PREV_LIT_OUTPUT
FfxFloat32x3 SampleSourcePrevLitOutput(FfxFloat32x2 uv)
{
    return g_src_prev_lit_output.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_DIFFUSE_GI
FfxFloat32x3 SampleDownsampledDiffuseGI(FfxFloat32x2 uv)
{
    return g_downsampled_diffuse_gi.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_SRV_DOWNSAMPLED_SPECULAR_GI
FfxFloat32x3 SampleDownsampledSpecularGI(FfxFloat32x2 uv)
{
    return g_downsampled_specular_gi.SampleLevel(g_clamp_nearest_sampler, uv, 0.0f).rgb;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_DEPTH
void StoreDownsampledDepth(FfxUInt32x2 pixel_coordinate, FfxFloat32 value)
{
    g_downsampled_depth[pixel_coordinate] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_DEPTH
void StoreDownsampledPrevDepth(FfxUInt32x2 pixel_coordinate, FfxFloat32 value)
{
    g_downsampled_history_depth[pixel_coordinate] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_NORMAL
void StoreDownsampledNormal(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    g_downsampled_normal[pixel_coordinate] = FfxFloat32x4(value, 0.0f);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_HISTORY_NORMAL
void StoreDownsampledPrevNormal(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    g_downsampled_history_normal[pixel_coordinate] = FfxFloat32x4(value, 0.0f);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_ROUGHNESS
void StoreDownsampledRoughness(FfxUInt32x2 pixel_coordinate, FfxFloat32 value)
{
    g_downsampled_roughness[pixel_coordinate] = value;
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_MOTION_VECTORS
void StoreDownsampledMotionVector(FfxUInt32x2 pixel_coordinate, FfxFloat32x2 value)
{
    g_downsampled_motion_vectors[pixel_coordinate] = FfxFloat32x4(value, 0.0f, 0.0f);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_DOWNSAMPLED_PREV_LIT_OUTPUT
void StoreDownsampledPrevLitOutput(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    g_downsampled_prev_lit_output[pixel_coordinate] = FfxFloat32x4(value, 0.0f);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_UPSAMPLED_DIFFUSE_GI
void StoreUpsampledDiffuseGI(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    g_upsampled_diffuse_gi[pixel_coordinate] = FfxFloat32x4(value, 1.0f);
}
#endif

#if defined BRIXELIZER_GI_BIND_UAV_UPSAMPLED_SPECULAR_GI
void StoreUpsampledSpecularGI(FfxUInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    g_upsampled_specular_gi[pixel_coordinate] = FfxFloat32x4(value, 1.0f);
}
#endif

#endif // #if defined(FFX_GPU)

#endif // FFX_BRIXELIZER_GI_CALLBACKS_HLSL_H
