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
#define FFX_LPM_DECLARE_SRV(regIndex)  register(DECLARE_SRV_REGISTER(regIndex))
#define FFX_LPM_DECLARE_UAV(regIndex)  register(DECLARE_UAV_REGISTER(regIndex))
#define FFX_LPM_DECLARE_CB(regIndex)   register(DECLARE_CB_REGISTER(regIndex))

#if defined(LPM_BIND_CB_LPM)
    cbuffer cbLPM : FFX_LPM_DECLARE_CB(LPM_BIND_CB_LPM)
    {
        FfxUInt32x4 ctl[24];
        FfxBoolean shoulder;
        FfxBoolean con;
        FfxBoolean soft;
        FfxBoolean con2;
        FfxBoolean clip;
        FfxBoolean scaleOnly;
        FfxUInt32  displayMode;
        FfxUInt32  pad;
        #define FFX_LPM_CONSTANT_BUFFER_1_SIZE 32 // Number of 32-bit values. This must be kept in sync with the cbLPM size.
    };
#else
    #define ctl 0
    #define shoulder 0
    #define con 0
    #define soft 0
    #define con2 0
    #define clip 0
    #define scaleOnly 0
    #define displayMode 0
    #define pad 0
#endif

#if defined(FFX_GPU)
#define FFX_LPM_ROOTSIG_STRINGIFY(p) FFX_LPM_ROOTSIG_STR(p)
#define FFX_LPM_ROOTSIG_STR(p) #p
#define FFX_LPM_ROOTSIG [RootSignature( "DescriptorTable(UAV(u0, numDescriptors = " FFX_LPM_ROOTSIG_STRINGIFY(FFX_LPM_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "DescriptorTable(SRV(t0, numDescriptors = " FFX_LPM_ROOTSIG_STRINGIFY(FFX_LPM_RESOURCE_IDENTIFIER_COUNT) ")), " \
                                    "CBV(b0), " \
                                    "StaticSampler(s0, filter = FILTER_MIN_MAG_MIP_LINEAR, " \
                                                      "addressU = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressV = TEXTURE_ADDRESS_CLAMP, " \
                                                      "addressW = TEXTURE_ADDRESS_CLAMP, " \
                                                      "comparisonFunc = COMPARISON_NEVER, " \
                                                      "borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK)" )]

#if defined(FFX_LPM_EMBED_ROOTSIG)
#define FFX_LPM_EMBED_ROOTSIG_CONTENT FFX_LPM_ROOTSIG
#else
#define FFX_LPM_EMBED_ROOTSIG_CONTENT
#endif // #if FFX_LPM_EMBED_ROOTSIG
#endif // #if defined(FFX_GPU)

FfxUInt32x4 LpmFilterCtl(FfxUInt32 i)
{
    return ctl[i];
}

FfxBoolean GetShoulder()
{
    return shoulder;
}

FfxBoolean GetCon()
{
    return con;
}

FfxBoolean GetSoft()
{
    return soft;
}

FfxBoolean GetCon2()
{
    return con2;
}

FfxBoolean GetClip()
{
    return clip;
}

FfxBoolean GetScaleOnly()
{
    return scaleOnly;
}

FfxUInt32 GetMonitorDisplayMode()
{
    return displayMode;
}

#if FFX_HALF
FfxFloat16x3 ApplyGamma(FfxFloat16x3 color)
{
    color = ffxPow(color, FfxFloat16(1.0f / 2.2f));
    return color;
}

FfxFloat16x3 ApplyPQ(FfxFloat16x3 color)
{
    // Apply ST2084 curve
    FfxFloat16 m1 = 2610.0 / 4096.0 / 4;
    FfxFloat16 m2 = 2523.0 / 4096.0 * 128;
    FfxFloat16 c1 = 3424.0 / 4096.0;
    FfxFloat16 c2 = 2413.0 / 4096.0 * 32;
    FfxFloat16 c3 = 2392.0 / 4096.0 * 32;
    FfxFloat16x3 cp = ffxPow(abs(color.xyz), m1);
    color.xyz = ffxPow((c1 + c2 * cp) / (1 + c3 * cp), m2);
    return color;
}
#else
FfxFloat32x3 ApplyGamma(FfxFloat32x3 color)
{
    color = ffxPow(color, 1.0f / 2.2f);
    return color;
}

FfxFloat32x3 ApplyPQ(FfxFloat32x3 color)
{
    // Apply ST2084 curve
    FfxFloat32 m1 = 2610.0 / 4096.0 / 4;
    FfxFloat32 m2 = 2523.0 / 4096.0 * 128;
    FfxFloat32 c1 = 3424.0 / 4096.0;
    FfxFloat32 c2 = 2413.0 / 4096.0 * 32;
    FfxFloat32 c3 = 2392.0 / 4096.0 * 32;
    FfxFloat32x3 cp = ffxPow(abs(color.xyz), m1);
    color.xyz = ffxPow((c1 + c2 * cp) / (1 + c3 * cp), m2);
    return color;
}
#endif

SamplerState s_LinearClamp : register(s0);

#if FFX_HALF
#define ColorFormat FfxFloat16x4
#else
#define ColorFormat FfxFloat32x4
#endif

    // SRVs
    #if defined LPM_BIND_SRV_INPUT_COLOR
        Texture2D<ColorFormat>             r_input_color       : FFX_LPM_DECLARE_SRV(LPM_BIND_SRV_INPUT_COLOR);
    #endif
    // UAV declarations
    #if defined LPM_BIND_UAV_OUTPUT_COLOR
        RWTexture2D<ColorFormat>           rw_output_color     : FFX_LPM_DECLARE_UAV(LPM_BIND_UAV_OUTPUT_COLOR);
    #endif

#if defined(LPM_BIND_SRV_INPUT_COLOR)
ColorFormat LoadInput(FfxUInt32x2 iPxPos)
{
    return r_input_color[iPxPos];
}
#endif // defined(LPM_BIND_SRV_INPUT_COLOR)

#if defined(LPM_BIND_UAV_OUTPUT_COLOR)
void StoreOutput(FfxUInt32x2 iPxPos, ColorFormat fColor)
{
    rw_output_color[iPxPos] = fColor;
}
#endif // defined(LPM_BIND_UAV_OUTPUT_COLOR)

#endif // #if defined(FFX_GPU)
