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


// Particle structures
// ===================
#if __cplusplus
struct GPUParticlePartA
{
    Vec4 m_params[2];
};
#else
struct GPUParticlePartA
{
    uint2   m_PackedTintAndAlpha;                           // The color and opacity
    uint2   m_PackedVelocityXYEmitterNDotLAndRotation;      // Normalized view space velocity XY used for streak extrusion. The lighting term for the while emitter in Z. The rotation angle in W.

    uint    m_StreakLengthAndEmitterProperties;             //  0-15: fp16 streak length
                                                            // 16-23: The index of the emitter
                                                            // 24-29: Atlas index
                                                            // 30:    Whether or not the emitter supports velocity-based streaks
                                                            // 31:    Whether or not the particle is sleeping (ie, don't update position)
    float   m_Rotation;                                     // Uncompressed rotation - some issues with using fp16 rotation (also, saves unpacking it)
    uint    m_CollisionCount;                               // Keep track of how many times the particle has collided
    uint    m_pad;
};
#endif  // __cplusplus

#if __cplusplus
struct GPUParticlePartB
{
    Vec4 m_params[3];
};
#else
struct GPUParticlePartB
{
    float3  m_Position;             // World space position
    float   m_Mass;                 // Mass of particle

    float3  m_Velocity;             // World space velocity
    float   m_Lifespan;             // Lifespan of the particle.

    float   m_DistanceToEye;        // The distance from the particle to the eye
    float   m_Age;                  // The current age counting down from lifespan to zero
    float   m_StartSize;            // The size at spawn time
    float   m_EndSize;              // The time at maximum age
};
#endif  // __cplusplus

#if __cplusplus
struct IndirectCommand
{
    int args[5];
};
#else
struct IndirectCommand
{
    uint IndexCountPerInstance;
    uint InstanceCount;
    uint StartIndexLocation;
    int  BaseVertexLocation;
    uint StartInstanceLocation;
};
#endif  // __cplusplus

#if !__cplusplus

// fp16util
// ===================

// HLSL intrinsics
// cross
min16float3 RTGCross(min16float3 a, min16float3 b)
{
    return min16float3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// dot
min16float RTGDot2(min16float2 a, min16float2 b)
{
    return a.x * b.x + a.y * b.y;
}

min16float RTGDot3(min16float3 a, min16float3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

min16float RTGDot4(min16float4 a, min16float4 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

// length
min16float RTGLength2(min16float2 a)
{
    return sqrt(RTGDot2(a, a));
}

min16float RTGLength3(min16float3 a)
{
    return sqrt(RTGDot3(a, a));
}

min16float RTGLength4(min16float4 a)
{
    return sqrt(RTGDot4(a, a));
}

// normalize
min16float2 RTGNormalize2(min16float2 a)
{
    min16float l = RTGLength2(a);
    return l == 0.0 ? a : a / l;
}

min16float3 RTGNormalize3(min16float3 a)
{
    min16float l = RTGLength3(a);
    return l == 0.0 ? a : a / l;
}

min16float4 RTGNormalize4(min16float4 a)
{
    min16float l = RTGLength4(a);
    return l == 0.0 ? a : a / l;
}

// distance
min16float RTGDistance2(min16float2 from, min16float2 to)
{
    return RTGLength2(to - from);
}

min16float RTGDistance3(min16float3 from, min16float3 to)
{
    return RTGLength3(to - from);
}

min16float RTGDistance4(min16float4 from, min16float4 to)
{
    return RTGLength4(to - from);
}

// Packing and Unpacking
// min16{u}int2
int PackInt16(min16int2 v)
{
    uint x = asuint(int(v.x));
    uint y = asuint(int(v.y));
    return asint(x | y << 16);
}

uint PackInt16(min16uint2 v)
{
    return uint(v.x | (uint)(v.y) << 16);
}

min16int2 UnpackInt16(int v)
{
    uint x = asuint(v.x) & 0xFFFF;
    uint y = asuint(v.x) >> 16;
    return min16uint2(asint(x), asint(y));
}

min16uint2 UnpackInt16(uint v)
{
    return min16uint2(v.x & 0xFFFF, v.x >> 16);
}

// min16{u}int4
int2 PackInt16(min16int4 v)
{
    return int2(PackInt16(v.xy), PackInt16(v.zw));
}

uint2 PackInt16(min16uint4 v)
{
    return uint2(PackInt16(v.xy), PackInt16(v.zw));
}

min16int4 UnpackInt16(int2 v)
{
    return min16int4(UnpackInt16(v.x), UnpackInt16(v.y));
}

min16uint4 UnpackInt16(uint2 v)
{
    return min16uint4(UnpackInt16(v.x), UnpackInt16(v.y));
}

uint PackFloat16(min16float v)
{
    uint p = f32tof16(v);
    return p.x;
}

// min16float2
uint PackFloat16(min16float2 v)
{
    uint2 p = f32tof16(float2(v));
    return p.x | (p.y << 16);
}

min16float2 UnpackFloat16(uint a)
{
    float2 tmp = f16tof32(uint2(a & 0xFFFF, a >> 16));
    return min16float2(tmp);
}

// min16float4
uint2 PackFloat16(min16float4 v)
{
    return uint2(PackFloat16(v.xy), PackFloat16(v.zw));
}

min16float4 UnpackFloat16(uint2 v)
{
    return min16float4(UnpackFloat16(v.x), UnpackFloat16(v.y));
}

// Particle helpers
// ===================
float GetTextureOffset(uint emitterProperties)
{
    uint index = (emitterProperties & 0x001f00000) >> 24;

    return (float)index * 0.5;  // Assumes 2 textures in the atlas!
}

bool IsStreakEmitter(uint emitterProperties)
{
    return (emitterProperties >> 30) & 0x01 ? true : false;
}

bool IsReactiveEmitter(uint emitterProperties, uint flags)
{
    uint index = (emitterProperties & 0x000ff0000) >> 16;

    return (flags >> index) & 0x01 ? true : false;
}

#endif  // !__cplusplus
