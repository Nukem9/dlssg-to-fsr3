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

#if defined(FFX_GPU)
#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#if defined(FFX_DOF_BIND_CB_DOF)
    layout (set = 0, binding = FFX_DOF_BIND_CB_DOF, std140) uniform cbDOF_t
    {
        FfxFloat32   cocScale;
        FfxFloat32   cocBias;
        FfxUInt32x2  inputSizeHalf;
        FfxUInt32x2  inputSize;
        FfxFloat32x2 inputSizeHalfRcp;
        FfxFloat32   cocLimit;
        FfxUInt32    maxRings;
    #define FFX_DOF_CONSTANT_BUFFER_1_SIZE 10  // Number of 32-bit values. This must be kept in sync with the cbDOF size.
    } cbDOF;
#endif

FfxFloat32 CocScale()
{
    return cbDOF.cocScale;
}

FfxFloat32 CocBias()
{
    return cbDOF.cocBias;
}

FfxUInt32x2 InputSizeHalf()
{
    return cbDOF.inputSizeHalf;
}

FfxUInt32x2 InputSize()
{
    return cbDOF.inputSize;
}

FfxFloat32x2 InputSizeHalfRcp()
{
    return cbDOF.inputSizeHalfRcp;
}

FfxFloat32 CocLimit()
{
    return cbDOF.cocLimit;
}

FfxUInt32 MaxRings()
{
    return cbDOF.maxRings;
}

layout (set = 0, binding = 1000)
uniform sampler LinearSampler;
layout (set = 0, binding = 1001)
uniform sampler PointSampler;

#if FFX_HALF
#define FFX_DOF_HALF_OPT_LAYOUT rgba16f
#else // #if FFX_HALF
#define FFX_DOF_HALF_OPT_LAYOUT rgba32f
#endif // #if FFX_HALF #else

// SRVs
#if defined(FFX_DOF_BIND_SRV_INPUT_DEPTH)
    layout (set = 0, binding = FFX_DOF_BIND_SRV_INPUT_DEPTH)
    uniform texture2D r_input_depth;
#endif
#if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)
    layout (set = 0, binding = FFX_DOF_BIND_SRV_INPUT_COLOR)
    uniform texture2D r_input_color;
#endif
#if defined(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)
    layout (set = 0, binding = FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)
    uniform texture2D r_internal_bilat_color;
#endif
#if defined(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)
    layout (set = 0, binding = FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)
    uniform texture2D r_internal_dilated_radius;
#endif

// UAVs
#if defined(FFX_DOF_BIND_UAV_INTERNAL_BILAT_COLOR)
    layout (set = 0, binding = FFX_DOF_BIND_UAV_INTERNAL_BILAT_COLOR, FFX_DOF_HALF_OPT_LAYOUT)
    uniform image2D rw_internal_bilat_color[4];
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_RADIUS)
    layout (set = 0, binding = FFX_DOF_BIND_UAV_INTERNAL_RADIUS, rg32f)
    uniform image2D rw_internal_radius;
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_DILATED_RADIUS)
    layout (set = 0, binding = FFX_DOF_BIND_UAV_INTERNAL_DILATED_RADIUS, rg32f)
    uniform image2D rw_internal_dilated_radius;
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_NEAR)
    layout (set = 0, binding = FFX_DOF_BIND_UAV_INTERNAL_NEAR, FFX_DOF_HALF_OPT_LAYOUT)
    uniform image2D rw_internal_near;
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_FAR)
    layout (set = 0, binding = FFX_DOF_BIND_UAV_INTERNAL_FAR, FFX_DOF_HALF_OPT_LAYOUT)
    uniform image2D rw_internal_far;
#endif
#if defined(FFX_DOF_BIND_UAV_OUTPUT_COLOR)
    layout (set = 0, binding = FFX_DOF_BIND_UAV_OUTPUT_COLOR, rgba32f)
    uniform image2D rw_output_color;
#endif
#if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)
    layout (set = 0, binding = FFX_DOF_BIND_UAV_INTERNAL_GLOBALS, std430)
    coherent buffer DofGlobalVars_t { uint maxTileRad; } rw_internal_globals;
#endif

#include "ffx_dof_common.h"

#if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)
FfxHalfOpt4 FfxDofLoadSource(FfxInt32x2 tex)
{
    return FfxHalfOpt4(texelFetch(sampler2D(r_input_color, LinearSampler), tex, 0));
}
#endif // #if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_BILAT_COLOR)
void FfxDofStoreBilatMip(FfxUInt32 mip, FfxInt32x2 tex, FfxHalfOpt4 value)
{
    imageStore(rw_internal_bilat_color[mip], tex, value);
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_BILAT_COLOR)

#if defined(FFX_DOF_BIND_CB_DOF)
FfxFloat32 FfxDofGetCoc(FfxFloat32 depth)
{
    // clamped for perf reasons
    return clamp(CocScale() * depth + CocBias(), -CocLimit(), CocLimit());
}
#endif // #if defined(FFX_DOF_BIND_CB_DOF)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_DILATED_RADIUS)
void FfxDofStoreDilatedRadius(FfxUInt32x2 coord, FfxFloat32x2 dilatedMinMax)
{
    imageStore(rw_internal_dilated_radius, ivec2(coord), vec4(dilatedMinMax, 0, 0));
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_DILATED_RADIUS)

#if defined(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)
#if defined(FFX_DOF_BIND_CB_DOF)
FfxFloat32x2 FfxDofSampleDilatedRadius(FfxUInt32x2 coord)
{
    return textureLod(sampler2D(r_internal_dilated_radius, LinearSampler), vec2(coord) * InputSizeHalfRcp(), 0).rg;
}
#endif // #if defined(FFX_DOF_BIND_CB_DOF)

FfxFloat32x2 FfxDofLoadDilatedRadius(FfxUInt32x2 coord)
{
    return texelFetch(sampler2D(r_internal_dilated_radius, LinearSampler), ivec2(coord), 0).rg;
}
#endif // #if defined(FFX_DOF_BIND_SRV_INTERNAL_DILATED_RADIUS)

#if defined(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)
FfxHalfOpt4 FfxDofLoadInput(FfxUInt32x2 coord)
{
    return FfxHalfOpt4(texelFetch(sampler2D(r_internal_bilat_color, LinearSampler), ivec2(coord), 0));
}

#if defined(FFX_DOF_BIND_CB_DOF)
FfxHalfOpt4 FfxDofSampleInput(FfxFloat32x2 coord, FfxUInt32 mip)
{
    return FfxHalfOpt4(textureLod(sampler2D(r_internal_bilat_color, PointSampler), coord, mip));
}
#endif // #if defined(FFX_DOF_BIND_CB_DOF)
#endif // #if defined(FFX_DOF_BIND_SRV_INTERNAL_BILAT_COLOR)

#if defined(FFX_DOF_BIND_SRV_INPUT_DEPTH)
FfxFloat32x4 FfxDofGatherDepth(FfxFloat32x2 coord)
{
    return textureGather(sampler2D(r_input_depth, LinearSampler), coord);
}

FfxFloat32 FfxDofLoadFullDepth(FfxUInt32x2 coord)
{
    return texelFetch(sampler2D(r_input_depth, LinearSampler), ivec2(coord), 0).r;
}
#endif // #if defined(FFX_DOF_BIND_SRV_INPUT_DEPTH)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_NEAR)
void FfxDofStoreNear(FfxUInt32x2 coord, FfxHalfOpt4 color)
{
    imageStore(rw_internal_near, ivec2(coord), color);
}

FfxHalfOpt4 FfxDofLoadNear(FfxUInt32x2 coord)
{
    return FfxHalfOpt4(imageLoad(rw_internal_near, ivec2(coord)));
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_NEAR)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_FAR)
void FfxDofStoreFar(FfxUInt32x2 coord, FfxHalfOpt4 color)
{
    imageStore(rw_internal_far, ivec2(coord), color);
}

FfxHalfOpt4 FfxDofLoadFar(FfxUInt32x2 coord)
{
    return FfxHalfOpt4(imageLoad(rw_internal_far, ivec2(coord)));
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_FAR)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)
void FfxDofAccumMaxTileRadius(FfxUInt32 radius)
{
    atomicMax(rw_internal_globals.maxTileRad, radius);
}

FfxUInt32 FfxDofGetMaxTileRadius()
{
    return rw_internal_globals.maxTileRad;
}

void FfxDofResetMaxTileRadius()
{
    rw_internal_globals.maxTileRad = 0;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_GLOBALS)

#if defined(FFX_DOF_BIND_UAV_INTERNAL_RADIUS)
void FfxDofStoreTileRadius(FfxUInt32x2 tile, FfxFloat32x2 radius)
{
    imageStore(rw_internal_radius, ivec2(tile), vec4(radius, 0, 0));
}

FfxFloat32x2 FfxDofLoadTileRadius(FfxUInt32x2 tile)
{
    return imageLoad(rw_internal_radius, ivec2(tile)).rg;
}
#endif // #if defined(FFX_DOF_BIND_UAV_INTERNAL_RADIUS)

#if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)
FfxFloat32x4 FfxDofLoadFullInput(FfxUInt32x2 coord)
{
    return texelFetch(sampler2D(r_input_color, LinearSampler), ivec2(coord), 0);
}
#elif (defined(FFX_DOF_BIND_UAV_OUTPUT_COLOR)) && FFX_DOF_OPTION_COMBINE_IN_PLACE
FfxFloat32x4 FfxDofLoadFullInput(FfxUInt32x2 coord)
{
    return imageLoad(rw_output_color, ivec2(coord));
}
#endif // #if defined(FFX_DOF_BIND_SRV_INPUT_COLOR)

#if defined(FFX_DOF_BIND_UAV_OUTPUT_COLOR)
void FfxDofStoreOutput(FfxUInt32x2 coord, FfxFloat32x4 color)
{
    imageStore(rw_output_color, ivec2(coord), color);
}
#endif // #if defined(FFX_DOF_BIND_UAV_OUTPUT_COLOR)

#endif // #if defined(FFX_GPU)
