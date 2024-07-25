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

#include "ffx_dof_resources.h"
#include "ffx_core.h"
#include "ffx_dof_common.h"

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
#define FFX_DOF_DECLARE_SRV(regIndex)   register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_DOF_DECLARE_UAV(regIndex)   register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_DOF_DECLARE_CB(regIndex)    register(DECLARE_CB_REGISTER(regIndex))

#if defined(FFX_DOF_BIND_CB_DOF)
    cbuffer cbDOF : FFX_DOF_DECLARE_CB(FFX_DOF_BIND_CB_DOF)
    {
        FfxFloat32   cocScale;
        FfxFloat32   cocBias;
        FfxUInt32x2  inputSizeHalf;
        FfxUInt32x2  inputSize;
        FfxFloat32x2 inputSizeHalfRcp;
        FfxFloat32   cocLimit;
        FfxUInt32    maxRings;
    #define FFX_DOF_CONSTANT_BUFFER_1_SIZE 10  // Number of 32-bit values. This must be kept in sync with the cbDOF size.
    };
#else
#define cocScale 0
#define cocBias 0
#define inputSizeHalf 0
#define inputSize 0
#define inputSizeHalfRcp 0
#define cocLimit 0
#define maxRings 0
#endif

#define FFX_DOF_ROOTSIG_STRINGIFY(p) FFX_DOF_ROOTSIG_STR(p)
#define FFX_DOF_ROOTSIG_STR(p) #p
#define FFX_DOF_ROOTSIG [RootSignature("DescriptorTable(UAV(u0, numDescriptors = " FFX_DOF_ROOTSIG_STRINGIFY(FFX_DOF_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "DescriptorTable(SRV(t0, numDescriptors = " FFX_DOF_ROOTSIG_STRINGIFY(FFX_DOF_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "CBV(b0), " \
                                    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                                                      "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                      "comparisonFunc = COMPARISON_NEVER, " \
                                                      "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK), " \
                                    "StaticSampler(s1, filter = FILTER_MIN_MAG_MIP_POINT, " \
                                                      "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                      "comparisonFunc = COMPARISON_NEVER, " \
                                                      "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK)" )]

#if defined(FFX_DOF_EMBED_ROOTSIG)
#define FFX_DOF_EMBED_ROOTSIG_CONTENT FFX_DOF_ROOTSIG
#else
#define FFX_DOF_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_SPD_EMBED_ROOTSIG

FfxFloat32 CocScale()
{
    return cocScale;
}

FfxFloat32 CocBias()
{
    return cocBias;
}

FfxUInt32x2 InputSizeHalf()
{
    return inputSizeHalf;
}

FfxUInt32x2 InputSize()
{
    return inputSize;
}

FfxFloat32x2 InputSizeHalfRcp()
{
    return inputSizeHalfRcp;
}

FfxFloat32 CocLimit()
{
    return cocLimit;
}

FfxUInt32 MaxRings()
{
    return maxRings;
}

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

// SRVs
#if defined(FFX_DOF_BIND_SRV_INPUT_DEPTH)
    Texture2D<FfxFloat32>     r_input_depth                 : FFX_DOF_DECLARE_SRV(FFX_DOF_BIND_SRV_INPUT_DEPTH);
#endif
#if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)
    Texture2D<FfxFloat32x4>   r_input_color                 : FFX_DOF_DECLARE_SRV(FFX_DOF_BIND_SRV_INPUT_COLOR);
#endif
#if defined(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)
    Texture2D<FfxHalfOpt4>   r_internal_bilat_color        : FFX_DOF_DECLARE_SRV(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR);
#endif
#if defined(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)
    Texture2D<FfxFloat32x2>   r_internal_dilated_radius     : FFX_DOF_DECLARE_SRV(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS);
#endif

// UAVs
#if defined(FFX_DOF_BIND_UAV_INTERNAL_BILAT_COLOR)
    RWTexture2D<FfxHalfOpt4>  rw_internal_bilat_color[4]    : FFX_DOF_DECLARE_UAV(FFX_DOF_BIND_UAV_INTERNAL_BILAT_COLOR);
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_RADIUS)
    RWTexture2D<FfxFloat32x2> rw_internal_radius            : FFX_DOF_DECLARE_UAV(FFX_DOF_BIND_UAV_INTERNAL_RADIUS);
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_DILATED_RADIUS)
    RWTexture2D<FfxFloat32x2> rw_internal_dilated_radius    : FFX_DOF_DECLARE_UAV(FFX_DOF_BIND_UAV_INTERNAL_DILATED_RADIUS);
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_NEAR)
    RWTexture2D<FfxHalfOpt4>  rw_internal_near              : FFX_DOF_DECLARE_UAV(FFX_DOF_BIND_UAV_INTERNAL_NEAR);
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_FAR)
    RWTexture2D<FfxHalfOpt4>  rw_internal_far               : FFX_DOF_DECLARE_UAV(FFX_DOF_BIND_UAV_INTERNAL_FAR);
#endif
#if defined(FFX_DOF_BIND_UAV_OUTPUT_COLOR)
    RWTexture2D<FfxFloat32x4> rw_output_color               : FFX_DOF_DECLARE_UAV(FFX_DOF_BIND_UAV_OUTPUT_COLOR);
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)
    struct DofGlobalVars { uint maxTileRad; };
    globallycoherent RWStructuredBuffer<DofGlobalVars> rw_internal_globals : FFX_DOF_DECLARE_UAV(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS);
#endif

#if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)
FfxHalfOpt4 FfxDofLoadSource(FfxInt32x2 tex)
{
    return FfxHalfOpt4(r_input_color[tex]);
}
#endif // #if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_BILAT_COLOR)
void FfxDofStoreBilatMip(FfxUInt32 mip, FfxInt32x2 tex, FfxHalfOpt4 value)
{
    rw_internal_bilat_color[mip][tex] = value;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_BILAT_COLOR)

FfxFloat32 FfxDofGetCoc(FfxFloat32 depth)
{
    // clamped for perf reasons
    return clamp(CocScale() * depth + CocBias(), -CocLimit(), CocLimit());
}

#if defined(FFX_DOF_BIND_UAV_INTERNAL_DILATED_RADIUS)
void FfxDofStoreDilatedRadius(FfxUInt32x2 coord, FfxFloat32x2 dilatedMinMax)
{
    rw_internal_dilated_radius[coord] = dilatedMinMax;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_DILATED_RADIUS)

#if defined(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)
FfxFloat32x2 FfxDofSampleDilatedRadius(FfxUInt32x2 coord)
{
    return r_internal_dilated_radius.SampleLevel(LinearSampler, float2(coord) * InputSizeHalfRcp(), 0);
}
#endif // #if defined(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)

#if defined(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)
FfxFloat32x2 FfxDofLoadDilatedRadius(FfxUInt32x2 coord)
{
    return r_internal_dilated_radius[coord];
}
#endif // #if defined(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)

#if defined(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)
FfxHalfOpt4 FfxDofLoadInput(FfxUInt32x2 coord)
{
    return r_internal_bilat_color[coord];
}
#endif // #if defined(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)

#if defined(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)
FfxHalfOpt4 FfxDofSampleInput(FfxFloat32x2 coord, FfxUInt32 mip)
{
    return r_internal_bilat_color.SampleLevel(PointSampler, coord, mip);
}
#endif // #if defined(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)

#if defined(FFX_DOF_BIND_SRV_INPUT_DEPTH)
FfxFloat32x4 FfxDofGatherDepth(FfxFloat32x2 coord)
{
    return r_input_depth.GatherRed(LinearSampler, coord);
}
#endif // #if defined(FFX_DOF_BIND_SRV_INPUT_DEPTH)

#if defined(FFX_DOF_BIND_SRV_INPUT_DEPTH)
FfxFloat32 FfxDofLoadFullDepth(FfxUInt32x2 coord)
{
    return r_input_depth[coord];
}
#endif // #if defined(FFX_DOF_BIND_SRV_INPUT_DEPTH)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_NEAR)
void FfxDofStoreNear(FfxUInt32x2 coord, FfxHalfOpt4 color)
{
    rw_internal_near[coord] = color;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_NEAR)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_NEAR)
FfxHalfOpt4 FfxDofLoadNear(FfxUInt32x2 coord)
{
    return rw_internal_near[coord];
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_NEAR)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_FAR)
void FfxDofStoreFar(FfxUInt32x2 coord, FfxHalfOpt4 color)
{
    rw_internal_far[coord] = color;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_FAR)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_FAR)
FfxHalfOpt4 FfxDofLoadFar(FfxUInt32x2 coord)
{
    return rw_internal_far[coord];
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_FAR)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)
void FfxDofAccumMaxTileRadius(FfxUInt32 radius)
{
    uint dummy;
    InterlockedMax(rw_internal_globals[0].maxTileRad, radius, dummy);
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)
FfxUInt32 FfxDofGetMaxTileRadius()
{
    return rw_internal_globals[0].maxTileRad;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)
void FfxDofResetMaxTileRadius()
{
    rw_internal_globals[0].maxTileRad = 0;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_RADIUS)
void FfxDofStoreTileRadius(FfxUInt32x2 tile, FfxFloat32x2 radius)
{
    rw_internal_radius[tile] = radius;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_RADIUS)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_RADIUS)
FfxFloat32x2 FfxDofLoadTileRadius(FfxUInt32x2 tile)
{
    return rw_internal_radius[tile];
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_RADIUS)

FfxFloat32x4 FfxDofLoadFullInput(FfxUInt32x2 coord)
{
#if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)
    return r_input_color[coord];
#elif (defined(FFX_DOF_BIND_UAV_OUTPUT_COLOR)) && FFX_DOF_OPTION_COMBINE_IN_PLACE
    return rw_output_color[coord];
#endif // #if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)
    return 0;
}

#if defined(FFX_DOF_BIND_UAV_OUTPUT_COLOR)
void FfxDofStoreOutput(FfxUInt32x2 coord, FfxFloat32x4 color)
{
    rw_output_color[coord] = color;
}
#endif // #if defined(FFX_DOF_BIND_UAV_OUTPUT_COLOR)

#endif // #if defined(FFX_GPU)
