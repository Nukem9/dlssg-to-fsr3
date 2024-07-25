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

//
//  Shader code for rendering particles as simple quads using rasterization
//

#include "particlecommon.h"
#include "particlerendercommon.h"

struct PS_INPUT
{
    nointerpolation float4  ViewSpaceCentreAndRadius    : TEXCOORD0;
    float2                  TexCoord                    : TEXCOORD1;
    float3                  ViewPos                     : TEXCOORD2;
    nointerpolation float3  VelocityXYEmitterNdotL      : TEXCOORD3;
    nointerpolation float3  Extrusion                   : TEXCOORD4;
    nointerpolation float2  EllipsoidRadius             : TEXCOORD5;
    nointerpolation float   Reactive                    : TEXCOORD6;
    nointerpolation float4  Color                       : COLOR0;
    float4                  Position                    : SV_POSITION;
};


// The particle buffer data. Note this is only one half of the particle data - the data that is relevant to rendering as opposed to simulation
StructuredBuffer<GPUParticlePartA>      g_ParticleBufferA           : register( t0 );

// A buffer containing the pre-computed view space positions of the particles
StructuredBuffer<uint2>                 g_PackedViewSpacePositions  : register( t1 );

// The number of sorted particles
StructuredBuffer<int>                   g_NumParticlesBuffer         : register( t2 );

// The sorted index list of particles
StructuredBuffer<int>                   g_SortedIndexBuffer          : register( t3 );

// The texture atlas for the particles
Texture2D                               g_ParticleTexture            : register( t4 );

// The opaque scene depth buffer read as a texture
Texture2D<float>                        g_DepthTexture               : register( t5 );

SamplerState g_samClampLinear   : register( s0 );


// Vertex shader only path
PS_INPUT VS_StructuredBuffer( uint VertexId : SV_VertexID )
{
    PS_INPUT Output = (PS_INPUT)0;

    // Particle index 
    uint particleIndex = VertexId / 4;

    // Per-particle corner index
    uint cornerIndex = VertexId % 4;

    float xOffset = 0;

    const float2 offsets[ 4 ] =
    {
        float2( -1,  1 ),
        float2(  1,  1 ),
        float2( -1, -1 ),
        float2(  1, -1 ),
    };

    int NumParticles = g_NumParticlesBuffer[ 0 ];

    int index = g_SortedIndexBuffer[ NumParticles - particleIndex - 1 ];

    GPUParticlePartA pa = g_ParticleBufferA[ index ];

    float4 ViewSpaceCentreAndRadius = UnpackFloat16( g_PackedViewSpacePositions[ index ] );
    float4 VelocityXYEmitterNdotLRotation = UnpackFloat16( pa.m_PackedVelocityXYEmitterNDotLAndRotation );

    uint emitterProperties = pa.m_StreakLengthAndEmitterProperties;

    bool streaks = IsStreakEmitter( emitterProperties );

    float2 offset = offsets[ cornerIndex ];
    float2 uv = (offset+1)*float2( 0.25, 0.5 );
    uv.x += GetTextureOffset( emitterProperties );

    float radius = ViewSpaceCentreAndRadius.w;
    float3 cameraFacingPos;

    if ( streaks )
    {
        float2 viewSpaceVelocity = VelocityXYEmitterNdotLRotation.xy;

        float2 ellipsoidRadius = float2( radius, UnpackFloat16( pa.m_StreakLengthAndEmitterProperties ).x );

        float2 extrusionVector = viewSpaceVelocity;
        float2 tangentVector = float2( extrusionVector.y, -extrusionVector.x );
        float2x2 transform = float2x2( tangentVector, extrusionVector );

        Output.Extrusion.xy = extrusionVector;
        Output.Extrusion.z = 1.0;
        Output.EllipsoidRadius = ellipsoidRadius;

        cameraFacingPos = ViewSpaceCentreAndRadius.xyz;

        cameraFacingPos.xy += mul( offset * ellipsoidRadius, transform );
    }
    else
    {
        float s, c;
        sincos( VelocityXYEmitterNdotLRotation.w, s, c );
        float2x2 rotation = { float2( c, -s ), float2( s, c ) };

        offset = mul( offset, rotation );

        cameraFacingPos = ViewSpaceCentreAndRadius.xyz;
        cameraFacingPos.xy += radius * offset;
    }

#if defined(REACTIVE_FLAGS)
    bool reactive   = IsReactiveEmitter( emitterProperties, REACTIVE_FLAGS );
    Output.Reactive = reactive ? 1.0 : 0.0;
#else
    Output.Reactive = 0.0;
#endif  // REACTIVE_FLAGS

    Output.Position = mul( g_mProjection, float4( cameraFacingPos, 1 ) );

    Output.TexCoord = uv;
    Output.Color = UnpackFloat16( pa.m_PackedTintAndAlpha );
    Output.ViewSpaceCentreAndRadius = ViewSpaceCentreAndRadius;
    Output.VelocityXYEmitterNdotL = VelocityXYEmitterNdotLRotation.xyz;
    Output.ViewPos = cameraFacingPos;

    return Output;
}


struct PS_OUTPUT
{
    float4 color : SV_TARGET0;

#if defined(ADDITIONAL_TRANSLUCENT_OUTPUTS)
    // This will be a string that represents the outputs and bindings for them
    ADDITIONAL_TRANSLUCENT_OUTPUTS
#endif  // ADDITIONAL_TRANSLUCENT_OUTPUTS
};


// Ratserization path's pixel shader
PS_OUTPUT PS_Billboard( PS_INPUT In )
{
    PS_OUTPUT output = (PS_OUTPUT)0;

    // Retrieve the particle data
    float3 particleViewSpacePos = In.ViewSpaceCentreAndRadius.xyz;
    float  particleRadius = In.ViewSpaceCentreAndRadius.w;

    // Get the depth at this point in screen space
    float depth = g_DepthTexture.Load( uint3( In.Position.x, In.Position.y, 0 ) ).x;

    // Get viewspace position by generating a point in screen space at the depth of the depth buffer
    float4 viewSpacePos;
    viewSpacePos.x = In.Position.x / (float)g_ScreenWidth;
    viewSpacePos.y = 1 - ( In.Position.y / (float)g_ScreenHeight );
    viewSpacePos.xy = (2*viewSpacePos.xy) - 1;
    viewSpacePos.z = depth;
    viewSpacePos.w = 1;

    // ...then transform it into view space using the inverse projection matrix and a divide by W
    viewSpacePos = mul( g_mProjectionInv, viewSpacePos );
    viewSpacePos.xyz /= viewSpacePos.w;

    // Calculate the depth fade factor
    float depthFade = saturate( ( particleViewSpacePos.z - viewSpacePos.z ) / particleRadius );

    float4 albedo = 1;
    albedo.a = depthFade;

    // Read the texture atlas
    albedo *= g_ParticleTexture.SampleLevel( g_samClampLinear, In.TexCoord, 0 );	// 2d

                                                                                    // Multiply in the particle color
    output.color = albedo * In.Color;

    // Calculate the UV based the screen space position
    float3 n = 0;
    float2 uv;

    if ( In.Extrusion.z > 0.0 )
    {
        float2 ellipsoidRadius = In.EllipsoidRadius;

        float2 extrusionVector = In.Extrusion.xy;
        float2 tangentVector = float2( extrusionVector.y, -extrusionVector.x );
        float2x2 transform = float2x2( tangentVector, extrusionVector );

        float2 vecToCentre = In.ViewPos.xy - particleViewSpacePos.xy;
        vecToCentre = mul( transform, vecToCentre );

        uv = vecToCentre / ellipsoidRadius;
    }
    else
    {
        uv = (In.ViewPos.xy - particleViewSpacePos.xy ) / particleRadius;
    }

    // Scale and bias
    uv = (1+uv)*0.5;

    float pi = 3.1415926535897932384626433832795;

    n.x = -cos( pi * uv.x );
    n.y = -cos( pi * uv.y );
    n.z = sin( pi * length( uv ) );
    n = normalize( n );

    float ndotl = saturate( dot( g_SunDirectionVS.xyz, n ) );

    // Fetch the emitter's lighting term
    float emitterNdotL = In.VelocityXYEmitterNdotL.z;

    // Mix the particle lighting term with the emitter lighting
    ndotl = lerp( ndotl, emitterNdotL, 0.5 );

    // Ambient lighting plus directional lighting
    float3 lighting = g_AmbientColor.rgb + ndotl * g_SunColor.rgb;

    // Multiply lighting term in
    output.color.rgb *= lighting;

    float Alpha = 0.f;
    float ReactiveMask = 0.f;
    
    if ( In.Reactive > 0.0 )
    {
        ReactiveMask = max(output.color.r, max(output.color.g, output.color.b)) * albedo.a;
    }

#if defined(ADDITIONAL_TRANSLUCENT_EXPORTS)
    // This will be a string that represents what information to store if the optional outputs
    ADDITIONAL_TRANSLUCENT_EXPORTS
#endif  // ADDITIONAL_TRANSLUCENT_EXPORTS

    return output;
}
