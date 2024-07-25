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
#ifdef __hlsl_dx_compiler
#pragma dxc diagnostic push
#pragma dxc diagnostic ignored "-Wambig-lit-shift"
#endif //__hlsl_dx_compiler
#include "ffx_core.h"
#ifdef __hlsl_dx_compiler
#pragma dxc diagnostic pop
#endif //__hlsl_dx_compiler

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#if defined(FFX_GPU)
#pragma warning(disable: 3205)  // conversion from larger type to smaller
#endif // #if defined(FFX_GPU)

#define DECLARE_SRV_REGISTER(regIndex)  t##regIndex
#define DECLARE_UAV_REGISTER(regIndex)  u##regIndex
#define DECLARE_CB_REGISTER(regIndex)   b##regIndex
#define FFX_CLASSIFIER_DECLARE_SRV(regIndex)  register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_CLASSIFIER_DECLARE_UAV(regIndex)  register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_CLASSIFIER_DECLARE_CB(regIndex)   register(DECLARE_CB_REGISTER(regIndex))

#if defined(CLASSIFIER_BIND_CB_CLASSIFIER)
cbuffer cbClassifierReflection : FFX_CLASSIFIER_DECLARE_CB(CLASSIFIER_BIND_CB_CLASSIFIER)
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

#define FFX_CLASSIFIER_CONSTANT_BUFFER_1_SIZE 120
};
#else
#define invViewProjection                           0
#define projection                                  0
#define invProjection                               0
#define viewMatrix                                  0
#define invView                                     0
#define prevViewProjection                          0
#define renderSize                                  0
#define inverseRenderSize                           0
#define iblFactor                                   0
#define roughnessThreshold                          0
#define varianceThreshold                           0
#define frameIndex                                  0
#define samplesPerQuad                              0
#define temporalVarianceGuidedTracingEnabled        0
#define globalRoughnessThreshold                    0
#define rtRoughnessThreshold                        0
#define mask                                        0
#define reflectionWidth                             0
#define reflectionHeight                            0
#define hybridMissWeight                            0
#define hybridSpawnRate                             0
#define vrtVarianceThreshold                        0
#define reflectionsBackfacingThreshold              0
#define randomSamplesPerPixel                       0
#define motionVectorScale                           0
#define normalsUnpackMul                            0
#define normalsUnpackAdd                            0
#define roughnessChannel                            0
#define isRoughnessPerceptual                       0
#endif

#if defined(FFX_GPU)
#define FFX_CLASSIFIER_ROOTSIG_STRINGIFY(p) FFX_CLASSIFIER_ROOTSIG_STR(p)
#define FFX_CLASSIFIER_ROOTSIG_STR(p) #p
#define FFX_CLASSIFIER_ROOTSIG [RootSignature("DescriptorTable(UAV(u0, numDescriptors = " FFX_CLASSIFIER_ROOTSIG_STRINGIFY(FFX_CLASSIFIER_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                            "DescriptorTable(SRV(t0, numDescriptors = " FFX_CLASSIFIER_ROOTSIG_STRINGIFY(FFX_CLASSIFIER_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                            "CBV(b0), " \
                                            "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                                                "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                "addressW = TEXTURE_ADDRESS_WRAP, " \
                                                "comparisonFunc = COMPARISON_ALWAYS, " \
                                                "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK, " \
                                                "maxAnisotropy = 1), " \
                                            "StaticSampler(s1, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                                                "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                "comparisonFunc = COMPARISON_ALWAYS, " \
                                                "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK, " \
                                                "maxAnisotropy = 1)" )]

#if defined(FFX_CLASSIFIER_EMBED_ROOTSIG)
#define FFX_CLASSIFIER_EMBED_ROOTSIG_CONTENT FFX_CLASSIFIER_ROOTSIG
#else
#define FFX_CLASSIFIER_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_CLASSIFIER_EMBED_ROOTSIG

#endif // #if defined(FFX_GPU)

SamplerState s_EnvironmentMapSampler : register(s0);
SamplerState s_LinearSampler : register(s1);

FfxFloat32Mat4 InvViewProjection()
{
    return invViewProjection;
}

FfxFloat32Mat4 Projection()
{
    return projection;
}

FfxFloat32Mat4 InvProjection()
{
    return invProjection;
}

FfxFloat32Mat4 ViewMatrix()
{
    return viewMatrix;
}

FfxFloat32Mat4 InvView()
{
    return invView;
}

FfxFloat32Mat4 PrevViewProjection()
{
    return prevViewProjection;
}

FfxUInt32x2 RenderSize()
{
    return renderSize;
}

FfxFloat32x2 InverseRenderSize()
{
    return inverseRenderSize;
}

FfxFloat32 IBLFactor()
{
    return iblFactor;
}

FfxFloat32 RoughnessThreshold()
{
    return globalRoughnessThreshold;
}

FfxUInt32 FrameIndex()
{
    return frameIndex;
}

FfxUInt32 SamplesPerQuad()
{
    return samplesPerQuad;
}

FfxBoolean TemporalVarianceGuidedTracingEnabled()
{
    return FfxBoolean(temporalVarianceGuidedTracingEnabled);
}

FfxFloat32 RTRoughnessThreshold()
{
    return rtRoughnessThreshold;
}

FfxUInt32 Mask()
{
    return mask;
}

FfxUInt32 ReflectionWidth()
{
    return reflectionWidth;
}

FfxUInt32 ReflectionHeight()
{
    return reflectionHeight;
}

FfxFloat32 HybridMissWeight()
{
    return hybridMissWeight;
}

FfxFloat32 HybridSpawnRate()
{
    return hybridSpawnRate;
}

FfxFloat32 VRTVarianceThreshold()
{
    return vrtVarianceThreshold;
}

FfxFloat32 ReflectionsBackfacingThreshold()
{
    return reflectionsBackfacingThreshold;
}

FfxUInt32 RandomSamplesPerPixel()
{
    return randomSamplesPerPixel;
}

FfxFloat32x2 MotionVectorScale()
{
    return motionVectorScale;
}


FfxFloat32 NormalsUnpackMul()
{
    return normalsUnpackMul;
}

FfxFloat32 NormalsUnpackAdd()
{
    return normalsUnpackAdd;
}

FfxUInt32 RoughnessChannel()
{
    return roughnessChannel;
}

FfxBoolean IsRoughnessPerceptual()
{
    return FfxBoolean(isRoughnessPerceptual);
}


#if defined CLASSIFIER_BIND_SRV_INPUT_DEPTH
    Texture2D<FfxFloat32>       r_input_depth               : FFX_CLASSIFIER_DECLARE_SRV(CLASSIFIER_BIND_SRV_INPUT_DEPTH);
#endif
#if defined CLASSIFIER_BIND_SRV_INPUT_MOTION_VECTORS
    Texture2D<FfxFloat32x2>     r_input_motion_vectors      : FFX_CLASSIFIER_DECLARE_SRV(CLASSIFIER_BIND_SRV_INPUT_MOTION_VECTORS);
#endif
#if defined CLASSIFIER_BIND_SRV_INPUT_NORMAL
    Texture2D<FfxFloat32x3>     r_input_normal              : FFX_CLASSIFIER_DECLARE_SRV(CLASSIFIER_BIND_SRV_INPUT_NORMAL);
#endif
#if defined CLASSIFIER_BIND_SRV_INPUT_MATERIAL_PARAMETERS
    Texture2D<FfxFloat32x4>     r_input_material_parameters : FFX_CLASSIFIER_DECLARE_SRV(CLASSIFIER_BIND_SRV_INPUT_MATERIAL_PARAMETERS);
#endif
#if defined CLASSIFIER_BIND_SRV_INPUT_ENVIRONMENT_MAP
    TextureCube                 r_input_environment_map     : FFX_CLASSIFIER_DECLARE_SRV(CLASSIFIER_BIND_SRV_INPUT_ENVIRONMENT_MAP);
#endif
#if defined CLASSIFIER_BIND_SRV_VARIANCE_HISTORY
    Texture2D<FfxFloat32>       r_variance_history          : FFX_CLASSIFIER_DECLARE_SRV(CLASSIFIER_BIND_SRV_VARIANCE_HISTORY);
#endif
#if defined CLASSIFIER_BIND_SRV_HIT_COUNTER_HISTORY
    Texture2D<FfxUInt32>       r_hit_counter_history       : FFX_CLASSIFIER_DECLARE_SRV(CLASSIFIER_BIND_SRV_HIT_COUNTER_HISTORY);
#endif


// UAVs
#if defined CLASSIFIER_BIND_UAV_RADIANCE
        RWTexture2D<FfxFloat32x4>                       rw_radiance                         : FFX_CLASSIFIER_DECLARE_UAV(CLASSIFIER_BIND_UAV_RADIANCE);
#endif
#if defined CLASSIFIER_BIND_UAV_RAY_LIST
        RWStructuredBuffer<FfxUInt32>                   rw_ray_list                         : FFX_CLASSIFIER_DECLARE_UAV(CLASSIFIER_BIND_UAV_RAY_LIST);
#endif
#if defined CLASSIFIER_BIND_UAV_HW_RAY_LIST
        RWStructuredBuffer<FfxUInt32>                   rw_hw_ray_list                      : FFX_CLASSIFIER_DECLARE_UAV(CLASSIFIER_BIND_UAV_HW_RAY_LIST);
#endif
#if defined CLASSIFIER_BIND_UAV_DENOISER_TILE_LIST
        RWStructuredBuffer<FfxUInt32>                   rw_denoiser_tile_list               : FFX_CLASSIFIER_DECLARE_UAV(CLASSIFIER_BIND_UAV_DENOISER_TILE_LIST);
#endif
#if defined CLASSIFIER_BIND_UAV_RAY_COUNTER
        globallycoherent RWStructuredBuffer<FfxUInt32>  rw_ray_counter                      : FFX_CLASSIFIER_DECLARE_UAV(CLASSIFIER_BIND_UAV_RAY_COUNTER);
#endif
#if defined CLASSIFIER_BIND_UAV_EXTRACTED_ROUGHNESS
        RWTexture2D<FfxFloat32>                         rw_extracted_roughness              : FFX_CLASSIFIER_DECLARE_UAV(CLASSIFIER_BIND_UAV_EXTRACTED_ROUGHNESS);
#endif
#if defined CLASSIFIER_BIND_UAV_HIT_COUNTER
            RWTexture2D<FfxUInt32>                      rw_hit_counter                      : FFX_CLASSIFIER_DECLARE_UAV(CLASSIFIER_BIND_UAV_HIT_COUNTER);
#endif

#if FFX_HALF

#endif // #if defined(FFX_HALF)

#if defined(CLASSIFIER_BIND_SRV_INPUT_NORMAL)
FfxFloat32x3 LoadWorldSpaceNormal(FfxInt32x2 pixel_coordinate)
{
    return normalize(NormalsUnpackMul() * r_input_normal.Load(FfxInt32x3(pixel_coordinate, 0)).xyz + NormalsUnpackAdd());
}
#endif // #if defined(CLASSIFIER_BIND_SRV_INPUT_NORMAL)

#if defined(CLASSIFIER_BIND_SRV_INPUT_ENVIRONMENT_MAP)
FfxFloat32x3 SampleEnvironmentMap(FfxFloat32x3 direction, FfxFloat32 preceptualRoughness)
{
    FfxFloat32 Width; FfxFloat32 Height;
    r_input_environment_map.GetDimensions(Width, Height);
    FfxInt32 maxMipLevel = FfxInt32(log2(FfxFloat32(Width > 0 ? Width : 1)));
    FfxFloat32 mip = clamp(preceptualRoughness * FfxFloat32(maxMipLevel), 0.0, FfxFloat32(maxMipLevel));
    return r_input_environment_map.SampleLevel(s_EnvironmentMapSampler, direction, mip).xyz * IBLFactor();
}
#endif // #if defined(CLASSIFIER_BIND_SRV_INPUT_ENVIRONMENT_MAP)

#if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)
void IncrementRayCounterSW(FfxUInt32 value, FFX_PARAMETER_OUT FfxUInt32 original_value)
{
    InterlockedAdd(rw_ray_counter[0], value, original_value);
}
#endif // #if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)

#if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)
void IncrementRayCounterHW(FfxUInt32 value, FFX_PARAMETER_OUT FfxUInt32 original_value)
{
    InterlockedAdd(rw_ray_counter[4], value, original_value);
}
#endif // #if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)

#if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)
void IncrementDenoiserTileCounter(FFX_PARAMETER_OUT FfxUInt32 original_value)
{
    InterlockedAdd(rw_ray_counter[2], 1, original_value);
}
#endif // #if defined (CLASSIFIER_BIND_UAV_RAY_COUNTER)

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

#if defined (CLASSIFIER_BIND_UAV_RAY_LIST)
void StoreRay(FfxInt32 index, FfxUInt32x2 ray_coord, FfxBoolean copy_horizontal, FfxBoolean copy_vertical, FfxBoolean copy_diagonal)
{
    FfxUInt32 packedRayCoords = PackRayCoords(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
    rw_ray_list[index] = packedRayCoords;
}
#endif // #if defined (CLASSIFIER_BIND_UAV_RAY_LIST)

#if defined (CLASSIFIER_BIND_UAV_RAY_LIST)
void StoreRaySWHelper(FfxInt32 index)
{
    rw_ray_list[index] = 0xffffffffu;
}
#endif // #if defined (CLASSIFIER_BIND_UAV_RAY_LIST)

#if defined (CLASSIFIER_BIND_UAV_HW_RAY_LIST)
void StoreRayHW(FfxInt32 index, FfxUInt32x2 ray_coord, FfxBoolean copy_horizontal, FfxBoolean copy_vertical, FfxBoolean copy_diagonal)
{
    FfxUInt32 packedRayCoords = PackRayCoords(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
    rw_hw_ray_list[index] = packedRayCoords;
}
#endif // #if defined (CLASSIFIER_BIND_UAV_HW_RAY_LIST)

#if defined (CLASSIFIER_BIND_UAV_DENOISER_TILE_LIST)
void StoreDenoiserTile(FfxInt32 index, FfxUInt32x2 tile_coord)
{
    rw_denoiser_tile_list[index] = ((tile_coord.y & 0xffffu) << 16) | ((tile_coord.x & 0xffffu) << 0); // Store out pixel to trace
}
#endif // #if defined (CLASSIFIER_BIND_UAV_DENOISER_TILE_LIST)

#if defined (CLASSIFIER_BIND_UAV_EXTRACTED_ROUGHNESS)
void StoreExtractedRoughness(FfxUInt32x2 coordinate, FfxFloat32 roughness)
{
    rw_extracted_roughness[coordinate] = roughness;
}
#endif // #if defined (CLASSIFIER_BIND_UAV_EXTRACTED_ROUGHNESS)

#if defined (CLASSIFIER_BIND_SRV_INPUT_MATERIAL_PARAMETERS)
FfxFloat32 LoadRoughnessFromMaterialParametersInput(FfxUInt32x3 coordinate) /**/
{
    FfxFloat32 rawRoughness = r_input_material_parameters.Load(coordinate)[RoughnessChannel()];
    if (IsRoughnessPerceptual())
    {
        rawRoughness *= rawRoughness;
    }

    return rawRoughness;
}
#endif // #if defined (CLASSIFIER_BIND_SRV_INPUT_MATERIAL_PARAMETERS)

#if defined (CLASSIFIER_BIND_SRV_VARIANCE_HISTORY)
FfxFloat32 SampleVarianceHistory(FfxFloat32x2 coordinate)
{
    return (FfxFloat32)r_variance_history.SampleLevel(s_LinearSampler, coordinate, 0.0f).x;
}
#endif // #if defined (CLASSIFIER_BIND_SRV_VARIANCE_HISTORY)

#if defined (CLASSIFIER_BIND_UAV_RADIANCE)
void StoreRadiance(FfxUInt32x2 coordinate, FfxFloat32x4 radiance) /**/
{
    rw_radiance[coordinate] = radiance;
}
#endif // #if defined (CLASSIFIER_BIND_UAV_RADIANCE)

#if defined (CLASSIFIER_BIND_SRV_INPUT_DEPTH)
FfxFloat32 GetInputDepth(FfxUInt32x2 coordinate)
{
    return r_input_depth[coordinate];
}
#endif // #if defined (CLASSIFIER_BIND_SRV_INPUT_DEPTH)

#if defined(CLASSIFIER_BIND_SRV_HIT_COUNTER_HISTORY)
FfxUInt32 LoadHitCounterHistory(FfxUInt32x2 coordinate)
{
    return r_hit_counter_history[coordinate];
}
#endif // #if defined(CLASSIFIER_BIND_SRV_HIT_COUNTER_HISTORY)

#if defined(CLASSIFIER_BIND_UAV_HIT_COUNTER)
void StoreHitCounter(FfxUInt32x2 coordinate, FfxUInt32 value)
{
    rw_hit_counter[coordinate] = value;
}
#endif // #if defined(CLASSIFIER_BIND_UAV_HIT_COUNTER)

#if defined (CLASSIFIER_BIND_SRV_INPUT_MOTION_VECTORS)
FfxFloat32x2 LoadMotionVector(FfxInt32x2 pixel_coordinate)
{
    return MotionVectorScale() * r_input_motion_vectors.Load(FfxInt32x3(pixel_coordinate, 0));
}
#endif // #if defined (CLASSIFIER_BIND_SRV_INPUT_MOTION_VECTORS)

#endif // #if defined(FFX_GPU)
