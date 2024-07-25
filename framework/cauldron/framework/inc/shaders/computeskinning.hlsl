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

#include "surfacerendercommon.h"

cbuffer CBBones : register(b0)
{
    MatrixPair bones[MAX_NUM_BONES];
};

cbuffer CBStrides : register(b1)
{
    VertexStrides vertexStrides;
}

matrix GetCurrentSkinningMatrix(float4 Weights, uint4 Joints)
{
    matrix skinningMatrix =
        Weights.x * bones[Joints.x].Current +
        Weights.y * bones[Joints.y].Current +
        Weights.z * bones[Joints.z].Current +
        Weights.w * bones[Joints.w].Current;

    return skinningMatrix;
}
matrix GetPreviousSkinningMatrix(float4 Weights, uint4 Joints)
{
    matrix skinningMatrix =
        Weights.x * bones[Joints.x].Previous +
        Weights.y * bones[Joints.y].Previous +
        Weights.z * bones[Joints.z].Previous +
        Weights.w * bones[Joints.w].Previous;
    return skinningMatrix;
}

// Inputs
ByteAddressBuffer positions : register(t0);
ByteAddressBuffer normals :   register(t1);
ByteAddressBuffer weights :   register(t2);
ByteAddressBuffer joints :    register(t3);

// Outputs
RWByteAddressBuffer currentSkinnedPositions : register(u0);
RWByteAddressBuffer prevSkinnedPositions    : register(u1);
RWByteAddressBuffer currentSkinnedNormals   : register(u2);

//--------------------------------------------------------------------------------------
// Main function
//--------------------------------------------------------------------------------------
[numthreads(64, 1, 1)]
void MainCS(uint3 dtID : SV_DispatchThreadID)
{
    if (dtID.x >= vertexStrides.numVertices)
        return;

    const uint positionIndex = dtID.x * vertexStrides.positionStride;
    const uint normalIndex   = dtID.x * vertexStrides.normalStride;
    const uint weightsIndex  = dtID.x * vertexStrides.weights0Stride;
    const uint jointsIndex   = dtID.x * vertexStrides.joints0Stride;

    const float3 vertexPosition = asfloat(positions.Load3(positionIndex));
    const float3 vertexNormal   = DeCompressNormals(asfloat(normals.Load3(normalIndex)));
    const uint4  vertexJoints   = vertexStrides.joints0Stride == 8 ? uint4(asuint(joints.Load2(jointsIndex)), 0, 0) : asuint(joints.Load4(jointsIndex));
    const float4 vertexWeights  = asfloat(weights.Load4(weightsIndex));

    // Shorts
    uint4 jointsValues = vertexJoints;
    if (vertexStrides.joints0Stride == 8)
    {
        jointsValues.x = (vertexJoints.x & 0xFFFF);
        jointsValues.y = (vertexJoints.x >> 16);
        jointsValues.z = (vertexJoints.y & 0xFFFF);
        jointsValues.w = (vertexJoints.y >> 16);
    }

    matrix skinningMatrix = GetCurrentSkinningMatrix(vertexWeights, jointsValues);
    matrix prevSkinningMatrix = GetPreviousSkinningMatrix(vertexWeights, jointsValues);

    const float3 positionSkinned     = mul(skinningMatrix, float4(vertexPosition, 1)).xyz;
    const float3 normalSkinned       = normalize(mul(skinningMatrix, float4(vertexNormal, 1)).xyz);
    const float3 prevPositionSkinned = mul(prevSkinningMatrix, float4(vertexPosition, 1)).xyz;;

    currentSkinnedPositions.Store3(positionIndex, asuint(positionSkinned));
    currentSkinnedNormals.Store3(normalIndex, asuint(CompressNormals(normalSkinned)));
    prevSkinnedPositions.Store3(positionIndex, asuint(prevPositionSkinned));
}
