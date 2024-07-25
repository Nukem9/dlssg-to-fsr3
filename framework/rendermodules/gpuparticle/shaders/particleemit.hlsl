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

#include "simulationbindings.h"


// Emitter index has 8 bits
// Texture index has 5 bits
uint WriteEmitterProperties( uint emitterIndex, uint textureIndex, bool isStreakEmitter )
{
    uint properties = 0;

    properties |= (emitterIndex & 0xff) << 16;

    properties |= ( textureIndex & 0x1f ) << 24;

    if ( isStreakEmitter )
    {
        properties |= 1 << 30;
    }

    return properties;
}

groupshared int g_ldsNumParticlesAvailable;


// Emit particles, one per thread, in blocks of 1024 at a time
[numthreads(1024,1,1)]
void CS_Emit( uint3 localIdx : SV_GroupThreadID, uint3 globalIdx : SV_DispatchThreadID )
{
    if ( localIdx.x == 0 )
    {
        int maxParticles = min( g_MaxParticlesThisFrame, g_MaxParticles );
        g_ldsNumParticlesAvailable = clamp( g_DeadList[ 0 ], 0, maxParticles );
    }

    GroupMemoryBarrierWithGroupSync();

    // Check to make sure we don't emit more particles than we specified
    if ( globalIdx.x < g_ldsNumParticlesAvailable )
    {
        int numDeadParticles = 0;
        InterlockedAdd( g_DeadList[ 0 ], -1, numDeadParticles );

        if ( numDeadParticles > 0 && numDeadParticles <= g_MaxParticles )
        {
            // Initialize the particle data to zero to avoid any unexpected results
            GPUParticlePartA pa = (GPUParticlePartA)0;
            GPUParticlePartB pb = (GPUParticlePartB)0;

            // Generate some random numbers from reading the random texture
            float2 uv = float2( globalIdx.x / 1024.0, g_ElapsedTime );
            float3 randomValues0 = g_RandomBuffer.SampleLevel( g_samWrapPoint, uv, 0 ).xyz;

            float2 uv2 = float2( (globalIdx.x + 1) / 1024.0, g_ElapsedTime );
            float3 randomValues1 = g_RandomBuffer.SampleLevel( g_samWrapPoint, uv2, 0 ).xyz;

            float velocityMagnitude = length( g_vEmitterVelocity.xyz );

            pb.m_Position = g_vEmitterPosition.xyz + ( randomValues0.xyz * g_PositionVariance.xyz );

            pa.m_StreakLengthAndEmitterProperties = WriteEmitterProperties( g_EmitterIndex, g_TextureIndex, g_EmitterStreaks ? true : false );
            pa.m_CollisionCount = 0;

            pb.m_Mass = g_Mass;
            pb.m_Velocity = g_vEmitterVelocity.xyz + ( randomValues1.xyz * velocityMagnitude * g_VelocityVariance );
            pb.m_Lifespan = g_ParticleLifeSpan;
            pb.m_Age = pb.m_Lifespan;
            pb.m_StartSize = g_StartSize;
            pb.m_EndSize = g_EndSize;

            int index = g_DeadList[ numDeadParticles ];

            // Write the new particle state into the global particle buffer
            g_ParticleBufferA[ index ] = pa;
            g_ParticleBufferB[ index ] = pb;
        }
    }
}
