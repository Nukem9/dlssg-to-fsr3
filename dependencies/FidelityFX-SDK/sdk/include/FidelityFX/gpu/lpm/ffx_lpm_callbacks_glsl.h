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

#include "ffx_lpm_resources.h"

#if defined(FFX_GPU)
#include "ffx_core.h"

#ifndef FFX_PREFER_WAVE64
#define FFX_PREFER_WAVE64
#endif // #ifndef FFX_PREFER_WAVE64

#if defined(LPM_BIND_CB_LPM)
    layout (set = 0, binding = LPM_BIND_CB_LPM, std140) uniform cbLPM_t
    {
        FfxUInt32x4   ctl[24];
        FfxBoolean    shoulder;
        FfxBoolean    con;
        FfxBoolean    soft;
        FfxBoolean    con2;
        FfxBoolean    clip;
        FfxBoolean    scaleOnly;
        FfxUInt32     displayMode;
        FfxUInt32     pad;
    } cbLPM;
#else
    #define ctl         0
    #define shoulder    0
    #define con         0
    #define soft        0
    #define con2        0
    #define clip        0
    #define scaleOnly   0
    #define displayMode 0
    #define pad         0
#endif

FfxUInt32x4 LpmFilterCtl(FfxUInt32 i)
{
    return cbLPM.ctl[i];
}

FfxBoolean GetShoulder()
{
    return cbLPM.shoulder;
}

FfxBoolean GetCon()
{
    return cbLPM.con;
}

FfxBoolean GetSoft()
{
    return cbLPM.soft;
}

FfxBoolean GetCon2()
{
    return cbLPM.con2;
}

FfxBoolean GetClip()
{
    return cbLPM.clip;
}

FfxBoolean GetScaleOnly()
{
    return cbLPM.scaleOnly;
}

FfxUInt32 GetMonitorDisplayMode()
{
    return cbLPM.displayMode;
}

#if FFX_HALF
FfxFloat16x3 ApplyGamma(FfxFloat16x3 color)
{
    color = pow(color, FfxFloat16x3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    return color;
}

FfxFloat16x3 ApplyPQ(FfxFloat16x3 color)
{
    // Apply ST2084 curve
    FfxFloat16   m1 = FfxFloat16(2610.0 / 4096.0 / 4.0);
    FfxFloat16   m2 = FfxFloat16(2523.0 / 4096.0 * 128.0);
    FfxFloat16   c1 = FfxFloat16(3424.0 / 4096.0);
    FfxFloat16   c2 = FfxFloat16(2413.0 / 4096.0 * 32.0);
    FfxFloat16   c3 = FfxFloat16(2392.0 / 4096.0 * 32.0);
    FfxFloat16x3 cp = pow(abs(color.xyz), FfxFloat16x3(m1, m1, m1));
    color.xyz       = pow((c1 + c2 * cp) / (FfxFloat16(1.0) + c3 * cp), FfxFloat16x3(m2, m2, m2));
    return color;
}
#else
FfxFloat32x3 ApplyGamma(FfxFloat32x3 color)
{
    color = pow(color, FfxFloat32x3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
    return color;
}

FfxFloat32x3 ApplyPQ(FfxFloat32x3 color)
{
    // Apply ST2084 curve
    FfxFloat32   m1 = 2610.0 / 4096.0 / 4;
    FfxFloat32   m2 = 2523.0 / 4096.0 * 128;
    FfxFloat32   c1 = 3424.0 / 4096.0;
    FfxFloat32   c2 = 2413.0 / 4096.0 * 32;
    FfxFloat32   c3 = 2392.0 / 4096.0 * 32;
    FfxFloat32x3 cp = pow(abs(color.xyz), FfxFloat32x3(m1, m1, m1));
    color.xyz       = pow((c1 + c2 * cp) / (1 + c3 * cp), FfxFloat32x3(m2, m2, m2));
    return color;
}
#endif

layout (set = 0, binding = 1000) uniform sampler s_LinearClamp;

#if FFX_HALF
#define ColorFormat FfxFloat16x4
#define OutputFormat rgba16f
#else
#define ColorFormat FfxFloat32x4
#define OutputFormat rgba32f
#endif

// SRVs
#if defined LPM_BIND_SRV_INPUT_COLOR
    layout (set = 0, binding = LPM_BIND_SRV_INPUT_COLOR)     uniform texture2D  r_input_color;
#endif

// UAV declarations
#if defined LPM_BIND_UAV_OUTPUT_COLOR
    layout (set = 0, binding = LPM_BIND_UAV_OUTPUT_COLOR, OutputFormat)    uniform image2D  rw_output_color;
#endif

#if defined(LPM_BIND_SRV_INPUT_COLOR)
ColorFormat LoadInput(FfxUInt32x2 iPxPos)
{
    return ColorFormat(texelFetch(r_input_color, FfxInt32x2(iPxPos), 0));
}
#endif  // defined(LPM_BIND_SRV_INPUT_COLOR)

#if defined(LPM_BIND_UAV_OUTPUT_COLOR)
void StoreOutput(FfxUInt32x2 iPxPos, ColorFormat fColor)
{
    imageStore(rw_output_color, FfxInt32x2(iPxPos), fColor);
}
#endif  // defined(LPM_BIND_UAV_OUTPUT_COLOR)

#endif // #if defined(FFX_GPU)
