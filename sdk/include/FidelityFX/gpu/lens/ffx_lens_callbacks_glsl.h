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

#include "ffx_lens_resources.h"

#if defined(FFX_GPU)
#include "ffx_core.h"

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#pragma warning(disable: 3205)  // conversion from larger type to smaller

#if defined(LENS_BIND_CB_LENS)
    layout(set = 0, binding = LENS_BIND_CB_LENS, std140) uniform cbLens_t
    {
        FfxFloat32 grainScale;
        FfxFloat32 grainAmount;
        FfxUInt32  grainSeed;
        FfxUInt32  pad;
        
        FfxUInt32x2 center;
        FfxFloat32 chromAb;
        FfxFloat32 vignette;
       #define FFX_LENS_CONSTANT_BUFFER_1_SIZE 8  // Number of 32-bit values. This must be kept in sync with the cbLens size.
    } cbLens;
#endif

FfxFloat32 GrainScale()
{
#ifdef LENS_BIND_CB_LENS
    return cbLens.grainScale;
#else
    return 0;
#endif
}

FfxFloat32 GrainAmount()
{
#ifdef LENS_BIND_CB_LENS
    return cbLens.grainAmount;
#else
    return 0.0;
#endif
}

FfxUInt32 GrainSeed()
{
#ifdef LENS_BIND_CB_LENS
    return cbLens.grainSeed;
#else
    return 0;
#endif
}

FfxUInt32x2 Center()
{
#ifdef LENS_BIND_CB_LENS
    return cbLens.center;
#else
    return FfxUInt32x2(0, 0);
#endif

}

FfxFloat32 Vignette()
{
#ifdef LENS_BIND_CB_LENS
    return cbLens.vignette;
#else
    return 0.0;
#endif
}

FfxFloat32 ChromAb()
{
#ifdef LENS_BIND_CB_LENS
    return cbLens.chromAb;
#else
    return 0.0;
#endif
}

layout(set = 0, binding = 1000) uniform sampler s_LinearClamp;

// SRVs
#if defined LENS_BIND_SRV_INPUT_TEXTURE
layout(set = 0, binding = LENS_BIND_SRV_INPUT_TEXTURE) uniform texture2D r_input_texture;
#endif

// UAVs
#if defined LENS_BIND_UAV_OUTPUT_TEXTURE
layout(set = 0, binding = LENS_BIND_UAV_OUTPUT_TEXTURE, rgba32f) uniform image2D rw_output_texture;
#endif

#if FFX_HALF

#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat16 FfxLensSampleR(FfxFloat32x2 fPxPos)
{
    return FfxFloat16(texture(sampler2D(r_input_texture, s_LinearClamp), fPxPos).r);
}
#endif  // defined(LENS_BIND_SRV_INPUT_TEXTURE)

#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat16 FfxLensSampleG(FfxFloat32x2 fPxPos)
{
    return FfxFloat16(texture(sampler2D(r_input_texture, s_LinearClamp), fPxPos).g);
}
#endif  // defined(LENS_BIND_SRV_INPUT_TEXTURE)

#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat16 FfxLensSampleB(FfxFloat32x2 fPxPos)
{
    return FfxFloat16(texture(sampler2D(r_input_texture, s_LinearClamp), fPxPos).b);
}
#endif  // defined(LENS_BIND_SRV_INPUT_TEXTURE)

#if defined(LENS_BIND_UAV_OUTPUT_TEXTURE)
void StoreLensOutput(FfxInt32x2 iPxPos, FfxFloat16x3 fColor)
{
    imageStore(rw_output_texture, iPxPos, FfxFloat32x4(fColor, 1.0));
}
#endif  // defined(LENS_BIND_UAV_OUTPUT_TEXTURE)

#else  // FFX_HALF

#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat32 FfxLensSampleR(FfxFloat32x2 fPxPos)
{
    return texture(sampler2D(r_input_texture, s_LinearClamp), fPxPos).r;
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)

#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat32 FfxLensSampleG(FfxFloat32x2 fPxPos)
{
    return texture(sampler2D(r_input_texture, s_LinearClamp), fPxPos).g;
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)

#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat32 FfxLensSampleB(FfxFloat32x2 fPxPos)
{
    return texture(sampler2D(r_input_texture, s_LinearClamp), fPxPos).b;
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)

#if defined(LENS_BIND_UAV_OUTPUT_TEXTURE)
void StoreLensOutput(FfxInt32x2 iPxPos, FfxFloat32x3 fColor)
{
    imageStore(rw_output_texture, iPxPos, FfxFloat32x4(fColor, 1.0));
}
#endif // defined(LENS_BIND_UAV_OUTPUT_TEXTURE)

#endif // FFX_HALF

#endif // #if defined(FFX_GPU)
