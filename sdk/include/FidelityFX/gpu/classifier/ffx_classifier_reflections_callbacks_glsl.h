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

#include "ffx_classifier_resources.h"

#if defined(FFX_GPU)
#include "ffx_core.h"

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#if defined(CLASSIFIER_BIND_CB_CLASSIFIER)
    layout (set = 0, binding = CLASSIFIER_BIND_CB_CLASSIFIER, std140) uniform cbClassifierReflections_t
    {
        FfxFloat32Mat4  invViewProjection;
        FfxFloat32Mat4  projection;
        FfxFloat32Mat4  invProjection;
        FfxFloat32Mat4  viewMatrix;
        FfxFloat32Mat4  invView;
        FfxFloat32Mat4  prevViewProjection;
        FfxUInt32x2     renderSize;
        FfxFloat32x2    inverseRenderSize;
        FfxFloat32      iblFactor;
        FfxUInt32       frameIndex;
        FfxUInt32       samplesPerQuad;
        FfxUInt32       temporalVarianceGuidedTracingEnabled;
        FfxFloat32      globalRoughnessThreshold;
        FfxFloat32      rtRoughnessThreshold;
        FfxUInt32       mask;
        FfxUInt32       reflectionWidth;
        FfxUInt32       reflectionHeight;
        FfxFloat32      hybridMissWeight;
        FfxFloat32      hybridSpawnRate;
        FfxFloat32      vrtVarianceThreshold;
        FfxFloat32      reflectionsBackfacingThreshold;
        FfxUInt32       randomSamplesPerPixel;
        FfxFloat32x2    motionVectorScale;
        FfxFloat32      normalsUnpackMul;
        FfxFloat32      normalsUnpackAdd;
        FfxUInt32       roughnessChannel;
        FfxUInt32       isRoughnessPerceptual;
    } cbClassifierReflection;

FfxFloat32Mat4 InvViewProjection()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.invViewProjection;
#else
    return FfxFloat32Mat4(0.0f);
#endif
}

FfxFloat32Mat4 Projection()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.projection;
#else
    return FfxFloat32Mat4(0.0f);
#endif
}

FfxFloat32Mat4 InvProjection()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.invProjection;
#else
    return FfxFloat32Mat4(0.0f);
#endif
}

FfxFloat32Mat4 ViewMatrix()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.viewMatrix;
#else
    return FfxFloat32Mat4(0.0f);
#endif
}

FfxFloat32Mat4 InvView()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.invView;
#else
    return FfxFloat32Mat4(0.0f);
#endif
}

FfxFloat32Mat4 PrevViewProjection()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.prevViewProjection;
#else
    return FfxFloat32Mat4(0.0f);
#endif
}

FfxUInt32x2 RenderSize()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.renderSize;
#else
    return FfxUInt32x2(0);
#endif
}

FfxFloat32x2 InverseRenderSize()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.inverseRenderSize;
#else
    return FfxFloat32x2(0.0f);
#endif
}

FfxFloat32 IBLFactor()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.iblFactor;
#else
    return 0.0f;
#endif
}

FfxUInt32 FrameIndex()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.frameIndex;
#else
    return 0;
#endif
}

FfxUInt32 SamplesPerQuad()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.samplesPerQuad;
#else
    return 0;
#endif
}

FfxBoolean TemporalVarianceGuidedTracingEnabled()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return FfxBoolean(cbClassifierReflection.temporalVarianceGuidedTracingEnabled);
#else
    return false;
#endif
}

FfxFloat32 RoughnessThreshold()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.globalRoughnessThreshold;
#else
    return 0.0f;
#endif
}

FfxFloat32 RTRoughnessThreshold()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.rtRoughnessThreshold;
#else
    return 0.0f;
#endif
}

FfxUInt32 Mask()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.mask;
#else
    return 0;
#endif
}

FfxUInt32 ReflectionWidth()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.reflectionWidth;
#else
    return 0;
#endif
}

FfxUInt32 ReflectionHeight()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.reflectionHeight;
#else
    return 0;
#endif
}

FfxFloat32 HybridMissWeight()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.hybridMissWeight;
#else
    return 0.f;
#endif
}

FfxFloat32 HybridSpawnRate()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.hybridSpawnRate;
#else
    return 0.f;
#endif
}

FfxFloat32 VRTVarianceThreshold()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.vrtVarianceThreshold;
#else
    return 0.f;
#endif
}

FfxFloat32 ReflectionsBackfacingThreshold()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.reflectionsBackfacingThreshold;
#else
    return 0.f;
#endif
}

FfxUInt32 RandomSamplesPerPixel()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.randomSamplesPerPixel;
#else
    return 0;
#endif
}

FfxFloat32x2 MotionVectorScale()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.motionVectorScale;
#else
    return FfxFloat32x2(0.0f);
#endif
}

FfxFloat32 NormalsUnpackMul()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.normalsUnpackMul;
#else
    return 0.0f;
#endif
}

FfxFloat32 NormalsUnpackAdd()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.normalsUnpackAdd;
#else
    return 0.0f;
#endif
}

FfxUInt32 RoughnessChannel()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return cbClassifierReflection.roughnessChannel;
#else
    return 0;
#endif
}

FfxBoolean IsRoughnessPerceptual()
{
#if defined CLASSIFIER_BIND_CB_CLASSIFIER
    return FfxBoolean(cbClassifierReflection.isRoughnessPerceptual);
#else
    return false;
#endif
}

#endif // #if defined(CLASSIFIER_BIND_CB_CLASSIFIER)

layout (set = 0, binding = 1000) uniform sampler s_EnvironmentMapSampler;
layout (set = 0, binding = 1001) uniform sampler s_LinearSampler;

// SRVs
#if defined CLASSIFIER_BIND_SRV_INPUT_DEPTH
    layout (set = 0, binding = CLASSIFIER_BIND_SRV_INPUT_DEPTH)               uniform texture2D r_input_depth;
#endif
#if defined CLASSIFIER_BIND_SRV_INPUT_MOTION_VECTORS
    layout (set = 0, binding = CLASSIFIER_BIND_SRV_INPUT_MOTION_VECTORS)      uniform texture2D r_input_motion_vectors;
#endif
#if defined CLASSIFIER_BIND_SRV_INPUT_NORMAL
    layout (set = 0, binding = CLASSIFIER_BIND_SRV_INPUT_NORMAL)              uniform texture2D r_input_normal;
#endif
#if defined CLASSIFIER_BIND_SRV_INPUT_MATERIAL_PARAMETERS
    layout (set = 0, binding = CLASSIFIER_BIND_SRV_INPUT_MATERIAL_PARAMETERS) uniform texture2D r_input_material_parameters;
#endif
#if defined CLASSIFIER_BIND_SRV_INPUT_ENVIRONMENT_MAP
    layout (set = 0, binding = CLASSIFIER_BIND_SRV_INPUT_ENVIRONMENT_MAP)     uniform textureCube r_input_environment_map;
#endif
#if defined CLASSIFIER_BIND_SRV_VARIANCE_HISTORY
    layout (set = 0, binding = CLASSIFIER_BIND_SRV_VARIANCE_HISTORY)          uniform texture2D r_variance_history;
#endif
#if defined CLASSIFIER_BIND_SRV_HIT_COUNTER_HISTORY
    layout(set = 0, binding = CLASSIFIER_BIND_SRV_HIT_COUNTER_HISTORY)        uniform utexture2D r_hit_counter_history;
#endif

// UAVs
#if defined CLASSIFIER_BIND_UAV_RADIANCE
        layout (set = 0, binding = CLASSIFIER_BIND_UAV_RADIANCE, rgba32f)                  uniform image2D rw_radiance;
#endif
#if defined CLASSIFIER_BIND_UAV_RAY_LIST
        layout (set = 0, binding = CLASSIFIER_BIND_UAV_RAY_LIST, std430)                   buffer rw_ray_list_t
        {
            FfxUInt32 data[];
        } rw_ray_list; 
#endif
#if defined CLASSIFIER_BIND_UAV_HW_RAY_LIST
        layout(set = 0, binding = CLASSIFIER_BIND_UAV_HW_RAY_LIST, std430)                 buffer rw_hw_ray_list_t
        {
            FfxUInt32 data[];
        } rw_hw_ray_list;
#endif
#if defined CLASSIFIER_BIND_UAV_DENOISER_TILE_LIST
        layout (set = 0, binding = CLASSIFIER_BIND_UAV_DENOISER_TILE_LIST, std430)         buffer rw_denoiser_tile_list_t
        {
            FfxUInt32 data[];
        } rw_denoiser_tile_list;
#endif
#if defined CLASSIFIER_BIND_UAV_RAY_COUNTER
        layout (set = 0, binding = CLASSIFIER_BIND_UAV_RAY_COUNTER, std430)                buffer rw_ray_counter_t
        {
            FfxUInt32 data[];
        } rw_ray_counter;
#endif
#if defined CLASSIFIER_BIND_UAV_EXTRACTED_ROUGHNESS
        layout (set = 0, binding = CLASSIFIER_BIND_UAV_EXTRACTED_ROUGHNESS, r32f)          uniform image2D rw_extracted_roughness;
#endif
#if defined CLASSIFIER_BIND_UAV_HIT_COUNTER
        layout(set = 0, binding = CLASSIFIER_BIND_UAV_HIT_COUNTER, r32ui)                  uniform uimage2D rw_hit_counter;
#endif


FfxFloat32x3 LoadWorldSpaceNormal(FfxInt32x2 pixel_coordinate)
{
#if defined(CLASSIFIER_BIND_SRV_INPUT_NORMAL)
    return normalize(NormalsUnpackMul() * texelFetch(r_input_normal, pixel_coordinate, 0).xyz + NormalsUnpackAdd());
#else
    return FfxFloat32x3(0.f);
#endif
}

FfxFloat32x3 SampleEnvironmentMap(FfxFloat32x3 direction, FfxFloat32 preceptualRoughness)
{
#if defined(CLASSIFIER_BIND_SRV_INPUT_ENVIRONMENT_MAP)
    FfxInt32x2 cubeSize = textureSize(r_input_environment_map, 0);
    FfxInt32 maxMipLevel = FfxInt32(log2(FfxFloat32(cubeSize.x > 0 ? cubeSize.x : 1)));
    FfxFloat32 lod = clamp(preceptualRoughness * FfxFloat32(maxMipLevel), 0.0, FfxFloat32(maxMipLevel));
    return textureLod(samplerCube(r_input_environment_map, s_EnvironmentMapSampler), direction, lod).xyz * IBLFactor();
#else
    return FfxFloat32x3(0.0f);
#endif
}

void IncrementRayCounterSW(FfxUInt32 value, FFX_PARAMETER_OUT FfxUInt32 original_value)
{
#if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)
    original_value = atomicAdd(rw_ray_counter.data[0], value);
#endif
}

void IncrementRayCounterHW(FfxUInt32 value, FFX_PARAMETER_OUT FfxUInt32 original_value)
{
#if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)
    original_value = atomicAdd(rw_ray_counter.data[4], value);
#endif
}

void IncrementDenoiserTileCounter(FFX_PARAMETER_OUT FfxUInt32 original_value)
{
#if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)
    original_value = atomicAdd(rw_ray_counter.data[2], 1);
#endif
}

FfxUInt32 PackRayCoords(FfxUInt32x2 ray_coord, FfxBoolean copy_horizontal, FfxBoolean copy_vertical, FfxBoolean copy_diagonal)
{
    FfxUInt32 ray_x_15bit = ray_coord.x & 32767;    // 0b111111111111111
    FfxUInt32 ray_y_14bit = ray_coord.y & 16383;    // 0b11111111111111;
    FfxUInt32 copy_horizontal_1bit = copy_horizontal ? 1 : 0;
    FfxUInt32 copy_vertical_1bit = copy_vertical ? 1 : 0;
    FfxUInt32 copy_diagonal_1bit = copy_diagonal ? 1 : 0;

    FfxUInt32 packed = (copy_diagonal_1bit << 31) | (copy_vertical_1bit << 30) | (copy_horizontal_1bit << 29) | (ray_y_14bit << 15) | (ray_x_15bit << 0);
    return packed;
}

void StoreRay(FfxUInt32 index, FfxUInt32x2 ray_coord, FfxBoolean copy_horizontal, FfxBoolean copy_vertical, FfxBoolean copy_diagonal)
{
#if defined (CLASSIFIER_BIND_UAV_RAY_LIST)
    FfxUInt32 packedRayCoords = PackRayCoords(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
    rw_ray_list.data[index] = packedRayCoords;
#endif
}

void StoreRaySWHelper(FfxUInt32 index)
{
#if defined (CLASSIFIER_BIND_UAV_RAY_LIST)
    rw_ray_list.data[index] = 0xffffffffu;
#endif
}

void StoreRayHW(FfxUInt32 index, FfxUInt32x2 ray_coord, FfxBoolean copy_horizontal, FfxBoolean copy_vertical, FfxBoolean copy_diagonal)
{
#if defined (CLASSIFIER_BIND_UAV_RAY_LIST)
    FfxUInt32 packedRayCoords = PackRayCoords(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
    rw_hw_ray_list.data[index] = packedRayCoords;
#endif
}

void StoreDenoiserTile(FfxUInt32 index, FfxUInt32x2 tile_coord)
{
#if defined (CLASSIFIER_BIND_UAV_DENOISER_TILE_LIST)
    rw_denoiser_tile_list.data[index] = ((tile_coord.y & 0xffffu) << 16) | ((tile_coord.x & 0xffffu) << 0); // Store out pixel to trace
#endif
}

void StoreExtractedRoughness(FfxUInt32x2 coordinate, FfxFloat32 roughness)
{
#if defined (CLASSIFIER_BIND_UAV_EXTRACTED_ROUGHNESS)
    imageStore(rw_extracted_roughness, FfxInt32x2(coordinate), FfxFloat32x4(roughness));
#endif
}

FfxFloat32 LoadRoughnessFromMaterialParametersInput(FfxUInt32x3 coordinate)
{
#if defined (CLASSIFIER_BIND_SRV_INPUT_MATERIAL_PARAMETERS)
    FfxFloat32 rawRoughness = texelFetch(r_input_material_parameters, FfxInt32x2(coordinate.xy), FfxInt32(coordinate.z))[RoughnessChannel()];
    if (IsRoughnessPerceptual())
    {
        rawRoughness *= rawRoughness;
    }

    return rawRoughness;
#else
    return 0.0f;
#endif
}

FfxFloat32 SampleVarianceHistory(FfxFloat32x2 coordinate) /**/
{
#if defined ( CLASSIFIER_BIND_SRV_VARIANCE_HISTORY )
    return FfxFloat32(textureLod(sampler2D(r_variance_history, s_LinearSampler), coordinate, 0.0f).x);
#else
    return 0.0;
#endif
}

void StoreRadiance(FfxUInt32x2 coordinate, FfxFloat32x4 radiance)
{
#if defined (CLASSIFIER_BIND_UAV_RADIANCE)
    imageStore(rw_radiance, FfxInt32x2(coordinate), radiance);
#endif
}

FfxFloat32 GetInputDepth(FfxUInt32x2 coordinate)
{
#if defined (CLASSIFIER_BIND_SRV_INPUT_DEPTH)
    return texelFetch(r_input_depth, FfxInt32x2(coordinate), 0).r;
#else
    return 0.0f;
#endif
}

void StoreHitCounter(FfxUInt32x2 coordinate, FfxUInt32 value)
{
#if defined(CLASSIFIER_BIND_UAV_HIT_COUNTER)
    imageStore(rw_hit_counter, FfxInt32x2(coordinate), FfxUInt32x4(value));
#endif
}

FfxUInt32 LoadHitCounterHistory(FfxUInt32x2 coordinate)
{
#if defined(CLASSIFIER_BIND_SRV_HIT_COUNTER_HISTORY)
    return FfxUInt32(texelFetch(r_hit_counter_history, FfxInt32x2(coordinate), 0).r);
#else
    return 0;
#endif
}

FfxFloat32x2 LoadMotionVector(FfxInt32x2 pixel_coordinate)
{
#if defined (CLASSIFIER_BIND_SRV_INPUT_MOTION_VECTORS) 
    return MotionVectorScale() * texelFetch(r_input_motion_vectors, pixel_coordinate, 0).xy;
#else
    return FfxFloat32x2(0.0f);
#endif
}

#endif // #if defined(FFX_GPU)
