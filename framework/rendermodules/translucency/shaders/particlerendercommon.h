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
#endif  // __cplusplus

// The rasterization path constant buffer
#if __cplusplus
struct RenderingConstantBuffer
{
    Mat4     m_Projection     = {};
    Mat4     m_ProjectionInv  = {};
    Vec4     m_SunColor       = {};
    Vec4     m_AmbientColor   = {};
    Vec4     m_SunDirectionVS = {};
    uint32_t m_ScreenWidth    = 0;
    uint32_t m_ScreenHeight   = 0;
    uint32_t m_pads[2]        = {};
};
#else
cbuffer RenderingConstantBuffer : register(b0)
{
    matrix g_mProjection;
    matrix g_mProjectionInv;

    float4 g_SunColor;
    float4 g_AmbientColor;
    float4 g_SunDirectionVS;

    uint g_ScreenWidth;
    uint g_ScreenHeight;
    uint g_pads0;
    uint g_pads1;
};
#endif  // __cplusplus
