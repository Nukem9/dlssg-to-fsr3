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

// Transforms origin to uv space
// Mat must be able to transform origin from its current space into clip space.
float3 ProjectPosition(float3 origin, float4x4 mat)
{
    float4 projected = mul(mat, float4(origin, 1));
    projected.xyz /= projected.w;
    projected.xy = 0.5 * projected.xy + 0.5;
    projected.y = (1 - projected.y);
    return projected.xyz;
}

// Origin and direction must be in the same space and mat must be able to transform from that space into clip space.
float3 ProjectDirection(float3 origin, float3 direction, float3 screen_space_origin, float4x4 mat) 
{
    float3 offsetted = ProjectPosition(origin + direction, mat);
    return offsetted - screen_space_origin;
}

// Mat must be able to transform origin from texture space to a linear space.
float3 InvProjectPosition(float3 coord, float4x4 mat) 
{
    coord.y = (1 - coord.y);
    coord.xy = 2 * coord.xy - 1;
    float4 projected = mul(mat, float4(coord, 1));
    projected.xyz /= projected.w;
    return projected.xyz;
}

float3 ScreenSpaceToViewSpace(float3 screen_uv_coord, float4x4 invProj)
{
    return InvProjectPosition(screen_uv_coord, invProj);
}

float3 ViewSpaceToWorldSpace(float4 view_space_coord, float4x4 invView)
{ 
    return mul(invView, view_space_coord).xyz;
}

float3 WorldSpaceToScreenSpacePrevious(float3 world_coord, float4x4 prevViewProj)
{ 
    return ProjectPosition(world_coord, prevViewProj);
}

float3 ScreenSpaceToWorldSpace(float3 screen_space_position, float4x4 invViewProj)
{
    return InvProjectPosition(screen_space_position, invViewProj);
}
