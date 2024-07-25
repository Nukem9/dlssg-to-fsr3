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

#include "ffx_denoiser_resources.h"

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

#pragma warning(disable: 3205)  // conversion from larger type to smaller

#define DECLARE_SRV_REGISTER(regIndex)  t##regIndex
#define DECLARE_UAV_REGISTER(regIndex)  u##regIndex
#define DECLARE_CB_REGISTER(regIndex)   b##regIndex
#define FFX_DENOISER_DECLARE_SRV(regIndex)  register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_DENOISER_DECLARE_UAV(regIndex)  register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_DENOISER_DECLARE_CB(regIndex)   register(DECLARE_CB_REGISTER(regIndex))

#if defined(DENOISER_BIND_CB_DENOISER)
    cbuffer cbDenoiserReflections : FFX_DENOISER_DECLARE_CB(DENOISER_BIND_CB_DENOISER)
    {
        FfxFloat32Mat4  invProjection;
        FfxFloat32Mat4  invView;
        FfxFloat32Mat4  prevViewProjection;
        FfxUInt32x2     renderSize;
        FfxFloat32x2    inverseRenderSize;
        FfxFloat32x2    motionVectorScale;
        FfxFloat32      normalsUnpackMul;
        FfxFloat32      normalsUnpackAdd;
        FfxBoolean      isRoughnessPerceptual;
        FfxFloat32      temporalStabilityFactor;
        FfxFloat32      roughnessThreshold;
       #define FFX_DENOISER_CONSTANT_BUFFER_1_SIZE 54  // Number of 32-bit values. This must be kept in sync with the cbDenoiser size.
    };
#else
    #define invProjection                               0
    #define invView                                     0
    #define prevViewProjection                          0
    #define renderSize                                  0
    #define inverseRenderSize                           0
    #define motionVectorScale                           0
    #define normalsUnpackMul                            0
    #define normalsUnpackAdd                            0
    #define isRoughnessPerceptual                       0
    #define temporalStabilityFactor                     0
    #define roughnessThreshold                          0
#endif

#if defined(FFX_GPU)
#define FFX_DENOISER_ROOTSIG_STRINGIFY(p) FFX_DENOISER_ROOTSIG_STR(p)
#define FFX_DENOISER_ROOTSIG_STR(p) #p
#define FFX_DENOISER_ROOTSIG [RootSignature("DescriptorTable(UAV(u0, numDescriptors = " FFX_DENOISER_ROOTSIG_STRINGIFY(FFX_DENOISER_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                            "DescriptorTable(SRV(t0, numDescriptors = " FFX_DENOISER_ROOTSIG_STRINGIFY(FFX_DENOISER_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                            "CBV(b0), " \
                                            "StaticSampler(s0, filter = FILTER_MIN_MAG_LINEAR_MIP_POINT, " \
                                                "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                "comparisonFunc = COMPARISON_ALWAYS, " \
                                                "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK, " \
                                                "maxAnisotropy = 1)" )]

#if defined(FFX_DENOISER_EMBED_ROOTSIG)
#define FFX_DENOISER_EMBED_ROOTSIG_CONTENT FFX_DENOISER_ROOTSIG
#else
#define FFX_DENOISER_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_DENOISER_EMBED_ROOTSIG

#endif // #if defined(FFX_GPU)

SamplerState s_LinearSampler : register(s0);

FfxFloat32Mat4 InvProjection()
{
    return invProjection;
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

FfxBoolean IsRoughnessPerceptual()
{
    return isRoughnessPerceptual;
}

FfxFloat32 TemporalStabilityFactor()
{
    return temporalStabilityFactor;
}

FfxFloat32 RoughnessThreshold()
{
    return roughnessThreshold;
}

// SRVs
    #if defined DENOISER_BIND_SRV_INPUT_DEPTH_HIERARCHY
        Texture2D<FfxFloat32>       r_input_depth_hierarchy     : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_INPUT_DEPTH_HIERARCHY);
    #endif
    #if defined DENOISER_BIND_SRV_INPUT_MOTION_VECTORS
        Texture2D<FfxFloat32x2>     r_input_motion_vectors      : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_INPUT_MOTION_VECTORS);
    #endif
    #if defined DENOISER_BIND_SRV_INPUT_NORMAL
        Texture2D<FfxFloat32x3>     r_input_normal              : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_INPUT_NORMAL);
    #endif
    #if defined DENOISER_BIND_SRV_RADIANCE
        Texture2D<FfxFloat32x4>     r_radiance                  : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_RADIANCE);
    #endif
    #if defined DENOISER_BIND_SRV_RADIANCE_HISTORY
        Texture2D<FfxFloat32x4>     r_radiance_history          : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_RADIANCE_HISTORY);
    #endif
    #if defined DENOISER_BIND_SRV_VARIANCE
        Texture2D<FfxFloat32>       r_variance                  : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_VARIANCE);
    #endif
    #if defined DENOISER_BIND_SRV_SAMPLE_COUNT
        Texture2D<FfxFloat32>       r_sample_count              : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_SAMPLE_COUNT);
    #endif
    #if defined DENOISER_BIND_SRV_AVERAGE_RADIANCE
        Texture2D<FfxFloat32x3>     r_average_radiance          : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_AVERAGE_RADIANCE);
    #endif
    #if defined DENOISER_BIND_SRV_EXTRACTED_ROUGHNESS
        Texture2D<FfxFloat32>       r_extracted_roughness       : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_EXTRACTED_ROUGHNESS);
    #endif
    #if defined DENOISER_BIND_SRV_DEPTH_HISTORY
        Texture2D<FfxFloat32>       r_depth_history             : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_DEPTH_HISTORY);
    #endif
    #if defined DENOISER_BIND_SRV_NORMAL_HISTORY
        Texture2D<FfxFloat32x3>     r_normal_history            : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_NORMAL_HISTORY);
    #endif
    #if defined DENOISER_BIND_SRV_ROUGHNESS_HISTORY
        Texture2D<FfxFloat32>       r_roughness_history         : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_ROUGHNESS_HISTORY);
    #endif
    #if defined DENOISER_BIND_SRV_REPROJECTED_RADIANCE
        Texture2D<FfxFloat32x4>     r_reprojected_radiance      : FFX_DENOISER_DECLARE_SRV(DENOISER_BIND_SRV_REPROJECTED_RADIANCE);
    #endif

// UAVs
    #if defined DENOISER_BIND_UAV_RADIANCE
            RWTexture2D<FfxFloat32x4>                       rw_radiance                         : FFX_DENOISER_DECLARE_UAV(DENOISER_BIND_UAV_RADIANCE);
    #endif
    #if defined DENOISER_BIND_UAV_VARIANCE
            RWTexture2D<FfxFloat32>                         rw_variance                         : FFX_DENOISER_DECLARE_UAV(DENOISER_BIND_UAV_VARIANCE);
    #endif
    #if defined DENOISER_BIND_UAV_SAMPLE_COUNT
            RWTexture2D<FfxFloat32>                         rw_sample_count                     : FFX_DENOISER_DECLARE_UAV(DENOISER_BIND_UAV_SAMPLE_COUNT);
    #endif
    #if defined DENOISER_BIND_UAV_AVERAGE_RADIANCE
            RWTexture2D<FfxFloat32x3>                       rw_average_radiance                 : FFX_DENOISER_DECLARE_UAV(DENOISER_BIND_UAV_AVERAGE_RADIANCE);
    #endif
    #if defined DENOISER_BIND_UAV_DENOISER_TILE_LIST
            RWStructuredBuffer<FfxUInt32>                   rw_denoiser_tile_list               : FFX_DENOISER_DECLARE_UAV(DENOISER_BIND_UAV_DENOISER_TILE_LIST);
    #endif
    #if defined DENOISER_BIND_UAV_REPROJECTED_RADIANCE
            RWTexture2D<FfxFloat32x3>                       rw_reprojected_radiance             : FFX_DENOISER_DECLARE_UAV(DENOISER_BIND_UAV_REPROJECTED_RADIANCE);
    #endif

#if FFX_HALF


FfxFloat16x3 FFX_DENOISER_LoadWorldSpaceNormalH(FfxInt32x2 pixel_coordinate)
{
    #if defined(DENOISER_BIND_SRV_INPUT_NORMAL)
        return normalize((FfxFloat16x3)(NormalsUnpackMul() * r_input_normal.Load(FfxInt32x3(pixel_coordinate, 0)) + NormalsUnpackAdd()));
    #else
        return 0.0f;
    #endif // #if defined(DENOISER_BIND_SRV_INPUT_NORMAL) 
}

FfxFloat16x3 LoadRadianceH(FfxInt32x3 coordinate)
{
    #if defined (DENOISER_BIND_SRV_RADIANCE)
        return (FfxFloat16x3)r_radiance.Load(coordinate).xyz;
    #else
        return 0.0f;
    #endif // #if defined (DENOISER_BIND_SRV_RADIANCE)
}

FfxFloat16 LoadVarianceH(FfxInt32x3 coordinate)
{
    #if defined (DENOISER_BIND_SRV_VARIANCE)
        return (FfxFloat16)r_variance.Load(coordinate).x;
    #else
        return 0.0f;
    #endif // #if defined (DENOISER_BIND_SRV_VARIANCE)
}

#if defined (DENOISER_BIND_SRV_AVERAGE_RADIANCE)
FfxFloat16x3 FFX_DNSR_Reflections_SampleAverageRadiance(FfxFloat32x2 uv)
{
    return (FfxFloat16x3)r_average_radiance.SampleLevel(s_LinearSampler, uv, 0.0f).xyz;
}
#endif // #if defined (DENOISER_BIND_SRV_AVERAGE_RADIANCE)

#if defined (DENOISER_BIND_SRV_EXTRACTED_ROUGHNESS) 
FfxFloat16 FFX_DNSR_Reflections_LoadRoughness(FfxInt32x2 pixel_coordinate)
{
    FfxFloat16 rawRoughness = (FfxFloat16)r_extracted_roughness.Load(FfxInt32x3(pixel_coordinate, 0));
    if (IsRoughnessPerceptual())
    {
        rawRoughness *= rawRoughness;
    }

    return rawRoughness;
}
#endif // #if defined (DENOISER_BIND_SRV_EXTRACTED_ROUGHNESS) 

void StoreRadianceH(FfxInt32x2 coordinate, FfxFloat16x4 radiance)
{
    #if defined (DENOISER_BIND_UAV_RADIANCE)
        rw_radiance[coordinate] = radiance;
    #endif // #if defined (DENOISER_BIND_UAV_RADIANCE)
}

void StoreVarianceH(FfxInt32x2 coordinate, FfxFloat16 variance)
{
    #if defined (DENOISER_BIND_UAV_VARIANCE)
        rw_variance[coordinate] = variance;
    #endif // #if defined (DENOISER_BIND_UAV_VARIANCE)
}

void FFX_DNSR_Reflections_StorePrefilteredReflections(FfxInt32x2 pixel_coordinate, FfxFloat16x3 radiance, FfxFloat16 variance)
{
    StoreRadianceH(pixel_coordinate, radiance.xyzz);
    StoreVarianceH(pixel_coordinate, variance.x);
}

void FFX_DNSR_Reflections_StoreTemporalAccumulation(FfxInt32x2 pixel_coordinate, FfxFloat16x3 radiance, FfxFloat16 variance)
{
    StoreRadianceH(pixel_coordinate, radiance.xyzz);
    StoreVarianceH(pixel_coordinate, variance.x);
}

#if defined (DENOISER_BIND_SRV_RADIANCE_HISTORY)
FfxFloat16x3 FFX_DNSR_Reflections_LoadRadianceHistory(FfxInt32x2 pixel_coordinate)
{
    return (FfxFloat16x3)r_radiance_history.Load(FfxInt32x3(pixel_coordinate, 0)).xyz;
}
#endif // #if defined (DENOISER_BIND_SRV_RADIANCE_HISTORY)

#if defined (DENOISER_BIND_SRV_RADIANCE_HISTORY)
FfxFloat16x3 FFX_DNSR_Reflections_SampleRadianceHistory(FfxFloat32x2 uv)
{
    return (FfxFloat16x3)r_radiance_history.SampleLevel(s_LinearSampler, uv, 0.0f).xyz;
}
#endif // #if defined (DENOISER_BIND_SRV_RADIANCE_HISTORY)

#if defined (DENOISER_BIND_SRV_VARIANCE)
FfxFloat16 FFX_DNSR_Reflections_SampleVarianceHistory(FfxFloat32x2 uv)
{
    return (FfxFloat16)r_variance.SampleLevel(s_LinearSampler, uv, 0.0f).x;
}
#endif // #if defined (DENOISER_BIND_SRV_VARIANCE)

#if defined (DENOISER_BIND_SRV_SAMPLE_COUNT)
FfxFloat16 FFX_DNSR_Reflections_SampleNumSamplesHistory(FfxFloat32x2 uv)
{
    return (FfxFloat16)r_sample_count.SampleLevel(s_LinearSampler, uv, 0.0f).x;
}
#endif // #if defined (DENOISER_BIND_SRV_SAMPLE_COUNT)

#if defined (DENOISER_BIND_UAV_REPROJECTED_RADIANCE)
void FFX_DNSR_Reflections_StoreRadianceReprojected(FfxInt32x2 pixel_coordinate, FfxFloat16x3 value)
{
    rw_reprojected_radiance[pixel_coordinate] = value;
}
#endif // #if defined (DENOISER_BIND_UAV_REPROJECTED_RADIANCE)

#if defined (DENOISER_BIND_UAV_AVERAGE_RADIANCE)
void FFX_DNSR_Reflections_StoreAverageRadiance(FfxInt32x2 pixel_coordinate, FfxFloat16x3 value)
{
    rw_average_radiance[pixel_coordinate] = value;
}
#endif // #if defined (DENOISER_BIND_UAV_AVERAGE_RADIANCE)

FfxFloat16x3 FFX_DNSR_Reflections_LoadWorldSpaceNormal(FfxInt32x2 pixel_coordinate)
{
    return FFX_DENOISER_LoadWorldSpaceNormalH(pixel_coordinate);
}

#if defined (DENOISER_BIND_SRV_ROUGHNESS_HISTORY)
FfxFloat16 FFX_DNSR_Reflections_SampleRoughnessHistory(FfxFloat32x2 uv)
{
    FfxFloat16 rawRoughness = (FfxFloat16)r_roughness_history.SampleLevel(s_LinearSampler, uv, 0.0f);
    if (IsRoughnessPerceptual())
    {
        rawRoughness *= rawRoughness;
    }

    return rawRoughness;
}
#endif // #if defined (DENOISER_BIND_SRV_ROUGHNESS_HISTORY)

#if defined (DENOISER_BIND_SRV_NORMAL_HISTORY)
FfxFloat16x3 FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(FfxInt32x2 pixel_coordinate)
{
    return normalize((FfxFloat16x3)(NormalsUnpackMul() * r_normal_history.Load(FfxInt32x3(pixel_coordinate, 0)) + NormalsUnpackAdd()));
}
#endif // #if defined (DENOISER_BIND_SRV_NORMAL_HISTORY)

#if defined (DENOISER_BIND_SRV_NORMAL_HISTORY)
FfxFloat16x3 FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(FfxFloat32x2 uv)
{
    return normalize((FfxFloat16x3)(NormalsUnpackMul() * r_normal_history.SampleLevel(s_LinearSampler, uv, 0.0f) + NormalsUnpackAdd()));
}
#endif // #if defined (DENOISER_BIND_SRV_NORMAL_HISTORY)

#if defined (DENOISER_BIND_SRV_RADIANCE)
FfxFloat16 FFX_DNSR_Reflections_LoadRayLength(FfxInt32x2 pixel_coordinate)
{
    return (FfxFloat16)r_radiance.Load(FfxInt32x3(pixel_coordinate, 0)).w;
}
#endif // #if defined (DENOISER_BIND_SRV_RADIANCE)

void FFX_DNSR_Reflections_StoreVariance(FfxInt32x2 pixel_coordinate, FfxFloat16 value)
{
    StoreVarianceH(pixel_coordinate, value);
}

#if defined (DENOISER_BIND_UAV_SAMPLE_COUNT)
void FFX_DNSR_Reflections_StoreNumSamples(FfxInt32x2 pixel_coordinate, FfxFloat16 value)
{
    rw_sample_count[pixel_coordinate] = value;
}
#endif // #if defined (DENOISER_BIND_UAV_SAMPLE_COUNT)

FfxFloat16x3 FFX_DNSR_Reflections_LoadRadiance(FfxInt32x2 pixel_coordinate)
{
    return LoadRadianceH(FfxInt32x3(pixel_coordinate, 0));
}

#if defined (DENOISER_BIND_SRV_REPROJECTED_RADIANCE)
FfxFloat16x3 FFX_DNSR_Reflections_LoadRadianceReprojected(FfxInt32x2 pixel_coordinate)
{
    return (FfxFloat16x3)r_reprojected_radiance.Load(FfxInt32x3(pixel_coordinate, 0)).xyz;
}
#endif // #if defined (DENOISER_BIND_SRV_REPROJECTED_RADIANCE)

FfxFloat16 FFX_DNSR_Reflections_LoadVariance(FfxInt32x2 pixel_coordinate)
{
    return LoadVarianceH(FfxInt32x3(pixel_coordinate, 0));
}

#if defined (DENOISER_BIND_SRV_SAMPLE_COUNT)
FfxFloat16 FFX_DNSR_Reflections_LoadNumSamples(FfxInt32x2 pixel_coordinate)
{
    return (FfxFloat16)r_sample_count.Load(FfxInt32x3(pixel_coordinate, 0)).x;
}
#endif // #if defined (DENOISER_BIND_SRV_SAMPLE_COUNT)

#else // FFX_HALF

FfxFloat32x3 LoadRadiance(FfxInt32x3 coordinate)
{
#if defined (DENOISER_BIND_SRV_RADIANCE)
    return r_radiance.Load(coordinate).xyz;
#else
    return 0.0f;
#endif
}

FfxFloat32 LoadVariance(FfxInt32x3 coordinate)
{
#if defined (DENOISER_BIND_SRV_VARIANCE) 
    return r_variance.Load(coordinate).x;
#else
    return 0.0f;
#endif
}

FfxFloat32x3 FFX_DENOISER_LoadWorldSpaceNormal(FfxInt32x2 pixel_coordinate)
{
#if defined(DENOISER_BIND_SRV_INPUT_NORMAL)
    return normalize(NormalsUnpackMul() * r_input_normal.Load(FfxInt32x3(pixel_coordinate, 0)) + NormalsUnpackAdd());
#else
    return 0.0f;
#endif
}

#if defined (DENOISER_BIND_SRV_EXTRACTED_ROUGHNESS)
FfxFloat32 FFX_DNSR_Reflections_LoadRoughness(FfxInt32x2 pixel_coordinate)
{ 
    FfxFloat32 rawRoughness = (FfxFloat32)r_extracted_roughness.Load(FfxInt32x3(pixel_coordinate, 0));
    if (IsRoughnessPerceptual())
    {
        rawRoughness *= rawRoughness;
    }

    return rawRoughness;
}
#endif // #if defined (DENOISER_BIND_SRV_EXTRACTED_ROUGHNESS)

void StoreRadiance(FfxInt32x2 coordinate, FfxFloat32x4 radiance)
{
#if defined (DENOISER_BIND_UAV_RADIANCE) 
    rw_radiance[coordinate] = radiance;
#endif
}

void StoreVariance(FfxInt32x2 coordinate, FfxFloat32 variance)
{
#if defined (DENOISER_BIND_UAV_VARIANCE) 
    rw_variance[coordinate] = variance;
#endif
}

void FFX_DNSR_Reflections_StorePrefilteredReflections(FfxInt32x2 pixel_coordinate, FfxFloat32x3 radiance, FfxFloat32 variance)
{
    StoreRadiance(pixel_coordinate, radiance.xyzz);
    StoreVariance(pixel_coordinate, variance.x);
}

void FFX_DNSR_Reflections_StoreTemporalAccumulation(FfxInt32x2 pixel_coordinate, FfxFloat32x3 radiance, FfxFloat32 variance)
{
    StoreRadiance(pixel_coordinate, radiance.xyzz);
    StoreVariance(pixel_coordinate, variance.x);
}

#if defined (DENOISER_BIND_SRV_AVERAGE_RADIANCE)
FfxFloat32x3 FFX_DNSR_Reflections_SampleAverageRadiance(FfxFloat32x2 uv)
{
    return (FfxFloat32x3)r_average_radiance.SampleLevel(s_LinearSampler, uv, 0.0f).xyz;
}
#endif // #if defined (DENOISER_BIND_SRV_AVERAGE_RADIANCE)

#if defined (DENOISER_BIND_SRV_RADIANCE_HISTORY)
FfxFloat32x3 FFX_DNSR_Reflections_LoadRadianceHistory(FfxInt32x2 pixel_coordinate)
{
    return (FfxFloat32x3)r_radiance_history.Load(FfxInt32x3(pixel_coordinate, 0)).xyz;
}
#endif // #if defined (DENOISER_BIND_SRV_RADIANCE_HISTORY)

#if defined (DENOISER_BIND_SRV_RADIANCE_HISTORY)
FfxFloat32x3 FFX_DNSR_Reflections_SampleRadianceHistory(FfxFloat32x2 uv)
{
    return (FfxFloat32x3)r_radiance_history.SampleLevel(s_LinearSampler, uv, 0.0f).xyz;
}
#endif // #if defined (DENOISER_BIND_SRV_RADIANCE_HISTORY)

#if defined (DENOISER_BIND_SRV_VARIANCE)
FfxFloat32 FFX_DNSR_Reflections_SampleVarianceHistory(FfxFloat32x2 uv)
{
    return (FfxFloat32)r_variance.SampleLevel(s_LinearSampler, uv, 0.0f).x;
}
#endif // #if defined (DENOISER_BIND_SRV_VARIANCE)

#if defined (DENOISER_BIND_SRV_SAMPLE_COUNT)
FfxFloat32 FFX_DNSR_Reflections_SampleNumSamplesHistory(FfxFloat32x2 uv)
{
    return (FfxFloat32)r_sample_count.SampleLevel(s_LinearSampler, uv, 0.0f).x;
}
#endif // #if defined (DENOISER_BIND_SRV_SAMPLE_COUNT)

#if defined (DENOISER_BIND_UAV_REPROJECTED_RADIANCE)
void FFX_DNSR_Reflections_StoreRadianceReprojected(FfxInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    rw_reprojected_radiance[pixel_coordinate] = value;
}
#endif // #if defined (DENOISER_BIND_UAV_REPROJECTED_RADIANCE)

#if defined (DENOISER_BIND_UAV_AVERAGE_RADIANCE)
void FFX_DNSR_Reflections_StoreAverageRadiance(FfxInt32x2 pixel_coordinate, FfxFloat32x3 value)
{
    rw_average_radiance[pixel_coordinate] = value;
}
#endif // #if defined (DENOISER_BIND_UAV_AVERAGE_RADIANCE)

FfxFloat32x3 FFX_DNSR_Reflections_LoadWorldSpaceNormal(FfxInt32x2 pixel_coordinate)
{
    return FFX_DENOISER_LoadWorldSpaceNormal(pixel_coordinate);
}

#if defined (DENOISER_BIND_SRV_ROUGHNESS_HISTORY)
FfxFloat32 FFX_DNSR_Reflections_SampleRoughnessHistory(FfxFloat32x2 uv)
{
    FfxFloat32 rawRoughness = (FfxFloat32)r_roughness_history.SampleLevel(s_LinearSampler, uv, 0.0f);
    if (IsRoughnessPerceptual())
    {
        rawRoughness *= rawRoughness;
    }

    return rawRoughness;
}
#endif // #if defined (DENOISER_BIND_SRV_ROUGHNESS_HISTORY)

#if defined (DENOISER_BIND_SRV_NORMAL_HISTORY)
FfxFloat32x3 FFX_DNSR_Reflections_LoadWorldSpaceNormalHistory(FfxInt32x2 pixel_coordinate)
{
    return normalize((FfxFloat32x3)(NormalsUnpackMul() * r_normal_history.Load(FfxInt32x3(pixel_coordinate, 0)) + NormalsUnpackAdd()));
}
#endif // #if defined (DENOISER_BIND_SRV_NORMAL_HISTORY)

#if defined (DENOISER_BIND_SRV_NORMAL_HISTORY)
FfxFloat32x3 FFX_DNSR_Reflections_SampleWorldSpaceNormalHistory(FfxFloat32x2 uv)
{
    return normalize((FfxFloat32x3)(NormalsUnpackMul() * r_normal_history.SampleLevel(s_LinearSampler, uv, 0.0f) + NormalsUnpackAdd()));
}
#endif // #if defined (DENOISER_BIND_SRV_NORMAL_HISTORY)

#if defined (DENOISER_BIND_SRV_RADIANCE)
FfxFloat32 FFX_DNSR_Reflections_LoadRayLength(FfxInt32x2 pixel_coordinate)
{
    return (FfxFloat32)r_radiance.Load(FfxInt32x3(pixel_coordinate, 0)).w;
}
#endif // #if defined (DENOISER_BIND_SRV_RADIANCE)

void FFX_DNSR_Reflections_StoreVariance(FfxInt32x2 pixel_coordinate, FfxFloat32 value)
{
    StoreVariance(pixel_coordinate, value);
}

#if defined (DENOISER_BIND_UAV_SAMPLE_COUNT)
void FFX_DNSR_Reflections_StoreNumSamples(FfxInt32x2 pixel_coordinate, FfxFloat32 value)
{
    rw_sample_count[pixel_coordinate] = value;
}
#endif // #if defined (DENOISER_BIND_UAV_SAMPLE_COUNT)

FfxFloat32x3 FFX_DNSR_Reflections_LoadRadiance(FfxInt32x2 pixel_coordinate)
{
    return LoadRadiance(FfxInt32x3(pixel_coordinate, 0));
}

#if defined (DENOISER_BIND_SRV_REPROJECTED_RADIANCE)
FfxFloat32x3 FFX_DNSR_Reflections_LoadRadianceReprojected(FfxInt32x2 pixel_coordinate)
{
    return (FfxFloat32x3)r_reprojected_radiance.Load(FfxInt32x3(pixel_coordinate, 0)).xyz;
}
#endif // #if defined (DENOISER_BIND_SRV_REPROJECTED_RADIANCE)

FfxFloat32 FFX_DNSR_Reflections_LoadVariance(FfxInt32x2 pixel_coordinate)
{
    return LoadVariance(FfxInt32x3(pixel_coordinate, 0));
}

#if defined (DENOISER_BIND_SRV_SAMPLE_COUNT)
FfxFloat32 FFX_DNSR_Reflections_LoadNumSamples(FfxInt32x2 pixel_coordinate)
{
    return (FfxFloat32)r_sample_count.Load(FfxInt32x3(pixel_coordinate, 0)).x;
}
#endif // #if defined (DENOISER_BIND_SRV_SAMPLE_COUNT)

#endif // #if defined(FFX_HALF)

FfxFloat32 FFX_DENOISER_LoadDepth(FfxInt32x2 pixel_coordinate, FfxInt32 mip)
{
#if defined(DENOISER_BIND_SRV_INPUT_DEPTH_HIERARCHY)
    return r_input_depth_hierarchy.Load(FfxInt32x3(pixel_coordinate, mip));
#else
    return 0.0f;
#endif
}

#if defined (DENOISER_BIND_UAV_DENOISER_TILE_LIST)
FfxUInt32 GetDenoiserTile(FfxUInt32 group_id)
{
    return rw_denoiser_tile_list[group_id];
}
#endif // #if defined (DENOISER_BIND_UAV_DENOISER_TILE_LIST)

#if defined (DENOISER_BIND_SRV_INPUT_MOTION_VECTORS)
FfxFloat32x2 FFX_DNSR_Reflections_LoadMotionVector(FfxInt32x2 pixel_coordinate)
{
    return MotionVectorScale() * r_input_motion_vectors.Load(FfxInt32x3(pixel_coordinate, 0));
}
#endif // #if defined (DENOISER_BIND_SRV_INPUT_MOTION_VECTORS)

FfxFloat32 FFX_DNSR_Reflections_LoadDepth(FfxInt32x2 pixel_coordinate)
{
    return FFX_DENOISER_LoadDepth(pixel_coordinate, 0);
}

#if defined (DENOISER_BIND_SRV_DEPTH_HISTORY)
FfxFloat32 FFX_DNSR_Reflections_LoadDepthHistory(FfxInt32x2 pixel_coordinate)
{
    return r_depth_history.Load(FfxInt32x3(pixel_coordinate, 0));
}
#endif // #if defined (DENOISER_BIND_SRV_DEPTH_HISTORY)

#if defined (DENOISER_BIND_SRV_DEPTH_HISTORY)
FfxFloat32 FFX_DNSR_Reflections_SampleDepthHistory(FfxFloat32x2 uv)
{
    return r_depth_history.SampleLevel(s_LinearSampler, uv, 0.0f);
}
#endif // #if defined (DENOISER_BIND_SRV_DEPTH_HISTORY)

#endif // #if defined(FFX_GPU)
