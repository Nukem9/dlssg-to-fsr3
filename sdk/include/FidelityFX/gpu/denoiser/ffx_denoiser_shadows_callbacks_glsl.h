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

#include "ffx_denoiser_resources.h"

#if defined(FFX_GPU)
#include "ffx_core.h"

#if defined(DENOISER_SHADOWS_BIND_CB0_DENOISER_SHADOWS)
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_CB0_DENOISER_SHADOWS, std140) uniform cb0DenoiserShadows_t
    {
        FfxInt32x2 iBufferDimensions;
    } cb0DenoiserShadows;
#endif

#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS, std140) uniform cb1DenoiserShadows_t
    {
        FfxFloat32x3    fEye;
        FfxInt32        iFirstFrame;
        FfxInt32x2      iBufferDimensions;
        FfxFloat32x2    fInvBufferDimensions;
        FfxFloat32x2    fMotionVectorScale;
        FfxFloat32x2    normalsUnpackMul_unpackAdd;
        FfxFloat32Mat4    fProjectionInverse;
        FfxFloat32Mat4    fReprojectionMatrix;
        FfxFloat32Mat4    fViewProjectionInverse;
    } cb1DenoiserShadows;
#endif

#if defined(DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS)
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS, std140) uniform cb2DenoiserShadows_t
    {
        FfxFloat32Mat4  fProjectionInverse;
        FfxFloat32x2    fInvBufferDimensions;
        FfxFloat32x2    normalsUnpackMul_unpackAdd;
        FfxInt32x2      iBufferDimensions;
        FfxFloat32      fDepthSimilaritySigma;
    } cb2DenoiserShadows;
#endif

FfxInt32x2 BufferDimensions()
{
#if defined(DENOISER_SHADOWS_BIND_CB0_DENOISER_SHADOWS)
    return cb0DenoiserShadows.iBufferDimensions;
#elif defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
    return cb1DenoiserShadows.iBufferDimensions;
#elif defined(DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS)
    return cb2DenoiserShadows.iBufferDimensions;
#endif

    return FfxInt32x2(0, 0);
}

FfxFloat32x2 MotionVectorScale()
{
#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
    return cb1DenoiserShadows.fMotionVectorScale;
#endif
    return FfxFloat32x2(0, 0);
}

// SRVs
#if defined DENOISER_SHADOWS_BIND_SRV_INPUT_HIT_MASK_RESULTS
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_SRV_INPUT_HIT_MASK_RESULTS)
    uniform utexture2D r_hit_mask_results;
#endif
#if defined DENOISER_SHADOWS_BIND_SRV_DEPTH
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_SRV_DEPTH)
    uniform texture2D r_depth;
#endif
#if defined DENOISER_SHADOWS_BIND_SRV_VELOCITY
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_SRV_VELOCITY)
    uniform texture2D r_velocity;
#endif
#if defined DENOISER_SHADOWS_BIND_SRV_NORMAL
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_SRV_NORMAL)
    uniform texture2D r_normal;
#endif
#if defined DENOISER_SHADOWS_BIND_SRV_HISTORY
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_SRV_HISTORY)
    uniform texture2D r_history;
#endif
#if defined DENOISER_SHADOWS_BIND_SRV_PREVIOUS_DEPTH
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_SRV_PREVIOUS_DEPTH)
    uniform texture2D r_previous_depth;
#endif
#if defined DENOISER_SHADOWS_BIND_SRV_PREVIOUS_MOMENTS
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_SRV_PREVIOUS_MOMENTS)
    uniform texture2D r_previous_moments;
#endif
#if FFX_HALF
    #if defined DENOISER_SHADOWS_BIND_SRV_FILTER_INPUT
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_SRV_FILTER_INPUT)
    uniform texture2D r_filter_input;
    #endif
#endif

// UAV declarations
#if defined DENOISER_SHADOWS_BIND_UAV_SHADOW_MASK
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_UAV_SHADOW_MASK, std430) buffer rw_shadow_mask_t
    {
        FfxUInt32 data[];
    } rw_shadow_mask;
#endif
#if defined DENOISER_SHADOWS_BIND_UAV_RAYTRACER_RESULT
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_UAV_RAYTRACER_RESULT, std430) buffer rw_raytracer_result_t
    {
        FfxUInt32 data[];
    } rw_raytracer_result;
#endif
#if defined DENOISER_SHADOWS_BIND_UAV_TILE_METADATA
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_UAV_TILE_METADATA, std430) buffer rw_tile_metadata_t
    {
        FfxUInt32 data[];
    } rw_tile_metadata;
#endif
#if defined DENOISER_SHADOWS_BIND_UAV_REPROJECTION_RESULTS
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_UAV_REPROJECTION_RESULTS, rg32f)
    uniform image2D rw_reprojection_results;
#endif
#if defined DENOISER_SHADOWS_BIND_UAV_CURRENT_MOMENTS
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_UAV_CURRENT_MOMENTS, rgba32f)
    uniform image2D rw_current_moments;
#endif
#if defined DENOISER_SHADOWS_BIND_UAV_HISTORY
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_UAV_HISTORY, rg32f)
    uniform image2D rw_history;
#endif
#if defined DENOISER_SHADOWS_BIND_UAV_FILTER_OUTPUT
    layout (set = 0, binding = DENOISER_SHADOWS_BIND_UAV_FILTER_OUTPUT) writeonly
    uniform image2D rw_filter_output;
#endif

#define TILE_SIZE_X 8
#define TILE_SIZE_Y 4

FfxUInt32 LaneIdToBitShift(FfxUInt32x2 localID)
{
    return localID.y * TILE_SIZE_X + localID.x;
}

FfxBoolean WaveMaskToBool(FfxUInt32 mask, FfxUInt32x2 localID)
{
    return FfxBoolean((FfxUInt32(1) << LaneIdToBitShift(localID.xy)) & mask);
}


#if defined(DENOISER_SHADOWS_BIND_SRV_INPUT_HIT_MASK_RESULTS)
FfxBoolean HitsLight(FfxUInt32x2 did, FfxUInt32x2 gtid, FfxUInt32x2 gid)
{
    FfxUInt32 mask = texelFetch(r_hit_mask_results, ivec2(gid), 0).r;
    return !WaveMaskToBool(mask, gtid);
}
#endif // #if defined(DENOISER_SHADOWS_BIND_SRV_INPUT_HIT_MASK_RESULTS)

FfxFloat32 NormalsUnpackMul()
{
#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
    return cb1DenoiserShadows.normalsUnpackMul_unpackAdd[0];
#endif
#if defined(DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS)
    return cb2DenoiserShadows.normalsUnpackMul_unpackAdd[0];
#endif
    return 0;
}

FfxFloat32 NormalsUnpackAdd()
{
#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
    return cb1DenoiserShadows.normalsUnpackMul_unpackAdd[1];
#endif
#if defined(DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS)
    return cb2DenoiserShadows.normalsUnpackMul_unpackAdd[1];
#endif
    return 0;
}

#if defined(DENOISER_SHADOWS_BIND_UAV_SHADOW_MASK)
void StoreShadowMask(FfxUInt32 offset, FfxUInt32 value)
{
    rw_shadow_mask.data[offset] = value;
}
#endif // #if defined(DENOISER_SHADOWS_BIND_UAV_SHADOW_MASK)

#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
FfxFloat32Mat4 ViewProjectionInverse()
{
    return cb1DenoiserShadows.fViewProjectionInverse;
}
#endif // #if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)

#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
FfxFloat32Mat4 ReprojectionMatrix()
{
    return cb1DenoiserShadows.fReprojectionMatrix;
}
#endif // #if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)

FfxFloat32Mat4 ProjectionInverse()
{
#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
    return cb1DenoiserShadows.fProjectionInverse;
#endif
#if defined(DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS)
    return cb2DenoiserShadows.fProjectionInverse;
#else
    return FfxFloat32Mat4(0);
#endif
}


FfxFloat32x2 InvBufferDimensions()
{
#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
    return cb1DenoiserShadows.fInvBufferDimensions;
#elif defined(DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS)
    return cb2DenoiserShadows.fInvBufferDimensions;
#else
    return FfxFloat32x2(0, 0);
#endif
}

#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
FfxInt32 IsFirstFrame()
{
    return cb1DenoiserShadows.iFirstFrame;
}
#endif // #if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)

#if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)
FfxFloat32x3 Eye()
{
    return cb1DenoiserShadows.fEye;
}
#endif // #if defined(DENOISER_SHADOWS_BIND_CB1_DENOISER_SHADOWS)

FfxFloat32 LoadDepth(FfxInt32x2 p)
{
#if defined(DENOISER_SHADOWS_BIND_SRV_DEPTH)
    return texelFetch(r_depth, ivec2(p), 0).x;
#else
    return 0.f;
#endif
}

#if defined(DENOISER_SHADOWS_BIND_SRV_PREVIOUS_DEPTH)
FfxFloat32 LoadPreviousDepth(FfxInt32x2 p)
{
    return texelFetch(r_previous_depth, ivec2(p), 0).x;
}
#endif // #if defined(DENOISER_SHADOWS_BIND_SRV_PREVIOUS_DEPTH)

#if defined(DENOISER_SHADOWS_BIND_SRV_NORMAL)
FfxFloat32x3 LoadNormals(FfxUInt32x2 p)
{
    FfxFloat32x3 normal = texelFetch(r_normal, ivec2(p), 0).xyz;
    normal = normal * NormalsUnpackMul().xxx + NormalsUnpackAdd().xxx;
    return normalize(normal);
}
#endif // #if defined(DENOISER_SHADOWS_BIND_SRV_NORMAL)

#if defined(DENOISER_SHADOWS_BIND_SRV_VELOCITY)
FfxFloat32x2 LoadVelocity(FfxInt32x2 p)
{
    FfxFloat32x2 velocity = texelFetch(r_velocity, ivec2(p), 0).rg;
    return velocity * MotionVectorScale();
}
#endif // #if defined(DENOISER_SHADOWS_BIND_SRV_VELOCITY)

#if defined(DENOISER_SHADOWS_BIND_SRV_HISTORY)
layout (set = 0, binding = 1000) uniform sampler s_trilinerClamp;
FfxFloat32 LoadHistory(FfxFloat32x2 p)
{
    return FfxFloat32(textureLod(sampler2D(r_history, s_trilinerClamp), p, 0.0f).x);
}
#endif

#if defined(DENOISER_SHADOWS_BIND_SRV_PREVIOUS_MOMENTS)
FfxFloat32x3 LoadPreviousMomentsBuffer(FfxInt32x2 p)
{
    return texelFetch(r_previous_moments, ivec2(p), 0).xyz;
}
#endif // #if defined(DENOISER_SHADOWS_BIND_SRV_PREVIOUS_MOMENTS)

#if defined(DENOISER_SHADOWS_BIND_UAV_RAYTRACER_RESULT)
FfxUInt32 LoadRaytracedShadowMask(FfxUInt32 p)
{
    return rw_raytracer_result.data[p];
}
#endif // #if defined(DENOISER_SHADOWS_BIND_UAV_RAYTRACER_RESULT)

#if defined(DENOISER_SHADOWS_BIND_UAV_TILE_METADATA)
void StoreMetadata(FfxUInt32 p, FfxUInt32 val)
{
    rw_tile_metadata.data[p] = val;
}
#endif // #if defined(DENOISER_SHADOWS_BIND_UAV_TILE_METADATA)

#if defined(DENOISER_SHADOWS_BIND_UAV_CURRENT_MOMENTS)
void StoreMoments(FfxUInt32x2 p, FfxFloat32x3 val)
{
    imageStore(rw_current_moments, ivec2(p), FfxFloat32x4(val, 0));
}
#endif // #if defined(DENOISER_SHADOWS_BIND_UAV_CURRENT_MOMENTS)

#if defined(DENOISER_SHADOWS_BIND_UAV_REPROJECTION_RESULTS)
void StoreReprojectionResults(FfxUInt32x2 p, FfxFloat32x2 val)
{
    imageStore(rw_reprojection_results, ivec2(p), FfxFloat32x4(val, 0, 0));
}
#endif // #if defined(DENOISER_SHADOWS_BIND_UAV_REPROJECTION_RESULTS)

#if defined(DENOISER_SHADOWS_BIND_CB2_DENOISER_SHADOWS)
FfxFloat32 DepthSimilaritySigma()
{
    return cb2DenoiserShadows.fDepthSimilaritySigma;
}
#endif

#if FFX_HALF

#if defined(DENOISER_SHADOWS_BIND_SRV_FILTER_INPUT)
    FfxFloat16x2 LoadFilterInput(FfxUInt32x2 p)
    {
        return FfxFloat16x2(texelFetch(r_filter_input, ivec2(p), 0).xy);
    }
#endif // #if defined(DENOISER_SHADOWS_BIND_SRV_FILTER_INPUT)

#endif // #if FFX_HALF

FfxBoolean IsShadowReciever(FfxUInt32x2 p)
{
    FfxFloat32 depth = LoadDepth(FfxInt32x2(p));
    return (depth > 0.0f) && (depth < 1.0f);
}

#if defined(DENOISER_SHADOWS_BIND_UAV_TILE_METADATA)
FfxUInt32 LoadTileMetaData(FfxUInt32 p)
{
    return rw_tile_metadata.data[p];
}
#endif // #if defined(DENOISER_SHADOWS_BIND_UAV_TILE_METADATA)

void StoreHistory(FfxUInt32x2 p, FfxFloat32x2 val)
{
    #if defined(DENOISER_SHADOWS_BIND_UAV_HISTORY)
        imageStore(rw_history, ivec2(p), FfxFloat32x4(val, 0, 0));
    #endif // #if defined(DENOISER_SHADOWS_BIND_UAV_HISTORY)
}

void StoreFilterOutput(FfxUInt32x2 p, FfxFloat32 val)
{
    #if defined(DENOISER_SHADOWS_BIND_UAV_FILTER_OUTPUT)
        imageStore(rw_filter_output, ivec2(p), FfxFloat32x4(val, 0, 0, 0));
    #endif // #if defined(DENOISER_SHADOWS_BIND_UAV_FILTER_OUTPUT)
}

#endif // #if defined(FFX_GPU)
