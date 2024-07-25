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

#include "../shaders/raytracingcommon.hlsl"

static const RAY_FLAG k_opaqueFlags
= RAY_FLAG_FORCE_OPAQUE
| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER
| RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES;

//--------------------------------------------------------------------------------------
// Texture definitions
//--------------------------------------------------------------------------------------
StructuredBuffer<uint4> sb_tiles : register(t0);
Texture2D<float>  t2d_depth : register(t1);
Texture2D<float3> t2d_normals : register(t2);
Texture2D t2d_blueNoise : register(t3);

RaytracingAccelerationStructure ras_opaque : register(t4);
RWTexture2D<uint> rwt2d_rayHitResults : register(u0);

//--------------------------------------------------------------------------------------
// Constant Buffer
//--------------------------------------------------------------------------------------
cbuffer cbRayTracing : register(b0)
{
    float4 textureSize;
    float3 lightDir;
    float pad[1];
    float4 pixelThickness_bUseCascadesForRayT_noisePhase_sunSize;


    float4x4 viewToWorld;
};


bool TraceOpaque(RaytracingAccelerationStructure ras, RayDesc ray)
{
    RayQuery < k_opaqueFlags > q;

    q.TraceRayInline(
        ras,
        k_opaqueFlags,
        0xff,
        ray);

    q.Proceed();

    return q.CommittedStatus() != COMMITTED_NOTHING;
}

bool TraceShadows(
    uint2 localID,
        const Tile currentTile)
{
    const uint2 pixelCoord = currentTile.location * k_tileSize + localID;

    // use tile mask to decide what pixels will fire a ray
    const bool bActiveLane = WaveMaskToBool(currentTile.mask, localID);
    bool bRayHitSomething = true;
    if (bActiveLane)
    {
        const float depth = t2d_depth[pixelCoord];
        const float3 normal = normalize(t2d_normals[pixelCoord].xyz * 2 - 1.f);

        const float2 uv = pixelCoord * textureSize.zw;
        const float4 homogeneous = mul(viewToWorld, float4(2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f, (depth), 1));
        const float3 worldPos = homogeneous.xyz / homogeneous.w;

        RayDesc ray;
        ray.Origin = worldPos + normal * pixelThickness_bUseCascadesForRayT_noisePhase_sunSize.x;
        ray.Direction = -lightDir;
        ray.TMin = currentTile.minT;
        ray.TMax = currentTile.maxT;

        {
            const float2 noise = t2d_blueNoise[pixelCoord % 128].rg + pixelThickness_bUseCascadesForRayT_noisePhase_sunSize.z;

            ray.Direction = normalize(MapToCone(fmod(noise, 1), ray.Direction, pixelThickness_bUseCascadesForRayT_noisePhase_sunSize.w));
        }

        // reverse ray direction for better traversal
        if (pixelThickness_bUseCascadesForRayT_noisePhase_sunSize.y)
        {
            ray.Origin = ray.Origin + ray.Direction * (currentTile.maxT);
            ray.Direction = -ray.Direction;
            ray.TMin = 0;
            ray.TMax = currentTile.maxT - currentTile.minT;
        }

        bRayHitSomething = TraceOpaque(ras_opaque, ray);
    }

    return bRayHitSomething;
}

[numthreads(TILE_SIZE_X * TILE_SIZE_Y, 1, 1)]
void TraceOpaqueOnly(uint localIndex : SV_GroupIndex, uint3 groupID : SV_GroupID)
{
    const uint2 localID = FXX_Rmp8x8(localIndex);

    const Tile currentTile= Tile::FromUint(sb_tiles[groupID.x]);

    const bool bRayHitSomething = TraceShadows(
        localID,
        currentTile);

    const uint waveOutput = BoolToWaveMask(bRayHitSomething, localID);
    if (localIndex == 0)
    {
        const uint oldMask = rwt2d_rayHitResults[currentTile.location].x;
        // add results to mask
        rwt2d_rayHitResults[currentTile.location] = waveOutput & oldMask;
    }
}
