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
#define FFX_LENS_DECLARE_SRV(regIndex)  register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_LENS_DECLARE_UAV(regIndex)  register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_LENS_DECLARE_CB(regIndex)   register(DECLARE_CB_REGISTER(regIndex))

#if defined(LENS_BIND_CB_LENS)
    cbuffer cbLens : FFX_LENS_DECLARE_CB(LENS_BIND_CB_LENS)
    {
        FfxFloat32 grainScale;
        FfxFloat32 grainAmount;
        FfxUInt32  grainSeed;
        FfxUInt32  pad;
        
        FfxUInt32x2 center;
        FfxFloat32 chromAb;
        FfxFloat32 vignette;
       #define FFX_LENS_CONSTANT_BUFFER_1_SIZE 8  // Number of 32-bit values. This must be kept in sync with the cbLens size.
    };
#else
    #define grainScale 0
    #define grainAmount 0
    #define grainSeed 0
    #define pad 0
    #define center uint2(0,0)
    #define vignette 0.0
    #define chromAb 0.0
#endif

#if defined(FFX_GPU)
#define FFX_LENS_ROOTSIG_STRINGIFY(p) FFX_LENS_ROOTSIG_STR(p)
#define FFX_LENS_ROOTSIG_STR(p) #p
#define FFX_LENS_ROOTSIG [RootSignature( "DescriptorTable(UAV(u0, numDescriptors = " FFX_LENS_ROOTSIG_STRINGIFY(FFX_LENS_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "DescriptorTable(SRV(t0, numDescriptors = " FFX_LENS_ROOTSIG_STRINGIFY(FFX_LENS_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "CBV(b0), " \
                                    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                                                      "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                      "comparisonFunc = COMPARISON_NEVER, " \
                                                      "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK)" )]

#if defined(FFX_LENS_EMBED_ROOTSIG)
#define FFX_LENS_EMBED_ROOTSIG_CONTENT FFX_LENS_ROOTSIG
#else
#define FFX_LENS_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_LENS_EMBED_ROOTSIG
#endif // #if defined(FFX_GPU)


FfxFloat32 GrainScale()
{
    return grainScale;
}

FfxFloat32 GrainAmount()
{
    return grainAmount;
}

FfxUInt32 GrainSeed()
{
    return grainSeed;
}

FfxUInt32x2 Center()
{
    return center;
}

FfxFloat32 Vignette()
{
    return vignette;
}

FfxFloat32 ChromAb()
{
    return chromAb;
}

SamplerState s_LinearClamp : register(s0);

// SRVs
#if defined LENS_BIND_SRV_INPUT_TEXTURE
    Texture2D<FfxFloat32x4> r_input_texture : FFX_LENS_DECLARE_SRV(LENS_BIND_SRV_INPUT_TEXTURE);
#endif

// UAVs
#if defined LENS_BIND_UAV_OUTPUT_TEXTURE
    RWTexture2D<FfxFloat32x4> rw_output_texture : FFX_LENS_DECLARE_UAV(LENS_BIND_UAV_OUTPUT_TEXTURE);
#endif

#if FFX_HALF

#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat16 FfxLensSampleR(FfxFloat32x2 fPxPos)
{
    return FfxFloat16(r_input_texture.SampleLevel(s_LinearClamp, fPxPos, 0).r);
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)
 
#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat16 FfxLensSampleG(FfxFloat32x2 fPxPos)
{
    return FfxFloat16(r_input_texture.SampleLevel(s_LinearClamp, fPxPos, 0).g);
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)
 
#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat16 FfxLensSampleB(FfxFloat32x2 fPxPos)
{
    return FfxFloat16(r_input_texture.SampleLevel(s_LinearClamp, fPxPos, 0).b);
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)
 
#if defined(LENS_BIND_UAV_OUTPUT_TEXTURE)
void StoreLensOutput(FfxInt32x2 iPxPos, FfxFloat16x3 fColor)
{
    rw_output_texture[iPxPos] = FfxFloat32x4(fColor, 1.0);
}
#endif // defined(LENS_BIND_UAV_OUTPUT_TEXTURE)

#else // FFX_HALF

#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat32 FfxLensSampleR(FfxFloat32x2 fPxPos)
{
    return r_input_texture.SampleLevel(s_LinearClamp, fPxPos, 0).r;
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)
 
#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat32 FfxLensSampleG(FfxFloat32x2 fPxPos)
{
    return r_input_texture.SampleLevel(s_LinearClamp, fPxPos, 0).g;
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)
 
#if defined(LENS_BIND_SRV_INPUT_TEXTURE)
FfxFloat32 FfxLensSampleB(FfxFloat32x2 fPxPos)
{
    return r_input_texture.SampleLevel(s_LinearClamp, fPxPos, 0).b;
}
#endif // defined(LENS_BIND_SRV_INPUT_TEXTURE)
 
#if defined(LENS_BIND_UAV_OUTPUT_TEXTURE)
void StoreLensOutput(FfxInt32x2 iPxPos, FfxFloat32x3 fColor)
{
    rw_output_texture[iPxPos] = FfxFloat32x4(fColor, 1.0);
}
#endif // defined(LENS_BIND_UAV_OUTPUT_TEXTURE)

#endif // FFX_HALF

#endif // #if defined(FFX_GPU)
