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

#ifndef FFX_BRIXELIZER_DEBUG_VISUALIZATION_H
#define FFX_BRIXELIZER_DEBUG_VISUALIZATION_H

#include "ffx_brixelizer_host_gpu_shared.h"

#define FFX_BRIXELIZER_TRAVERSAL_EPS               (GetDebugInfoPreviewSDFSolveEpsilon() / FfxFloat32(8.0))

#include "ffx_brixelizer_trace_ops.h"

// XXX -- tmp
FfxUInt32 FfxBrixelizerGetVoxelCascade(FfxUInt32 voxel_id)
{
    return voxel_id >> FFX_BRIXELIZER_CASCADE_ID_SHIFT;
}

#define FLT_INF 1e30f

// By Morgan McGuire @morgan3d, http://graphicscodex.com
// Reuse permitted under the BSD license.
// https://www.shadertoy.com/view/4dsSzr
FfxFloat32x3 FFX_HeatmapGradient(FfxFloat32 t)
{
    return clamp((pow(t, 1.5) * 0.8 + 0.2) * FfxFloat32x3(smoothstep(0.0, 0.35, t) + t * 0.5, smoothstep(0.5, 1.0, t), max(1.0 - t * 1.7, t * 7.0 - 6.0)), 0.0, 1.0);
}

// License?
FfxFloat32x3 FFX_RandomColor(FfxFloat32x2 uv) {
    uv                 = ffxFract(uv * FfxFloat32(15.718281828459045));
    FfxFloat32x3 seeds = FfxFloat32x3(FfxFloat32(0.123), FfxFloat32(0.456), FfxFloat32(0.789));
    seeds              = ffxFract((uv.x + FfxFloat32(0.5718281828459045) + seeds) * ((seeds + FFX_MODULO(uv.x, FfxFloat32(0.141592653589793))) * FfxFloat32(27.61803398875) + FfxFloat32(4.718281828459045)));
    seeds              = ffxFract((uv.y + FfxFloat32(0.5718281828459045) + seeds) * ((seeds + FFX_MODULO(uv.y, FfxFloat32(0.141592653589793))) * FfxFloat32(27.61803398875) + FfxFloat32(4.718281828459045)));
    seeds              = ffxFract((FfxFloat32(0.5718281828459045) + seeds) * ((seeds + FFX_MODULO(uv.x, FfxFloat32(0.141592653589793))) * FfxFloat32(27.61803398875) + FfxFloat32(4.718281828459045)));
    return seeds;
}

FfxFloat32x3 FFX_ViewSpaceToWorldSpace(FfxFloat32x4 view_space_coord)
{
    return FFX_TRANSFORM_VECTOR(GetDebugInfoInvView(), view_space_coord).xyz;
}

FfxFloat32x3 FFX_InvProjectPosition(FfxFloat32x3 coord, FfxFloat32x4x4 mat)
{
    coord.y  = (FfxFloat32(1.0) - coord.y);
    coord.xy = FfxFloat32(2.0) * coord.xy - 1;
    FfxFloat32x4 projected = FFX_TRANSFORM_VECTOR(mat, FfxFloat32x4(coord, 1));
    projected.xyz /= projected.w;
    return projected.xyz;
}

FfxFloat32x3 FFX_ScreenSpaceToViewSpace(FfxFloat32x3 screen_uv_coord)
{
    return FFX_InvProjectPosition(screen_uv_coord, GetDebugInfoInvProj());
}

FfxFloat32 FFX_HitEdgeDist(FfxFloat32x3 hit, FfxFloat32x3 boxMin, FfxFloat32x3 boxMax)
{
    FfxFloat32x3 a = min(abs(hit - boxMin), abs(hit - boxMax));
    return min(min(a.x + a.y, a.x + a.z), a.y + a.z);
}

// modified from inigo quilez
FfxFloat32 FFX_BoxHitDist(FfxUInt32x2 tid, FfxFloat32x3 ro, FfxFloat32x3 rd, FfxFloat32x3 boxMin, FfxFloat32x3 boxMax)
{
    FfxFloat32x3 halfSize = (boxMax - boxMin) / 2.0f;
    FfxFloat32x3 center = boxMin + halfSize;
    FfxFloat32x3 rop = ro - center;
    FfxFloat32x3 ird = 1.0f / rd;
    FfxFloat32x3 n = rop * ird;
    FfxFloat32x3 k = abs(ird) * halfSize;
    FfxFloat32x3 t1 = -n - k;
    FfxFloat32x3 t2 = -n + k;

    FfxFloat32 tNear = max(max(t1.x, t1.y), t1.z);
    FfxFloat32 tFar  = min(min(t2.x, t2.y), t2.z);

    if (tNear > tFar || tFar < 0.0f) return FLT_INF;

    FfxFloat32 nearEdgeDist = FFX_HitEdgeDist(ro + tNear * rd, boxMin, boxMax);
    FfxFloat32 farEdgeDist = FFX_HitEdgeDist(ro + tFar * rd, boxMin, boxMax);

    const FfxFloat32 nearClip = 0.1f;
    if (tNear > nearClip && nearEdgeDist / tNear < 0.001f) {
        return tNear;
    } else if (tFar > nearClip && farEdgeDist / tFar < 0.001f) {
        return tFar;
    }
    return FLT_INF;
}

void FfxBrixelizerDebugVisualization(FfxUInt32x2 tid)
{
    FfxUInt32 width, height;
    GetDebugOutputDimensions(width, height);
    FfxFloat32x2 uv = (FfxFloat32x2(tid.xy) + FfxFloat32(0.5).xx) / FfxFloat32x2(width, height);

    FfxFloat32x3 screen_uv_space_ray_origin = FfxFloat32x3(uv, FfxFloat32(0.5));
    FfxFloat32x3 view_space_ray             = FFX_ScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 view_space_ray_direction   = normalize(view_space_ray);
    FfxFloat32x3 ray_direction              = normalize(FFX_ViewSpaceToWorldSpace(FfxFloat32x4(view_space_ray_direction, FfxFloat32(0.0))));
    FfxFloat32x3 ray_origin                 = FFX_ViewSpaceToWorldSpace(FfxFloat32x4(0.0, 0.0, 0.0, 1.0));

    FfxBrixelizerRayDesc ray_desc;
    ray_desc.start_cascade_id = GetDebugInfoStartCascadeIndex();
    ray_desc.end_cascade_id   = GetDebugInfoEndCascadeIndex();
    ray_desc.t_min            = GetDebugInfoTMin();
    ray_desc.t_max            = GetDebugInfoTMax();
    ray_desc.origin           = ray_origin;
    ray_desc.direction        = ray_direction;

    FfxBrixelizerHitRaw hit_payload;
    FfxBoolean hit = FfxBrixelizerTraverseRaw(ray_desc, hit_payload);
    FfxFloat32 hit_dist = FLT_INF;
    FfxFloat32x3 out_color = FfxFloat32x3(0.0f, 0.0f, 0.0f);

    if (hit) {
        hit_dist = hit_payload.t;
        switch (GetDebugInfoDebugState()) {
        case FFX_BRIXELIZER_TRACE_DEBUG_MODE_DISTANCE: {
            FfxFloat32 dist = (hit_payload.t - ray_desc.t_min) / (ray_desc.t_max - ray_desc.t_min);
            out_color = FfxFloat32x3(0.0f, smoothstep(0.0f, 1.0f, dist), smoothstep(0.0f, 1.0f, 1.0f - dist));
            break;
        }
        case FFX_BRIXELIZER_TRACE_DEBUG_MODE_UVW: {
            FfxFloat32x3 uvw = FfxFloat32x3(
                FfxBrixelizerUnpackUnsigned8Bits((hit_payload.uvwc >> 0) & 0xff),
                FfxBrixelizerUnpackUnsigned8Bits((hit_payload.uvwc >> 8) & 0xff),
                FfxBrixelizerUnpackUnsigned8Bits((hit_payload.uvwc >> 16) & 0xff)
            );
            out_color = uvw;
            break;
        }
        case FFX_BRIXELIZER_TRACE_DEBUG_MODE_ITERATIONS:
            out_color = FFX_HeatmapGradient(FfxFloat32(hit_payload.iter_count) / FfxFloat32(64));
            break;
        case FFX_BRIXELIZER_TRACE_DEBUG_MODE_GRAD: {
            out_color = FfxBrixelizerGetHitNormal(hit_payload) * FfxFloat32(0.5) + FfxFloat32(0.5);
            break;
        }
        case FFX_BRIXELIZER_TRACE_DEBUG_MODE_BRICK_ID:
            out_color = FFX_RandomColor(FfxFloat32x2(FfxFloat32(hit_payload.brick_id % 256) / FfxFloat32(256.0), FfxFloat32((hit_payload.brick_id / 256) % 256) / FfxFloat32(256.0)));
            break;
        case FFX_BRIXELIZER_TRACE_DEBUG_MODE_CASCADE_ID: {
            FfxUInt32 voxel_id   = LoadBricksVoxelMap(FfxBrixelizerBrickGetIndex(hit_payload.brick_id));
            FfxUInt32 cascade_id = FfxBrixelizerGetVoxelCascade(voxel_id);
            out_color = FFX_RandomColor(FfxFloat32x2(FfxFloat32(cascade_id % 256) / FfxFloat32(256.0), FfxFloat32((cascade_id / 256) % 256) / FfxFloat32(256.0)));
            break;
        }
        }
    } else {
        out_color = FFX_HeatmapGradient(FfxFloat32(hit_payload.iter_count) / FfxFloat32(64));
    }

    FfxFloat32 aabb_hit_dist = FLT_INF;
    FfxFloat32x3 aabb_color = FfxFloat32x3(0.0f, 0.0f, 0.0f);

    FfxUInt32 num_debug_aabbs = min(GetDebugInfoMaxAABBs(), LoadContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_NUM_DEBUG_AABBS));

    for (FfxUInt32 i = 0; i < num_debug_aabbs; ++i) {
        FfxBrixelizerDebugAABB aabb = GetDebugAABB(i);

        FfxFloat32 this_hit_dist = FFX_BoxHitDist(tid, ray_desc.origin, ray_desc.direction, aabb.aabbMin, aabb.aabbMax);
        if (this_hit_dist < aabb_hit_dist) {
            aabb_hit_dist = this_hit_dist;
            aabb_color = aabb.color;
        }
    }

    if (aabb_hit_dist < hit_dist) {
        out_color = aabb_color;
    } else if (aabb_hit_dist < FLT_INF) {
        out_color = ffxLerp(out_color, aabb_color, 0.25f);
    }

    StoreDebugOutput(tid, out_color);   
}

#endif // ifndef FFX_BRIXELIZER_DEBUG_VISUALIZATION_H