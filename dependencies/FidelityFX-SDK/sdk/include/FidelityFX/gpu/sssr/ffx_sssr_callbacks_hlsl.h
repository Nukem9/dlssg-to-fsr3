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

#include "ffx_sssr_resources.h"

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
#define FFX_SSSR_DECLARE_SRV(regIndex)  register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_SSSR_DECLARE_UAV(regIndex)  register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_SSSR_DECLARE_CB(regIndex)   register(DECLARE_CB_REGISTER(regIndex))

#if defined(SSSR_BIND_CB_SSSR)
    cbuffer cbSSSR : FFX_SSSR_DECLARE_CB(SSSR_BIND_CB_SSSR)
    {
        FfxFloat32Mat4  invViewProjection;
        FfxFloat32Mat4  projection;
        FfxFloat32Mat4  invProjection;
        FfxFloat32Mat4  viewMatrix;
        FfxFloat32Mat4  invView;
        FfxFloat32Mat4  prevViewProjection;
        FfxUInt32x2     renderSize;
        FfxFloat32x2    inverseRenderSize;
        FfxFloat32      normalsUnpackMul;
        FfxFloat32      normalsUnpackAdd;
        FfxUInt32       roughnessChannel;
        FfxBoolean      isRoughnessPerceptual;
        FfxFloat32      iblFactor;
        FfxFloat32      temporalStabilityFactor;
        FfxFloat32      depthBufferThickness;
        FfxFloat32      roughnessThreshold;
        FfxFloat32      varianceThreshold;
        FfxUInt32       frameIndex;
        FfxUInt32       maxTraversalIntersections;
        FfxUInt32       minTraversalOccupancy;
        FfxUInt32       mostDetailedMip;
        FfxUInt32       samplesPerQuad;
        FfxUInt32       temporalVarianceGuidedTracingEnabled;

       #define FFX_SSSR_CONSTANT_BUFFER_1_SIZE 110  // Number of 32-bit values. This must be kept in sync with the cbSSSR size.
    };
#else
    #define invViewProjection                           0
    #define projection                                  0
    #define invProjection                               0
    #define viewMatrix                                  0
    #define invView                                     0
    #define prevViewProjection                          0
    #define normalsUnpackMul                            0
    #define normalsUnpackAdd                            0
    #define roughnessChannel                            0
    #define isRoughnessPerceptual                       0
    #define renderSize                                  0
    #define inverseRenderSize                           0
    #define iblFactor                                   0
    #define temporalStabilityFactor                     0
    #define depthBufferThickness                        0
    #define roughnessThreshold                          0
    #define varianceThreshold                           0
    #define frameIndex                                  0
    #define maxTraversalIntersections                   0
    #define minTraversalOccupancy                       0
    #define mostDetailedMip                             0
    #define samplesPerQuad                              0
    #define temporalVarianceGuidedTracingEnabled        0

    #define FFX_SSSR_CONSTANT_BUFFER_1_SIZE             0
#endif

#if defined(FFX_GPU)
#define FFX_SSSR_ROOTSIG_STRINGIFY(p) FFX_SSSR_ROOTSIG_STR(p)
#define FFX_SSSR_ROOTSIG_STR(p) #p
#define FFX_SSSR_ROOTSIG [RootSignature(    "DescriptorTable(UAV(u0, numDescriptors = " FFX_SSSR_ROOTSIG_STRINGIFY(FFX_SSSR_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                            "DescriptorTable(SRV(t0, numDescriptors = " FFX_SSSR_ROOTSIG_STRINGIFY(FFX_SSSR_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                            "CBV(b0), " \
                                            "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                                                "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                "addressW = TEXTURE_ADDRESS_WRAP, " \
                                                "comparisonFunc = COMPARISON_ALWAYS, " \
                                                "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK, " \
                                                "maxAnisotropy = 1, " \
                                                "visibility = SHADER_VISIBILITY_ALL) " )]

#if defined(FFX_SSSR_EMBED_ROOTSIG)
#define FFX_SSSR_EMBED_ROOTSIG_CONTENT FFX_SSSR_ROOTSIG
#else
#define FFX_SSSR_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_SSSR_EMBED_ROOTSIG

#endif // #if defined(FFX_GPU)

SamplerState s_EnvironmentMapSampler : register(s0);

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
    return isRoughnessPerceptual;
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

FfxFloat32 TemporalStabilityFactor()
{
    return temporalStabilityFactor;
}

FfxFloat32 DepthBufferThickness()
{
    return depthBufferThickness;
}

FfxFloat32 RoughnessThreshold()
{
    return roughnessThreshold;
}

FfxFloat32 VarianceThreshold()
{
    return varianceThreshold;
}

FfxUInt32 FrameIndex()
{
    return frameIndex;
}

FfxUInt32 MaxTraversalIntersections()
{
    return maxTraversalIntersections;
}

FfxUInt32 MinTraversalOccupancy()
{
    return minTraversalOccupancy;
}

FfxUInt32 MostDetailedMip()
{
    return mostDetailedMip;
}

FfxUInt32 SamplesPerQuad()
{
    return samplesPerQuad;
}

FfxBoolean TemporalVarianceGuidedTracingEnabled()
{
    return FfxBoolean(temporalVarianceGuidedTracingEnabled);
}

// SRVs
    #if defined SSSR_BIND_SRV_INPUT_COLOR
        Texture2D<FfxFloat32x4>     r_input_color                   : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_INPUT_COLOR);
    #endif
    #if defined SSSR_BIND_SRV_INPUT_DEPTH
        Texture2D<FfxFloat32>       r_input_depth                   : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_INPUT_DEPTH);
    #endif
    #if defined SSSR_BIND_SRV_DEPTH_HIERARCHY
        Texture2D<FfxFloat32>       r_depth_hierarchy               : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_DEPTH_HIERARCHY);
    #endif
    #if defined SSSR_BIND_SRV_INPUT_NORMAL
        Texture2D<FfxFloat32x3>     r_input_normal                  : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_INPUT_NORMAL);
    #endif
    #if defined SSSR_BIND_SRV_INPUT_MATERIAL_PARAMETERS
        Texture2D<FfxFloat32x4>     r_input_material_parameters     : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_INPUT_MATERIAL_PARAMETERS);
    #endif
    #if defined SSSR_BIND_SRV_INPUT_ENVIRONMENT_MAP
        TextureCube                 r_input_environment_map         : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_INPUT_ENVIRONMENT_MAP);
    #endif
    #if defined SSSR_BIND_SRV_RADIANCE
        Texture2D<FfxFloat32x4>     r_radiance                      : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_RADIANCE);
    #endif
    #if defined SSSR_BIND_SRV_RADIANCE_HISTORY
        Texture2D<FfxFloat32x4>     r_radiance_history              : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_RADIANCE_HISTORY);
    #endif
    #if defined SSSR_BIND_SRV_VARIANCE
        Texture2D<FfxFloat32>       r_variance                      : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_VARIANCE);
    #endif
    #if defined SSSR_BIND_SRV_EXTRACTED_ROUGHNESS
        Texture2D<FfxFloat32>       r_extracted_roughness           : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_EXTRACTED_ROUGHNESS);
    #endif
    #if defined SSSR_BIND_SRV_SOBOL_BUFFER
        Texture2D<FfxUInt32>        r_sobol_buffer                  : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_SOBOL_BUFFER);
    #endif
    #if defined SSSR_BIND_SRV_SCRAMBLING_TILE_BUFFER
        Texture2D<FfxUInt32>        r_scrambling_tile_buffer        : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_SCRAMBLING_TILE_BUFFER);
    #endif
    #if defined SSSR_BIND_SRV_BLUE_NOISE_TEXTURE
        Texture2D<FfxFloat32x2>     r_blue_noise_texture            : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_BLUE_NOISE_TEXTURE);
    #endif
    #if defined SSSR_BIND_SRV_INPUT_BRDF_TEXTURE
        Texture2D<FfxFloat32x4>     r_input_brdf_texture            : FFX_SSSR_DECLARE_SRV(SSSR_BIND_SRV_INPUT_BRDF_TEXTURE);
    #endif

// UAVs
    #if defined SSSR_BIND_UAV_OUTPUT
            RWTexture2D<FfxFloat32x4>                       rw_output                           : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_OUTPUT);
    #endif
    #if defined SSSR_BIND_UAV_RADIANCE
            RWTexture2D<FfxFloat32x4>                       rw_radiance                         : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_RADIANCE);
    #endif
    #if defined SSSR_BIND_UAV_VARIANCE
            RWTexture2D<FfxFloat32>                         rw_variance                         : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_VARIANCE);
    #endif
    #if defined SSSR_BIND_UAV_RAY_LIST
            RWStructuredBuffer<FfxUInt32>                   rw_ray_list                         : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_RAY_LIST);
    #endif
    #if defined SSSR_BIND_UAV_DENOISER_TILE_LIST
            RWStructuredBuffer<FfxUInt32>                   rw_denoiser_tile_list               : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_DENOISER_TILE_LIST);
    #endif
    #if defined SSSR_BIND_UAV_RAY_COUNTER
            globallycoherent RWStructuredBuffer<FfxUInt32>  rw_ray_counter                      : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_RAY_COUNTER);
    #endif
    #if defined SSSR_BIND_UAV_INTERSECTION_PASS_INDIRECT_ARGS
            RWStructuredBuffer<FfxUInt32>                   rw_intersection_pass_indirect_args  : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_INTERSECTION_PASS_INDIRECT_ARGS);
    #endif
    #if defined SSSR_BIND_UAV_EXTRACTED_ROUGHNESS
            RWTexture2D<FfxFloat32>                         rw_extracted_roughness              : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_EXTRACTED_ROUGHNESS);
    #endif
    #if defined SSSR_BIND_UAV_BLUE_NOISE_TEXTURE
            RWTexture2D<FfxFloat32x2>                       rw_blue_noise_texture               : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_BLUE_NOISE_TEXTURE);
    #endif
    #if defined SSSR_BIND_UAV_DEPTH_HIERARCHY
            RWTexture2D<FfxFloat32>                         rw_depth_hierarchy[13]        : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_DEPTH_HIERARCHY);
    #endif

    // SPD UAV
    #if defined SSSR_BIND_UAV_SPD_GLOBAL_ATOMIC
            globallycoherent RWStructuredBuffer<FfxUInt32>  rw_spd_global_atomic                : FFX_SSSR_DECLARE_UAV(SSSR_BIND_UAV_SPD_GLOBAL_ATOMIC);
    #endif

#if FFX_HALF

#if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)
FfxFloat16x4 SpdLoadH(FfxInt32x2 coordinate, FfxUInt32 slice)
{ 
    return (FfxFloat16x4)rw_depth_hierarchy[6][coordinate].xxxx;   // 5 -> 6 as we store a copy of the depth buffer at index 0
}
#endif // #if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)

#if defined (SSSR_BIND_SRV_INPUT_DEPTH)
FfxFloat16x4 SpdLoadSourceImageH(FfxInt32x2 coordinate, FfxUInt32 slice)
{
    return (FfxFloat16x4)r_input_depth[coordinate].xxxx;
}
#endif // #if defined (SSSR_BIND_SRV_INPUT_DEPTH)

#if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)
void SpdStoreH(FfxInt32x2 pix, FfxFloat16x4 outValue, FfxUInt32 coordinate, FfxUInt32 slice)
{
    rw_depth_hierarchy[coordinate + 1][pix] = outValue.x;    // + 1 as we store a copy of the depth buffer at index 0
}
#endif // #if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)

#endif // #if defined(FFX_HALF)

#if defined(SSSR_BIND_SRV_INPUT_NORMAL) 
FfxFloat32x3 FFX_SSSR_LoadWorldSpaceNormal(FfxInt32x2 pixel_coordinate)
{
    return normalize(NormalsUnpackMul() * r_input_normal.Load(FfxInt32x3(pixel_coordinate, 0)).xyz + NormalsUnpackAdd());
}
#endif // #if defined(SSSR_BIND_SRV_INPUT_NORMAL)

#if defined(SSSR_BIND_SRV_DEPTH_HIERARCHY)
FfxFloat32 FFX_SSSR_LoadDepth(FfxInt32x2 pixel_coordinate, FfxInt32 mip)
{
    return r_depth_hierarchy.Load(FfxInt32x3(pixel_coordinate, mip));
}
#endif // #if defined(SSSR_BIND_SRV_DEPTH_HIERARCHY)

#if defined(SSSR_BIND_SRV_BLUE_NOISE_TEXTURE)
FfxFloat32x2 FFX_SSSR_SampleRandomVector2D(FfxUInt32x2 pixel)
{
    return r_blue_noise_texture.Load(FfxInt32x3(pixel.xy % 128, 0));
}
#endif // #if defined(SSSR_BIND_SRV_BLUE_NOISE_TEXTURE)

#if defined(SSSR_BIND_SRV_INPUT_ENVIRONMENT_MAP)
FfxFloat32x3 FFX_SSSR_SampleEnvironmentMap(FfxFloat32x3 direction, FfxFloat32 preceptualRoughness)
{
    FfxFloat32 Width; FfxFloat32 Height;
    r_input_environment_map.GetDimensions(Width, Height);
    FfxInt32 maxMipLevel = FfxInt32(log2(FfxFloat32(Width > 0 ? Width : 1)));
    FfxFloat32 mip = clamp(preceptualRoughness * FfxFloat32(maxMipLevel), 0.0, FfxFloat32(maxMipLevel));
    return r_input_environment_map.SampleLevel(s_EnvironmentMapSampler, direction, mip).xyz * IBLFactor();
}
#endif // #if defined(SSSR_BIND_SRV_INPUT_ENVIRONMENT_MAP)

#if defined (SSSR_BIND_UAV_RAY_COUNTER)
void IncrementRayCounter(FfxUInt32 value, FFX_PARAMETER_OUT FfxUInt32 original_value)
{
    InterlockedAdd(rw_ray_counter[0], value, original_value);
}
#endif // #if defined (SSSR_BIND_UAV_RAY_COUNTER)

#if defined (SSSR_BIND_UAV_RAY_COUNTER)
void IncrementDenoiserTileCounter(FFX_PARAMETER_OUT FfxUInt32 original_value)
{
    InterlockedAdd(rw_ray_counter[2], 1, original_value);
}
#endif // #if defined (SSSR_BIND_UAV_RAY_COUNTER)

FfxUInt32 PackRayCoords(FfxUInt32x2 ray_coord, FfxBoolean copy_horizontal, FfxBoolean copy_vertical, FfxBoolean copy_diagonal)
{
    FfxUInt32 ray_x_15bit = ray_coord.x & 0b111111111111111;
    FfxUInt32 ray_y_14bit = ray_coord.y & 0b11111111111111;
    FfxUInt32 copy_horizontal_1bit = copy_horizontal ? 1 : 0;
    FfxUInt32 copy_vertical_1bit = copy_vertical ? 1 : 0;
    FfxUInt32 copy_diagonal_1bit = copy_diagonal ? 1 : 0;

    FfxUInt32 packed = (copy_diagonal_1bit << 31) | (copy_vertical_1bit << 30) | (copy_horizontal_1bit << 29) | (ray_y_14bit << 15) | (ray_x_15bit << 0);
    return packed;
}

#if defined (SSSR_BIND_UAV_RAY_LIST)
void StoreRay(FfxInt32 index, FfxUInt32x2 ray_coord, FfxBoolean copy_horizontal, FfxBoolean copy_vertical, FfxBoolean copy_diagonal)
{
    FfxUInt32 packedRayCoords = PackRayCoords(ray_coord, copy_horizontal, copy_vertical, copy_diagonal); // Store out pixel to trace
    rw_ray_list[index] = packedRayCoords;
}
#endif // #if defined (SSSR_BIND_UAV_RAY_LIST)

#if defined (SSSR_BIND_UAV_DENOISER_TILE_LIST)
void StoreDenoiserTile(FfxInt32 index, FfxUInt32x2 tile_coord)
{ 
    rw_denoiser_tile_list[index] = ((tile_coord.y & 0xffffu) << 16) | ((tile_coord.x & 0xffffu) << 0); // Store out pixel to trace
}
#endif // #if defined (SSSR_BIND_UAV_DENOISER_TILE_LIST)

#if defined (SSSR_BIND_SRV_DEPTH_HIERARCHY)
FfxBoolean IsReflectiveSurface(FfxInt32x2 pixel_coordinate, FfxFloat32 roughness)
{
#if FFX_SSSR_OPTION_INVERTED_DEPTH
    const FfxFloat32 far_plane = 0.0f;
    return r_depth_hierarchy[pixel_coordinate] > far_plane;
#else //  FFX_SSSR_OPTION_INVERTED_DEPTH
    const FfxFloat32 far_plane = 1.0f;
    return r_depth_hierarchy[pixel_coordinate] < far_plane;
#endif //  FFX_SSSR_OPTION_INVERTED_DEPTH
}
#endif // #if defined (SSSR_BIND_SRV_DEPTH_HIERARCHY)

#if defined (SSSR_BIND_UAV_EXTRACTED_ROUGHNESS)
void StoreExtractedRoughness(FfxUInt32x2 coordinate, FfxFloat32 roughness)
{
    rw_extracted_roughness[coordinate] = roughness;
}
#endif // #if defined (SSSR_BIND_UAV_EXTRACTED_ROUGHNESS)

#if defined (SSSR_BIND_SRV_INPUT_MATERIAL_PARAMETERS)
FfxFloat32 LoadRoughnessFromMaterialParametersInput(FfxUInt32x3 coordinate)
{
    FfxFloat32 rawRoughness = r_input_material_parameters.Load(coordinate)[RoughnessChannel()];
    if (IsRoughnessPerceptual())
    {
        rawRoughness *= rawRoughness;
    }
        
    return rawRoughness;
}
#endif // #if defined (SSSR_BIND_SRV_INPUT_MATERIAL_PARAMETERS)

#if defined (SSSR_BIND_UAV_RAY_COUNTER)
FfxBoolean IsRayIndexValid(FfxUInt32 ray_index)
{
    return ray_index < rw_ray_counter[1];
}
#endif // #if defined (SSSR_BIND_UAV_RAY_COUNTER)

#if defined (SSSR_BIND_UAV_RAY_LIST)
FfxUInt32 GetRaylist(FfxUInt32 ray_index)
{
    return rw_ray_list[ray_index];
}
#endif // #if defined (SSSR_BIND_UAV_RAY_LIST)

#if defined (SSSR_BIND_UAV_BLUE_NOISE_TEXTURE)
void FFX_SSSR_StoreBlueNoiseSample(FfxUInt32x2 coordinate, FfxFloat32x2 blue_noise_sample)
{
    rw_blue_noise_texture[coordinate] = blue_noise_sample;
}
#endif // #if defined (SSSR_BIND_UAV_BLUE_NOISE_TEXTURE)

#if defined (SSSR_BIND_SRV_VARIANCE)
FfxFloat32 FFX_SSSR_LoadVarianceHistory(FfxInt32x3 coordinate)
{
    return r_variance.Load(coordinate).x;
}
#endif // #if defined (SSSR_BIND_SRV_VARIANCE)

#if defined (SSSR_BIND_UAV_RADIANCE)
void FFX_SSSR_StoreRadiance(FfxUInt32x2 coordinate, FfxFloat32x4 radiance)
{
    rw_radiance[coordinate] = radiance;
}
#endif // #if defined (SSSR_BIND_UAV_RADIANCE)

#if defined (SSSR_BIND_SRV_SOBOL_BUFFER)
FfxUInt32 FFX_SSSR_GetSobolSample(FfxUInt32x3 coordinate)
{
    return r_sobol_buffer.Load(coordinate);
}
#endif // #if defined (SSSR_BIND_SRV_SOBOL_BUFFER)

#if defined (SSSR_BIND_SRV_SCRAMBLING_TILE_BUFFER)
FfxUInt32 FFX_SSSR_GetScramblingTile(FfxUInt32x3 coordinate)
{ 
    return r_scrambling_tile_buffer.Load(coordinate);
}
#endif // #if defined (SSSR_BIND_SRV_SCRAMBLING_TILE_BUFFER)

#if defined (SSSR_BIND_UAV_INTERSECTION_PASS_INDIRECT_ARGS)
void FFX_SSSR_WriteIntersectIndirectArgs(FfxUInt32 index, FfxUInt32 data)
{
    rw_intersection_pass_indirect_args[index] = data;
}
#endif // #if defined (SSSR_BIND_UAV_INTERSECTION_PASS_INDIRECT_ARGS)

#if defined (SSSR_BIND_UAV_RAY_COUNTER)
void FFX_SSSR_WriteRayCounter(FfxUInt32 index, FfxUInt32 data)
{
    rw_ray_counter[index] = data;
}
#endif // #if defined (SSSR_BIND_UAV_RAY_COUNTER)

#if defined (SSSR_BIND_UAV_RAY_COUNTER)
FfxUInt32 FFX_SSSR_GetRayCounter(FfxUInt32 index)
{
    return rw_ray_counter[index];
}
#endif // #if defined (SSSR_BIND_UAV_RAY_COUNTER)

#if defined (SSSR_BIND_SRV_INPUT_DEPTH)
void FFX_SSSR_GetInputDepthDimensions(FFX_PARAMETER_OUT FfxFloat32x2 image_size)
{
    r_input_depth.GetDimensions(image_size.x, image_size.y);
}
#endif // #if defined (SSSR_BIND_SRV_INPUT_DEPTH)

#if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)
void FFX_SSSR_GetDepthHierarchyMipDimensions(FfxUInt32 mip, FFX_PARAMETER_OUT FfxFloat32x2 image_size)
{
    rw_depth_hierarchy[mip].GetDimensions(image_size.x, image_size.y);
}
#endif // #if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)

#if defined (SSSR_BIND_SRV_INPUT_DEPTH)
FfxFloat32 FFX_SSSR_GetInputDepth(FfxUInt32x2 coordinate)
{
    return r_input_depth[coordinate];
}

FfxFloat32x4 SpdLoadSourceImage(FfxInt32x2 coordinate, FfxUInt32 slice)
{
    return FFX_SSSR_GetInputDepth(coordinate).xxxx;
}
#endif // #if defined (SSSR_BIND_SRV_INPUT_DEPTH)

#if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)
void FFX_SSSR_WriteDepthHierarchy(FfxUInt32 index, FfxUInt32x2 coordinate, FfxFloat32 data)
{
    rw_depth_hierarchy[index][coordinate] = data;
}
#endif // #if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)

#if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)
FfxFloat32x4 SpdLoad(FfxInt32x2 coordinate, FfxUInt32 slice)
{
    return rw_depth_hierarchy[6][coordinate].xxxx;   // 5 -> 6 as we store a copy of the depth buffer at index 0
}
#endif // #if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)

#if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)
void SpdStore(FfxInt32x2 pix, FfxFloat32x4 outValue, FfxUInt32 coordinate, FfxUInt32 slice)
{
    rw_depth_hierarchy[coordinate + 1][pix] = outValue.x;    // + 1 as we store a copy of the depth buffer at index 0
}
#endif // #if defined (SSSR_BIND_UAV_DEPTH_HIERARCHY)

#if defined (SSSR_BIND_UAV_SPD_GLOBAL_ATOMIC)
void SpdResetAtomicCounter(FfxUInt32 slice)
{
    rw_spd_global_atomic[0] = 0;
}
#endif // #if defined (SSSR_BIND_UAV_SPD_GLOBAL_ATOMIC)

#if defined (SSSR_BIND_UAV_SPD_GLOBAL_ATOMIC)
void FFX_SSSR_SPDIncreaseAtomicCounter(FFX_PARAMETER_INOUT FfxUInt32 spdCounter)
{
    InterlockedAdd(rw_spd_global_atomic[0], 1, spdCounter);
}
#endif // #if defined (SSSR_BIND_UAV_SPD_GLOBAL_ATOMIC)

#if defined(SSSR_BIND_SRV_INPUT_COLOR)
FfxFloat32x3 FFX_SSSR_LoadInputColor(FfxInt32x3 coordinate)
{
    return r_input_color.Load(coordinate).xyz;
}
#endif // #if defined(SSSR_BIND_SRV_INPUT_COLOR)

#if defined(SSSR_BIND_SRV_EXTRACTED_ROUGHNESS)
FfxFloat32 FFX_SSSR_LoadExtractedRoughness(FfxInt32x3 coordinate)
{
    return r_extracted_roughness.Load(coordinate).x;
}
#endif // #if defined(SSSR_BIND_SRV_EXTRACTED_ROUGHNESS)

#endif // #if defined(FFX_GPU)
