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



uint GetEmitterIndex( uint emitterProperties )
{
    return (emitterProperties >> 16) & 0xff;
}


bool IsSleeping( uint emitterProperties )
{
    return ( emitterProperties >> 31 ) & 0x01 ? true : false;
}


uint SetIsSleepingBit( uint properties )
{
    return properties | (1 << 31);
}


// Function to calculate the streak radius in X and Y given the particles radius and velocity
float2 calcEllipsoidRadius( float radius, float viewSpaceVelocitySpeed )
{
    float radiusY = radius * max( 1.0, 0.1*viewSpaceVelocitySpeed );
    return float2( radius, radiusY );
}


// Calculate the view space position given a point in screen space and a texel offset
float3 calcViewSpacePositionFromDepth( float2 normalizedScreenPosition, int2 texelOffset )
{
    float2 uv;

    // Add the texel offset to the normalized screen position
    normalizedScreenPosition.x += (float)texelOffset.x / (float)g_ScreenWidth;
    normalizedScreenPosition.y += (float)texelOffset.y / (float)g_ScreenHeight;

    // Scale, bias and convert to texel range
    uv.x = (0.5 + normalizedScreenPosition.x * 0.5) * (float)g_ScreenWidth; 
    uv.y = (1-(0.5 + normalizedScreenPosition.y * 0.5)) * (float)g_ScreenHeight; 

    // Fetch the depth value at this point
    float depth = g_DepthBuffer.Load( uint3( uv.x, uv.y, 0 ) ).x;

    // Generate a point in screen space with this depth
    float4 viewSpacePosOfDepthBuffer;
    viewSpacePosOfDepthBuffer.xy = normalizedScreenPosition.xy;
    viewSpacePosOfDepthBuffer.z = depth;
    viewSpacePosOfDepthBuffer.w = 1;

    // Transform into view space using the inverse projection matrix
    viewSpacePosOfDepthBuffer = mul( g_mProjectionInv, viewSpacePosOfDepthBuffer );
    viewSpacePosOfDepthBuffer.xyz /= viewSpacePosOfDepthBuffer.w;

    return viewSpacePosOfDepthBuffer.xyz;
}


// Simulate 256 particles per thread group, one thread per particle
[numthreads(256,1,1)]
void CS_Simulate( uint3 id : SV_DispatchThreadID )
{
    const float3 vGravity = float3( 0.0, -9.81, 0.0 );

    // Fetch the particle from the global buffer
    GPUParticlePartA pa = g_ParticleBufferA[ id.x ];
    GPUParticlePartB pb = g_ParticleBufferB[ id.x ];

    // If the partile is alive
    if ( pb.m_Age > 0.0f )
    {
        // Extract the individual emitter properties from the particle
        uint emitterProperties = pa.m_StreakLengthAndEmitterProperties;
        uint emitterIndex = GetEmitterIndex( emitterProperties );
        bool streaks = IsStreakEmitter( emitterProperties );
        float4 velocityXYEmitterNDotLAndRotation;// = UnpackFloat16( pa.m_PackedVelocityXYEmitterNDotLAndRotation );

                                                 // Age the particle by counting down from Lifespan to zero
        pb.m_Age -= g_FrameTime;

        // Update the rotation
        pa.m_Rotation += 0.24 * g_FrameTime;

        float3 vNewPosition = pb.m_Position;

        // Apply force due to gravity
        if ( !IsSleeping( emitterProperties ) )
        {
            pb.m_Velocity += pb.m_Mass * vGravity * g_FrameTime;

            // Apply a little bit of a wind force
            float3 windDir = float3( 1, 1, 0 );
            float windStrength = 0.1;

            pb.m_Velocity += normalize( windDir ) * windStrength * g_FrameTime;

            // Calculate the new position of the particle
            vNewPosition += pb.m_Velocity * g_FrameTime;
        }

        // Calculate the normalized age
        float fScaledLife = 1.0 - saturate( pb.m_Age / pb.m_Lifespan );

        // Calculate the size of the particle based on age
        float radius = lerp( pb.m_StartSize, pb.m_EndSize, fScaledLife );

        // By default, we are not going to kill the particle
        bool killParticle = false;

        if ( g_CollideParticles && g_FrameTime > 0.0 )
        {
            // Transform new position into view space
            float3 viewSpaceParticlePosition =  mul( g_mView, float4( vNewPosition, 1 ) ).xyz;

            // Also obtain screen space position
            float4 screenSpaceParticlePosition =  mul( g_mViewProjection, float4( vNewPosition, 1 ) );
            screenSpaceParticlePosition.xyz /= screenSpaceParticlePosition.w;

            // Only do depth buffer collisions if the particle is onscreen, otherwise assume no collisions
            if ( !IsSleeping( emitterProperties ) && screenSpaceParticlePosition.x > -1 && screenSpaceParticlePosition.x < 1 && screenSpaceParticlePosition.y > -1 && screenSpaceParticlePosition.y < 1 )
            {
                // Get the view space position of the depth buffer
                float3 viewSpacePosOfDepthBuffer = calcViewSpacePositionFromDepth( screenSpaceParticlePosition.xy, int2( 0, 0 ) );

                // If the particle view space position is behind the depth buffer, but not by more than the collision thickness, then a collision has occurred
                if ( ( viewSpaceParticlePosition.z < viewSpacePosOfDepthBuffer.z ) && ( viewSpaceParticlePosition.z > viewSpacePosOfDepthBuffer.z - g_CollisionThickness ) )
                {
                    // Generate the surface normal. Ideally, we would use the normals from the G-buffer as this would be more reliable than deriving them
                    float3 surfaceNormal;

                    // Take three points on the depth buffer
                    float3 p0 = viewSpacePosOfDepthBuffer;
                    float3 p1 = calcViewSpacePositionFromDepth( screenSpaceParticlePosition.xy, int2( 1, 0 ) );
                    float3 p2 = calcViewSpacePositionFromDepth( screenSpaceParticlePosition.xy, int2( 0, 1 ) );

                    // Generate the view space normal from the two vectors
                    float3 viewSpaceNormal = normalize( cross( p2 - p0, p1 - p0 ) );

                    // Transform into world space using the inverse view matrix
                    surfaceNormal = normalize( mul( g_mViewInv, float4(-viewSpaceNormal, 0) ).xyz );

                    // The velocity is reflected in the collision plane
                    float3 newVelocity = reflect( pb.m_Velocity, surfaceNormal );

                    // Update the velocity and apply some restitution
                    pb.m_Velocity = 0.3*newVelocity;

                    // Update the new collided position
                    vNewPosition = pb.m_Position + (pb.m_Velocity * g_FrameTime);

                    pa.m_CollisionCount++;
                }
            }
        }

        // Put particle to sleep if the velocity is small
        if ( g_EnableSleepState && pa.m_CollisionCount > 10 && length( pb.m_Velocity ) < 0.01 )
        {
            pa.m_StreakLengthAndEmitterProperties = SetIsSleepingBit( emitterProperties );
        }

        // If the position is below the floor, let's kill it now rather than wait for it to retire
        if ( vNewPosition.y < -10 )
        {
            killParticle = true;
        }

        // Write the new position
        pb.m_Position = vNewPosition;

        // Calculate the the distance to the eye for sorting in the rasterization path
        float3 vec = vNewPosition - g_EyePosition.xyz;
        pb.m_DistanceToEye = length( vec );

        // Lerp the color based on the age
        float4 color0 = g_StartColor[ emitterIndex ];
        float4 color1 = g_EndColor[ emitterIndex ];

        float4 tintAndAlpha = 0;

        tintAndAlpha = lerp( color0, color1, saturate(4*fScaledLife) );
        tintAndAlpha.a = pb.m_Age <= 0 ? 0 : tintAndAlpha.a;

        if ( g_ShowSleepingParticles && IsSleeping( emitterProperties ) )
        {
            tintAndAlpha.rgb = float3( 1, 0, 1 );
        }

        pa.m_PackedTintAndAlpha = PackFloat16( (min16float4)tintAndAlpha );

        // The emitter-based lighting models the emitter as a vertical cylinder
        float2 emitterNormal = normalize( vNewPosition.xz - g_EmitterLightingCenter[ emitterIndex ].xz );

        // Generate the lighting term for the emitter
        float emitterNdotL = saturate( dot( g_SunDirection.xz, emitterNormal ) + 0.5 );

        // Transform the velocity into view space
        float2 vsVelocity = mul( g_mView, float4( pb.m_Velocity.xyz, 0 ) ).xy;
        float viewSpaceSpeed = 10 * length( vsVelocity );
        float streakLength = calcEllipsoidRadius( radius, viewSpaceSpeed ).y;
        pa.m_StreakLengthAndEmitterProperties = PackFloat16( min16float2( streakLength, 0 ) );
        pa.m_StreakLengthAndEmitterProperties |= (0xffff0000 & emitterProperties);

        velocityXYEmitterNDotLAndRotation.xy = normalize( vsVelocity );
        velocityXYEmitterNDotLAndRotation.z = emitterNdotL;
        velocityXYEmitterNDotLAndRotation.w = pa.m_Rotation;

        pa.m_PackedVelocityXYEmitterNDotLAndRotation = PackFloat16( (min16float4)velocityXYEmitterNDotLAndRotation );

        // Pack the view spaced position and radius into a float4 buffer
        float4 viewSpacePositionAndRadius;

        viewSpacePositionAndRadius.xyz = mul( g_mView, float4( vNewPosition, 1 ) ).xyz;
        viewSpacePositionAndRadius.w = radius;

        g_PackedViewSpacePositions[ id.x ] = PackFloat16( (min16float4)viewSpacePositionAndRadius );

        // For streaked particles (the sparks), calculate the the max radius in XY and store in a buffer
        if ( streaks )
        {
            float2 r2 = calcEllipsoidRadius( radius, viewSpaceSpeed );
            g_MaxRadiusBuffer[ id.x ] = max( r2.x, r2.y );
        }
        else
        {
            // Not a streaked particle so will have rotation. When rotating, the particle has a max radius of the centre to the corner = sqrt( r^2 + r^2 )
            g_MaxRadiusBuffer[ id.x ] = 1.41 * radius;
        }

        // Dead particles are added to the dead list for recycling
        if ( pb.m_Age <= 0.0f || killParticle )
        {
            pb.m_Age = -1;

            uint dstIdx = 0;
            InterlockedAdd( g_DeadList[ 0 ], 1, dstIdx );
            g_DeadList[ dstIdx + 1 ] = id.x;
        }
        else
        {
            // Alive particles are added to the alive list
            int index = 0;
            InterlockedAdd( g_AliveParticleCount[ 0 ], 1, index );
            g_IndexBuffer[ index ] = id.x;
            g_DistanceBuffer[ index ] = pb.m_DistanceToEye;

            uint dstIdx = 0;
            // 6 indices per particle billboard
            InterlockedAdd( g_DrawArgs[ 0 ].IndexCountPerInstance, 6, dstIdx );
        }

        // Write the particle data back to the global particle buffer
        g_ParticleBufferA[ id.x ] = pa;
        g_ParticleBufferB[ id.x ] = pb;
    }
}


// Reset 256 particles per thread group, one thread per particle
// Also adds each particle to the dead list UAV
[numthreads(256,1,1)]
void CS_Reset( uint3 id : SV_DispatchThreadID, uint3 globalIdx : SV_DispatchThreadID )
{
    if ( globalIdx.x == 0 )
    {
        g_DeadList[ 0 ] = g_MaxParticles;
    }
    g_DeadList[ globalIdx.x + 1 ] = globalIdx.x;

    g_ParticleBufferA[ id.x ] = (GPUParticlePartA)0;
    g_ParticleBufferB[ id.x ] = (GPUParticlePartB)0;
}

[numthreads(1, 1, 1)]
void CS_ClearAliveCount(uint3 id : SV_DispatchThreadID)
{
    // Initialize the draw args and index buffer using the first thread in the Dispatch call
    g_DrawArgs[0].IndexCountPerInstance = 0; // Number of primitives reset to zero
    g_DrawArgs[0].InstanceCount = 1;
    g_DrawArgs[0].StartIndexLocation = 0;
    g_DrawArgs[0].BaseVertexLocation = 0;
    g_DrawArgs[0].StartInstanceLocation = 0;

    g_AliveParticleCount[0] = 0;
}
