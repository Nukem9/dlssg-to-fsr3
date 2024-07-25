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

#define NUM_EMITTERS 4
#if __cplusplus
struct SimulationConstantBuffer
{
    Vec4 m_StartColor[NUM_EMITTERS] = {};
    Vec4 m_EndColor[NUM_EMITTERS]   = {};

    Vec4 m_EmitterLightingCenter[NUM_EMITTERS] = {};

    Mat4 m_ViewProjection = {};
    Mat4 m_View           = {};
    Mat4 m_ViewInv        = {};
    Mat4 m_ProjectionInv  = {};

    Vec4 m_EyePosition  = {};
    Vec4 m_SunDirection = {};

    uint32_t m_ScreenWidth        = 0;
    uint32_t m_ScreenHeight       = 0;
    float    m_ElapsedTime        = 0.0f;
    float    m_CollisionThickness = 4.0f;

    int   m_CollideParticles      = 0;
    int   m_ShowSleepingParticles = 0;
    int   m_EnableSleepState      = 0;
    float m_FrameTime             = 0.0f;

    int      m_MaxParticles = 0;
    uint32_t m_pad01        = 0;
    uint32_t m_pad02        = 0;
    uint32_t m_pad03        = 0;
};
#else
cbuffer SimulationConstantBuffer : register(b0)
{
    float4 g_StartColor[NUM_EMITTERS];
    float4 g_EndColor[NUM_EMITTERS];

    float4 g_EmitterLightingCenter[NUM_EMITTERS];

    matrix g_mViewProjection;
    matrix g_mView;
    matrix g_mViewInv;
    matrix g_mProjectionInv;

    float4 g_EyePosition;
    float4 g_SunDirection;

    uint  g_ScreenWidth;
    uint  g_ScreenHeight;
    float g_ElapsedTime;
    float g_CollisionThickness;

    int   g_CollideParticles;
    int   g_ShowSleepingParticles;
    int   g_EnableSleepState;
    float g_FrameTime;

    int  g_MaxParticles;
    uint g_Pad0;
    uint g_Pad1;
    uint g_Pad2;
};
#endif  // __cplusplus

#if __cplusplus
struct EmitterConstantBuffer
{
    Vec4 m_EmitterPosition  = {};
    Vec4 m_EmitterVelocity  = {};
    Vec4 m_PositionVariance = {};

    int   m_MaxParticlesThisFrame = 0;
    float m_ParticleLifeSpan      = 0.0f;
    float m_StartSize             = 0.0f;
    float m_EndSize               = 0.0f;

    float m_VelocityVariance = 0.0f;
    float m_Mass             = 0.0f;
    int   m_Index            = 0;
    int   m_Streaks          = 0;

    int m_TextureIndex = 0;
    int m_pads[3]      = {};
};
#else
cbuffer EmitterConstantBuffer : register(b1)
{
    float4 g_vEmitterPosition;
    float4 g_vEmitterVelocity;
    float4 g_PositionVariance;

    int   g_MaxParticlesThisFrame;
    float g_ParticleLifeSpan;
    float g_StartSize;
    float g_EndSize;

    float g_VelocityVariance;
    float g_Mass;
    uint  g_EmitterIndex;
    uint  g_EmitterStreaks;

    uint g_TextureIndex;
    uint g_pads0;
    uint g_pads1;
    uint g_pads2;
};
#endif  // __cplusplus
