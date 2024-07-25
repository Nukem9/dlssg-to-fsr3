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
#include "ffx_core.h"

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#if defined(VRS_BIND_CB_VRS)
    layout(set = 0, binding = VRS_BIND_CB_VRS, std140) uniform cbVRS_t
    {
        FfxFloat32x2 motionVectorScale;
        FfxFloat32   varianceCutoff;
        FfxFloat32   motionFactor;
        FfxInt32x2   resolution;
        FfxUInt32    tileSize;
    } cbVRS;

#endif

FfxInt32x2 Resolution()
{
#if defined(VRS_BIND_CB_VRS)
    return cbVRS.resolution;
#else
    return FfxInt32x2(0, 0);
#endif
}

FfxUInt32 TileSize()
{
#if defined(VRS_BIND_CB_VRS)
    return cbVRS.tileSize;
#else
    return 0;
#endif
}

FfxFloat32 VarianceCutoff()
{
#if defined(VRS_BIND_CB_VRS)
    return cbVRS.varianceCutoff;
#else
    return 0.f;
#endif
}

FfxFloat32 MotionFactor()
{
#if defined(VRS_BIND_CB_VRS)
    return cbVRS.motionFactor;
#else
    return 0.f
#endif
}

FfxFloat32x2 MotionVectorScale()
{
#if defined(VRS_BIND_CB_VRS)
    return cbVRS.motionVectorScale;
#else
    return FfxFloat32x2(0.0f);
#endif
}

// SRVs
#if defined VRS_BIND_SRV_INPUT_COLOR
    layout (set = 0, binding = VRS_BIND_SRV_INPUT_COLOR)                uniform texture2D  r_input_color;
#endif
#if defined VRS_BIND_SRV_INPUT_MOTIONVECTORS
    layout (set = 0, binding = VRS_BIND_SRV_INPUT_MOTIONVECTORS)        uniform texture2D  r_input_velocity;
#endif
#if defined VRS_BIND_SRV_OUTPUT_VRSIMAGE
    layout (set = 0, binding = VRS_BIND_SRV_OUTPUT_VRSIMAGE)            uniform texture2D  r_vrsimage_output;
#endif

// UAV declarations
#if defined VRS_BIND_UAV_INPUT_COLOR
    layout (set = 0, binding = VRS_BIND_UAV_INPUT_COLOR, rgba32f)            uniform image2D  rw_input_color;
#endif
#if defined VRS_BIND_UAV_INPUT_MOTIONVECTORS
    layout (set = 0, binding = VRS_BIND_UAV_INPUT_MOTIONVECTORS, rg16f)      uniform image2D  rw_input_velocity;
#endif
#if defined VRS_BIND_UAV_OUTPUT_VRSIMAGE
    layout (set = 0, binding = VRS_BIND_UAV_OUTPUT_VRSIMAGE, r8ui)           uniform uimage2D  rw_vrsimage_output;
#endif


#if defined(VRS_BIND_SRV_INPUT_COLOR)
// read a value from previous frames color buffer and return luminance
FfxFloat32 ReadLuminance(FfxInt32x2 pos)
{
    FfxFloat32x3 color = texelFetch(r_input_color, pos, 0).xyz;

    // return color value converted to grayscale
    return dot(color, FfxFloat32x3(0.30, 0.59, 0.11));
    // in some cases using different weights, linearizing the color values
    // or multiplying luminance with a value based on specularity or depth
    // may yield better results
}
#endif // #if defined(VRS_BIND_SRV_INPUT_COLOR)

#if defined(VRS_BIND_SRV_INPUT_MOTIONVECTORS)
// read per pixel motion vectors and convert them to pixel-space
FfxFloat32x2 ReadMotionVec2D(FfxInt32x2 pos)
{
    FfxFloat32x2 velocity = texelFetch(r_input_velocity, pos, 0).xy * MotionVectorScale();
    // return 0 to not use motion vectors
    return velocity * Resolution();
}
#endif // #if defined(VRS_BIND_SRV_INPUT_MOTIONVECTORS)

#if defined(VRS_BIND_UAV_OUTPUT_VRSIMAGE)
void WriteVrsImage(FfxInt32x2 pos, FfxUInt32 value)
{
    imageStore(rw_vrsimage_output, pos, FfxUInt32x4(value));
}
#endif // #if defined(VRS_BIND_UAV_OUTPUT_VRSIMAGE)

#endif // #if defined(FFX_GPU)
