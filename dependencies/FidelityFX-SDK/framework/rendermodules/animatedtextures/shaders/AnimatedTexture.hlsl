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

cbuffer cb : register(b0)
{
    matrix  g_CurrentViewProjection;
    matrix  g_PreviousViewProjection;
    float2  g_CameraJitterCompensation;
    float   g_ScrollFactor;
    float   g_RotationFactor;
    int     g_Mode;
    int     pad0;
    int     pad1;
    int     pad2;
}


Texture2D       g_Texture       : register(t0);
SamplerState    g_Sampler       : register(s0);

struct VERTEX_OUT
{
    float4 CurrentPosition      : TEXCOORD0;
    float4 PreviousPosition     : TEXCOORD1;
    float3 TexCoord             : TEXCOORD2;
    float4 Position             : SV_POSITION;
};


VERTEX_OUT VSMain( uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID )
{
    VERTEX_OUT output = (VERTEX_OUT)0;

    const float2 offsets[ 4 ] =
    {
        float2( -1,  1 ),
        float2(  1,  1 ),
        float2( -1, -1 ),
        float2(  1, -1 ),
    };

    float2 offset = offsets[ vertexId ];
    float2 uv = (offset+1)*float2( instanceId == 0 ? -0.5 : 0.5, -0.5 );

    float4 worldPos = float4( offsets[ vertexId ], 0.0, 1.0 );

    worldPos.xyz += instanceId == 0 ? float3( -13, 1.5, 2 ) : float3( -13, 1.5, -2 );

    output.CurrentPosition = mul( g_CurrentViewProjection, worldPos );
    output.PreviousPosition = mul( g_PreviousViewProjection, worldPos );

    output.Position = output.CurrentPosition;

    output.TexCoord.xy = uv;
    output.TexCoord.z = instanceId;

    return output;
}

struct Output
{
    float4 finalColor : SV_TARGET0;
    float2 motionVectors : SV_TARGET1;
    float upscaleReactive : SV_TARGET2;
    float upscaleTransparencyAndComposition : SV_TARGET3;
};


float4 TextureLookup( int billboardIndex, float2 uv0 )
{
    float4 color = 1;

    if ( billboardIndex == 0 || g_Mode == 2 )
    {
        // Scrolling
        float2 uv = uv0;
        if ( g_Mode == 2 )
            uv += float2( -g_ScrollFactor, 0.0 );
        else
            uv += float2( -g_ScrollFactor, 0.5*g_ScrollFactor );

        color.rgb = g_Texture.SampleLevel( g_Sampler, uv, 0 ).rgb;
    }
    else if ( billboardIndex == 1 )
    {
        // Rotated UVs
        float s, c;
        sincos( g_RotationFactor, s, c );
        float2x2 rotation = { float2( c, s ), float2( -s, c ) };

        float2 rotatedUV = mul( rotation, uv0-float2( 0.5, -0.5) );
        color.rgb = g_Texture.SampleLevel( g_Sampler, rotatedUV, 0 ).rgb;
    }

    return color;
}


Output PSMain( VERTEX_OUT input )
{
    Output output = (Output)0;

    output.finalColor = TextureLookup( (int) (input.TexCoord.z + 0.5), input.TexCoord.xy);

    output.motionVectors = (input.PreviousPosition.xy / input.PreviousPosition.w) - (input.CurrentPosition.xy / input.CurrentPosition.w) + g_CameraJitterCompensation;
    output.motionVectors *= float2(0.5f, -0.5f);

    output.upscaleReactive = 0;                     // Nothing to write to the reactice mask. Color writes are off on this target anyway.
    output.upscaleTransparencyAndComposition = 1;   // Write a value into here to indicate the depth and motion vectors are as expected for a static object, but the surface contents are changing.

    return output;
}
