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

#include "ffx_vrs_resources.h"

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

#define FFX_DECLARE_SRV_REGISTER(regIndex)  t##regIndex
#define FFX_DECLARE_UAV_REGISTER(regIndex)  u##regIndex
#define FFX_DECLARE_CB_REGISTER(regIndex)   b##regIndex
#define FFX_VRS_DECLARE_SRV(regIndex)  register(FFX_DECLARE_SRV_REGISTER(regIndex))
#define FFX_VRS_DECLARE_UAV(regIndex)  register(FFX_DECLARE_UAV_REGISTER(regIndex))
#define FFX_VRS_DECLARE_CB(regIndex)   register(FFX_DECLARE_CB_REGISTER(regIndex))

#if defined(VRS_BIND_CB_VRS)
    // Constant Buffer
    cbuffer cbVRS : FFX_VRS_DECLARE_CB(VRS_BIND_CB_VRS)
    {
        FfxFloat32x2 motionVectorScale;
        FfxFloat32 varianceCutoff;
        FfxFloat32 motionFactor;
        FfxInt32x2 resolution;
        FfxUInt32 tileSize;
        #define FFX_VRS_CONSTANT_BUFFER_1_SIZE 7
    }
#else
    #define resolution          0
    #define tileSize            0
    #define varianceCutoff      0
    #define motionFactor        0
    #define motionVectorScale   0
#endif

FfxInt32x2 Resolution()
{
    return resolution;
}

FfxUInt32 TileSize()
{
    return tileSize;
}

FfxFloat32 VarianceCutoff()
{
    return varianceCutoff;
}

FfxFloat32 MotionFactor()
{
    return motionFactor;
}

FfxFloat32x2 MotionVectorScale()
{
    return motionVectorScale;
}

#if defined(FFX_GPU)
#define FFX_VRS_ROOTSIG_STRINGIFY(p) FFX_VRS_ROOTSIG_STR(p)
#define FFX_VRS_ROOTSIG_STR(p) #p
#define FFX_VRS_ROOTSIG [RootSignature( "DescriptorTable(UAV(u0, numDescriptors = " FFX_VRS_ROOTSIG_STRINGIFY(FFX_VRS_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "DescriptorTable(SRV(t0, numDescriptors = " FFX_VRS_ROOTSIG_STRINGIFY(FFX_VRS_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "CBV(b0) ")]

#if defined(FFX_VRS_EMBED_ROOTSIG)
#define FFX_VRS_EMBED_ROOTSIG_CONTENT FFX_VRS_ROOTSIG
#else
#define FFX_VRS_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_VRS_EMBED_ROOTSIG
#endif // #if defined(FFX_GPU)

// SRVs
#if defined VRS_BIND_SRV_INPUT_COLOR
    Texture2D                  r_input_color                    : FFX_VRS_DECLARE_SRV(VRS_BIND_SRV_INPUT_COLOR);
#endif
#if defined VRS_BIND_SRV_INPUT_MOTIONVECTORS
    Texture2D                  r_input_velocity                 : FFX_VRS_DECLARE_SRV(VRS_BIND_SRV_INPUT_MOTIONVECTORS);
#endif

// UAV declarations
#if defined VRS_BIND_UAV_OUTPUT_VRSIMAGE
    RWTexture2D<uint>                            rw_vrsimage_output        : FFX_VRS_DECLARE_UAV(VRS_BIND_UAV_OUTPUT_VRSIMAGE);
#endif

#if defined(VRS_BIND_SRV_INPUT_COLOR)
// read a value from previous frames color buffer and return luminance
FfxFloat32 ReadLuminance(FfxInt32x2 pos)
{

    FfxFloat32x3 color = r_input_color[pos].xyz;

    // return color value converted to grayscale
    return dot(color, FfxFloat32x3(0.30, 0.59, 0.11));
    // in some cases using different weights, linearizing the color values
    // or multiplying luminance with a value based on specularity or depth
    // may yield better results
}
#endif // #if defined(VRS_BIND_SRV_INPUT_COLOR)

#if defined (VRS_BIND_SRV_INPUT_MOTIONVECTORS)
// read per pixel motion vectors and convert them to pixel-space
FfxFloat32x2 ReadMotionVec2D(FfxInt32x2 pos)
{
    // return 0 to not use motion vectors
    return r_input_velocity[pos].xy * MotionVectorScale() * resolution;
}
#endif // #if defined (VRS_BIND_SRV_INPUT_MOTIONVECTORS)

#if defined (VRS_BIND_UAV_OUTPUT_VRSIMAGE)
void WriteVrsImage(FfxInt32x2 pos, FfxUInt32 value)
{
    rw_vrsimage_output[pos] = value;
}
#endif // #if defined (VRS_BIND_UAV_OUTPUT_VRSIMAGE)

#endif // #if defined(FFX_GPU)
