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

#if __cplusplus
    #pragma once
    #include "misc/math.h"
#endif // __cplusplus

struct MagnifierCBData
{
#if __cplusplus
    uint32_t        ImageWidth = 2560;
    uint32_t        ImageHeight = 1440;
    int32_t         MousePosX = 0;
    int32_t         MousePosY = 0;
    Vec4            BorderColorRGB = Vec4(0.72f, 0.002f, 0.0f, 1.f);
    
    // Directly modifiable by the UI
    mutable float   MagnificationAmount = 6.f;
    mutable float   MagnifierScreenRadius = 0.35f;
    mutable int32_t MagnifierOffsetX = 500;
    mutable int32_t MagnifierOffsetY = -500;
#else 
    uint     ImageWidth;
    uint     ImageHeight;
    int      MousePosX;
    int      MousePosY;
    float4   BorderColorRGB;
    float    MagnificationAmount;
    float    MagnifierScreenRadius;
    int      MagnifierOffsetX;
    int      MagnifierOffsetY;
#endif // __cplusplus
};

#if __cplusplus
    #pragma once
    #include <shaders/shadercommon.h>
#else
    #include "shadercommon.h"
#endif  // __cplusplus

struct HDRCBData
{
    #if __cplusplus
        Mat4    ContentToMonitorRecMatrix;
    #else
        matrix  ContentToMonitorRecMatrix;
    #endif  // __cplusplus
    float       DisplayMaxLuminance;
    DisplayMode MonitorDisplayMode;
};
