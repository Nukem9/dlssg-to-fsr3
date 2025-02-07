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

#ifndef TONEMAPPERCOMMON_H
#define TONEMAPPERCOMMON_H

#if __cplusplus
    #pragma once
    #include <shaders/shadercommon.h>
#else
    #include "shadercommon.h"
#endif // __cplusplus

#if __cplusplus
typedef struct AutoExposureSpdConstants
{
    uint32_t                    mips;
    uint32_t                    numWorkGroups;
    uint32_t                    workGroupOffset[2];
    uint32_t                    renderSize[2];
} AutoExposureSpdConstants;

struct TonemapperCBData
{
    mutable float       Exposure = 1.0f;
    mutable uint32_t    ToneMapper = 0;
    float               DisplayMaxLuminance;
    DisplayMode         MonitorDisplayMode;

    Mat4                ContentToMonitorRecMatrix;

    int32_t             LetterboxRectBase[2];
    int32_t             LetterboxRectSize[2];

    uint32_t            UseAutoExposure = 0;
    uint32_t            LensDistortionEnabled = 0;
    float               LensDistortionStrength = -0.2f;
    float               LensDistortionZoom = 0.4f;

    
};
#else
cbuffer AutoExposureSpdCBData : register(b0)
{
    uint  mips;
    uint  numWorkGroups;
    uint2 workGroupOffset;
    uint2 renderSize;
};

cbuffer TonemapperCBData : register(b0)
{
    float Exposure                      : packoffset(c0.x);
    uint  ToneMapper                    : packoffset(c0.y);
    float DisplayMaxLuminance           : packoffset(c0.z);
    DisplayMode MonitorDisplayMode      : packoffset(c0.w);
    matrix ContentToMonitorRecMatrix    : packoffset(c1);
    int2 LetterboxRectBase              : packoffset(c5.x);
    int2 LetterboxRectSize              : packoffset(c5.z);
    bool UseAutoExposure                : packoffset(c6.x);
    bool LensDistortionEnabled          : packoffset(c6.y);
    float LensDistortionStrength        : packoffset(c6.z);
    float LensDistortionZoom            : packoffset(c6.w);
}
#endif // __cplusplus

#endif // TONEMAPPERCOMMON_H
