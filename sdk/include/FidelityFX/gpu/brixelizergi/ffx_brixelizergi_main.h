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

#ifndef FFX_BRIXELIZER_GI_MAIN_H
#define FFX_BRIXELIZER_GI_MAIN_H

#include "../ffx_core.h"
#include "ffx_brixelizer_brick_common_private.h"

FFX_STATIC FfxFloat32 s_sdf_solve_eps;

#define FFX_BRIXELIZER_TRAVERSAL_EPS s_sdf_solve_eps
#include "ffx_brixelizergi_common.h"
#include "ffx_brixelizergi_probe_shading.h"
#include "ffx_brixelizer_trace_ops.h"

// Z-up
FfxFloat32x3 FfxBrixelizerGIUVToHemioct(FfxFloat32x2 e)
{
    e           = FfxFloat32(2.0) * e - FFX_BROADCAST_FLOAT32X2(FfxFloat32(1.0));
    FfxFloat32x2 temp = FfxFloat32x2(e.x + e.y, e.x - e.y) * FfxFloat32(0.5);
    FfxFloat32x3 v    = FfxFloat32x3(temp, FfxFloat32(1.0) - abs(temp.x) - abs(temp.y));
    return normalize(v);
}

FfxFloat32x2 FfxBrixelizerGIHemioctToUV(FfxFloat32x3 v)
{
    FfxFloat32x2 p = v.xy * (FfxFloat32(1.0) / (abs(v.x) + abs(v.y) + v.z));
    return FfxFloat32x2(p.x + p.y, p.x - p.y) * FfxFloat32(0.5) + FFX_BROADCAST_FLOAT32X2(FfxFloat32(0.5));
}

FfxFloat32x3x3 FfxBrixelizerGICreateTBN(FfxFloat32x3 N)
{
    FfxFloat32x3 U;
    if (abs(N.z) > FfxFloat32(0.0)) {
        FfxFloat32 k = ffxSqrt(N.y * N.y + N.z * N.z);
        U.x     = FfxFloat32(0.0);
        U.y     = -N.z / k;
        U.z     = N.y / k;
    } else {
        FfxFloat32 k = ffxSqrt(N.x * N.x + N.y * N.y);
        U.x     = N.y / k;
        U.y     = -N.x / k;
        U.z     = FfxFloat32(0.0);
    }
    FfxFloat32x3x3 TBN;
    TBN[0] = U;
    TBN[1] = cross(N, U);
    TBN[2] = N;
    return TBN;
}

FfxFloat32x3 FfxBrixelizerGITransform(FfxFloat32x3x3 TBN, FfxFloat32x3 direction)
{
    return direction.x * TBN[0] + direction.y * TBN[1] + direction.z * TBN[2]; // FFX_MATRIX_MULTIPLY(direction, TBN); //
}

FfxFloat32x2 FfxBrixelizerGIGetUV(FfxUInt32x2 tid)
{
    return (FfxFloat32x2(tid) + FFX_BROADCAST_FLOAT32X2(FfxFloat32(0.5))) / GetBufferDimensionsF32();
}

FfxUInt32x2 FfxBrixelizerGIRoundDown(FfxUInt32x2 p, FfxUInt32 v)
{
    return v * (p / v);
}

// From "Temporal Reprojection Anti-Aliasing"
// https://github.com/playdeadgames/temporal
/**********************************************************************
Copyright (c) [2015] [Playdead]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************************/
FfxFloat32x3 FfxBrixelizerGIClipAABB(FfxFloat32x3 aabb_min, FfxFloat32x3 aabb_max, FfxFloat32x3 prev_sample)
{
    // Main idea behind clipping - it prevents clustering when neighbor color space
    // is distant from history sample

    // Here we find intersection between color vector and aabb color box

    // Note: only clips towards aabb center
    FfxFloat32x3 aabb_center = 0.5 * (aabb_max + aabb_min);
    FfxFloat32x3 extent_clip = 0.5 * (aabb_max - aabb_min) + 0.001;

    // Find color vector
    FfxFloat32x3 color_vector = prev_sample - aabb_center;
    // Transform into clip space
    FfxFloat32x3 color_vector_clip = color_vector / extent_clip;
    // Find ffxMax absolute component
    color_vector_clip  = abs(color_vector_clip);
    FfxFloat32 max_abs_unit = ffxMax(ffxMax(color_vector_clip.x, color_vector_clip.y), color_vector_clip.z);

    if (max_abs_unit > 1.0) {
        return aabb_center + color_vector / max_abs_unit; // clip towards color vector
    } else {
        return prev_sample; // point is inside aabb
    }
}

FfxFloat32x3 FfxBrixelizerGIClipAABB(FfxFloat32x3 prev_sample, FfxFloat32x3 center, FfxFloat32 aabb_size)
{
    return FfxBrixelizerGIClipAABB(center - FFX_BROADCAST_FLOAT32X3(aabb_size), center + FFX_BROADCAST_FLOAT32X3(aabb_size), prev_sample);
}

// https://www.pcg-random.org/
FfxUInt32 FfxBrixelizerGIpcg(FfxUInt32 v)
{
    FfxUInt32 state = v * 747796405u + 2891336453u;
    FfxUInt32 word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

FfxUInt32 FfxBrixelizerGIPackNormalTo32bits(FfxFloat32x3 normal)
{
    FfxFloat32x2 octuv = FfxBrixelizerOctahedronToUV(normal);
    FfxUInt32   ix    = FfxUInt32(octuv.x * FfxFloat32(65535.0));
    FfxUInt32   iy    = FfxUInt32(octuv.y * FfxFloat32(65535.0));
    return ix | (iy << 16);
}

FfxFloat32x3 FfxBrixelizerGIUnpackNormalFrom32bits(FfxUInt32 payload)
{
    FfxFloat32 ox = FfxFloat32(payload & 0xffff) / FfxFloat32(65535.0);
    FfxFloat32 oy = FfxFloat32((payload >> 16) & 0xffff) / FfxFloat32(65535.0);
    return FfxBrixelizerUVToOctahedron(FfxFloat32x2(ox, oy));
}

FfxFloat32 FfxBrixelizerGIUnpackUnorm16(FfxUInt32 a)
{
    return FfxFloat32(a & 0xffffu) / FfxFloat32(255.0 * 255.0);
}

FfxUInt32 FfxBrixelizerGIPackUnorm16(FfxFloat32 a)
{
    return FfxUInt32(ffxSaturate(a) * FfxFloat32(255.0 * 255.0));
}

FfxFloat32 FfxBrixelizerGIUnpackUnorm8(FfxUInt32 a)
{
    return FfxFloat32(a & 0xffu) / 255.0;
}

FfxUInt32 FfxBrixelizerGIPackUnorm8(FfxFloat32 a)
{
    return FfxUInt32(ffxSaturate(a) * 255.0);
}

// Probes are spawned in screen space and placed on gbuffer
struct FfxBrixelizerGIProbeSpawnInfo
{
    FfxUInt32x2  seed_pixel;
    FfxFloat32x3 normal;
    FfxFloat32   depth;
    FfxFloat32   eps;
    FfxFloat32   pushoff;
};

FfxUInt32x4 FfxBrixelizerGIProbeSpawnInfoPack(in FfxBrixelizerGIProbeSpawnInfo p)
{
    FfxUInt32x4 pack = FfxUInt32x4(0, 0, 0, 0);
    pack.x |= p.seed_pixel.x;
    pack.x |= p.seed_pixel.y << 16u;
    pack.y = FfxBrixelizerGIPackNormalTo32bits(p.normal);
    pack.z = ffxAsUInt32(p.depth);
    pack.w = ffxPackF32(FfxFloat32x2(p.eps, p.pushoff));
    return pack;
}
FfxUInt32x4 FfxBrixelizerGIProbeSpawnInfoInvalidPack()
{
    FfxBrixelizerGIProbeSpawnInfo pinfo;

    pinfo.seed_pixel.x   = 0xffffu;
    pinfo.seed_pixel.y   = 0xffffu;
    pinfo.normal         = FFX_BROADCAST_FLOAT32X3(0.0);
    pinfo.depth          = FfxBrixelizerGIFrontendConstants_BackgroundDepth;
    pinfo.eps            = FfxFloat32(0.0);
    pinfo.pushoff        = FfxFloat32(0.0);
    return FfxBrixelizerGIProbeSpawnInfoPack(pinfo);
}
FfxBrixelizerGIProbeSpawnInfo FfxBrixelizerGIProbeSpawnInfoUnpack(FfxUInt32x4 pack)
{
    FfxBrixelizerGIProbeSpawnInfo pinfo;

    pinfo.seed_pixel.x   = pack.x & 0xffffu;
    pinfo.seed_pixel.y   = (pack.x >> 16u) & 0xffffu;
    pinfo.normal         = FfxBrixelizerGIUnpackNormalFrom32bits(pack.y);
    pinfo.depth          = ffxAsFloat(pack.z);
    FfxFloat32x2 hpack   = ffxUnpackF32(pack.w);
    pinfo.eps            = hpack.x;
    pinfo.pushoff        = hpack.y;
    return pinfo;
}
FfxFloat32x3 FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(in FfxBrixelizerGIProbeSpawnInfo p)
{
    FfxFloat32x2 uv = FfxBrixelizerGIGetUV(p.seed_pixel);
    return ffxGetWorldPosition(uv, p.depth);
}
FfxFloat32x3 FfxBrixelizerGIProbeSpawnInfoGetPrevSpawnPosition(in FfxBrixelizerGIProbeSpawnInfo p)
{
    FfxFloat32x2 uv = FfxBrixelizerGIGetUV(p.seed_pixel);
    return ffxGetPreviousWorldPosition(uv, p.depth);
}
FfxFloat32x3 FfxBrixelizerGIProbeSpawnInfoGetRayDirection(in FfxBrixelizerGIProbeSpawnInfo p, FfxUInt32x2 probe_coord, FfxUInt32x2 cell)
{
    FfxFloat32x2   xi            = SampleBlueNoise(probe_coord * 8u + cell, GetFrameIndex());
    FfxFloat32x2   hemioct_uv    = (FfxFloat32x2(cell) + xi) / 8.0;
    FfxFloat32x3   ray_direction = FfxBrixelizerGIUVToHemioct(hemioct_uv);
    FfxFloat32x3x3 TBN           = FfxBrixelizerGICreateTBN(p.normal);
    return FfxBrixelizerGITransform(TBN, ray_direction);
}
FfxFloat32x3 FfxBrixelizerGIProbeSpawnInfoGetRayUnjitteredDirection(in FfxBrixelizerGIProbeSpawnInfo p, FfxUInt32x2 cell)
{
    FfxFloat32x2   hemioct_uv    = (FfxFloat32x2(cell) + FFX_BROADCAST_FLOAT32X2(0.5)) / 8.0;
    FfxFloat32x3   ray_direction = FfxBrixelizerGIUVToHemioct(hemioct_uv);
    FfxFloat32x3x3 TBN           = FfxBrixelizerGICreateTBN(p.normal);
    return FfxBrixelizerGITransform(TBN, ray_direction);
}
FfxFloat32x3 FfxBrixelizerGIProbeSpawnInfoProjectOnHemisphere(in FfxBrixelizerGIProbeSpawnInfo p, FfxFloat32x3 dir)
{
    FfxFloat32x3x3 TBN  = FfxBrixelizerGICreateTBN(p.normal);
    FfxFloat32x3   proj = FfxFloat32x3(
        dot(dir, TBN[0]),
        dot(dir, TBN[1]),
        dot(dir, TBN[2]));
    return proj;
}
FfxBoolean FfxBrixelizerGIProbeSpawnInfoIsValid(in FfxBrixelizerGIProbeSpawnInfo p)
{
    return all(FFX_NOT_EQUAL(p.seed_pixel, FFX_BROADCAST_UINT32X2(0xffffu)));
}

FfxFloat32 FfxBrixelizerGIWeight(FfxFloat32x3 center_normal, FfxFloat32x3 center_world_position, FfxFloat32x3 test_normal, FfxFloat32x3 test_world_position, FfxFloat32 eps_size, FfxFloat32 power, FfxFloat32 normal_power)
{
    return ffxPow(ffxPow(ffxSaturate(dot(center_normal, test_normal)), normal_power) * ffxSaturate(FfxFloat32(1.0) - length(center_world_position - test_world_position) / eps_size), power);
}

FFX_MIN16_F FfxBrixelizerGIWeightMin16(FFX_MIN16_F3 center_normal, FfxFloat32x3 center_world_position, FFX_MIN16_F3 test_normal, FfxFloat32x3 test_world_position, FFX_MIN16_F eps_size, FFX_MIN16_F power)
{
    return ffxPow(ffxSaturate(dot(center_normal, test_normal)) * ffxSaturate(FFX_MIN16_F(1.0) - FFX_MIN16_F(length(center_world_position - test_world_position)) / eps_size), power);
}

FfxUInt32 FfxBrixelizerGIScreenProbes_FindClosestProbe(FfxUInt32x2 tid, FfxInt32x2 offset)
{
    FfxInt32x2 pos = FfxInt32x2(tid) + offset;

    if (any(FFX_LESS_THAN(pos, FFX_BROADCAST_UINT32X2(0))) || any(FFX_GREATER_THAN_EQUAL(pos, GetTileBufferDimensions())))
        return FFX_BRIXELIZER_GI_INVALID_ID;

    FfxUInt32      probe_idx = pos.x + pos.y * GetTileBufferDimensions().x;
    FfxBrixelizerGIProbeSpawnInfo pinfo     = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadTempProbeInfo(probe_idx));

    if (!FfxBrixelizerGIProbeSpawnInfoIsValid(pinfo))
        return FFX_BRIXELIZER_GI_INVALID_ID;

    return pos.x | (pos.y << 16u);
}

FfxFloat32 FfxBrixelizerGIGetFrameWeight(FfxUInt32 num_frames)
{
    return FfxFloat32(1.0) - FfxFloat32(1.0) / FfxFloat32(num_frames);
}

// Src: Hacker's Delight, Henry S. Warren, 2001
FfxFloat32 FfxBrixelizerGIRadicalInverse_VdC(FfxUInt32 bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return FfxFloat32(bits) * FfxFloat32(2.3283064365386963e-10); // / 0x100000000
}

FfxFloat32x2 FfxBrixelizerGIHammersley(FfxUInt32 i, FfxUInt32 N)
{
    return FfxFloat32x2(FfxFloat32(i) / FfxFloat32(N), FfxBrixelizerGIRadicalInverse_VdC(i));
}

// Copyright (c) 2018 Eric Heitz (the Authors).
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
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
FfxFloat32x3 FfxBrixelizerGISampleGGXVNDF(FfxFloat32x3 Ve, FfxFloat32 alpha_x, FfxFloat32 alpha_y, FfxFloat32 U1, FfxFloat32 U2)
{
    // Input Ve: view direction
    // Input alpha_x, alpha_y: roughness parameters
    // Input U1, U2: uniform random numbers
    // Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * ffxMax(0, dot(Ve, Ne)) * D(Ne) / Ve.z
    //
    //
    // Section 3.2: transforming the view direction to the hemisphere configuration
    FfxFloat32x3 Vh = normalize(FfxFloat32x3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    FfxFloat32  lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    FfxFloat32x3 T1    = lensq > 0 ? FfxFloat32x3(-Vh.y, Vh.x, 0) * ffxRsqrt(lensq) : FfxFloat32x3(1, 0, 0);
    FfxFloat32x3 T2    = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    FfxFloat32 r   = ffxSqrt(U1);
    FfxFloat32 phi = 2.0 * 3.14159265358979f * U2;
    FfxFloat32 t1  = r * cos(phi);
    FfxFloat32 t2  = r * sin(phi);
    FfxFloat32 s   = 0.5 * (1.0 + Vh.z);
    t2        = (1.0 - s) * ffxSqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    FfxFloat32x3 Nh = t1 * T1 + t2 * T2 + ffxSqrt(ffxMax(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    FfxFloat32x3 Ne = normalize(FfxFloat32x3(alpha_x * Nh.x, alpha_y * Nh.y, ffxMax(0.0, Nh.z)));
    return Ne;
}

FfxFloat32x3 FfxBrixelizerGISample_GGX_VNDF_Ellipsoid(FfxFloat32x3 Ve, FfxFloat32 alpha_x, FfxFloat32 alpha_y, FfxFloat32 U1, FfxFloat32 U2)
{
    return FfxBrixelizerGISampleGGXVNDF(Ve, alpha_x, alpha_y, U1, U2);
}

FfxFloat32x3 FfxBrixelizerGISample_GGX_VNDF_Hemisphere(FfxFloat32x3 Ve, FfxFloat32 alpha, FfxFloat32 U1, FfxFloat32 U2)
{
    return FfxBrixelizerGISample_GGX_VNDF_Ellipsoid(Ve, alpha, alpha, U1, U2);
}

FfxFloat32x3 FfxBrixelizerGISampleReflectionVector(FfxFloat32x3 view_direction, FfxFloat32x3 normal, FfxFloat32 roughness, FfxInt32x2 dispatch_thread_id, FfxUInt32x2 noise_coord)
{
    if (roughness < FfxFloat32(1.0e-3)) {
        return reflect(view_direction, normal);
    }

    FfxFloat32x3x3 tbn_transform      = FfxBrixelizerGICreateTBN(normal);
    FfxFloat32x3x3 inv_tbn_transform  = transpose(tbn_transform);
    FfxFloat32x3   view_direction_tbn = FfxBrixelizerGITransform(inv_tbn_transform, -view_direction);

    FfxFloat32x2 u = SampleBlueNoise(noise_coord, GetFrameIndex());

    FfxFloat32x3   sampled_normal_tbn      = FfxBrixelizerGISample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);
    FfxFloat32x3   reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);
    // Transform reflected_direction back to the initial space.

    return FfxBrixelizerGITransform(tbn_transform, reflected_direction_tbn);
}

FfxRay FfxBrixelizerGIGenReflectionRay(FfxUInt32x2 pixel_coordinate, FfxUInt32x2 noise_coord)
{
    FfxFloat32x2 uv                             = FfxBrixelizerGIGetUV(pixel_coordinate);
    FfxFloat32   z                              = LoadDepth(pixel_coordinate);
    FfxFloat32x3 screen_uv_space_ray_origin     = FfxFloat32x3(uv, z);
    FfxFloat32x3 view_space_ray                 = ffxScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 view_space_ray_direction       = normalize(view_space_ray);
    FfxFloat32x3 world_space_view_direction     = FFX_MATRIX_MULTIPLY(GetInverseViewMatrix(), FfxFloat32x4(view_space_ray_direction, FfxFloat32(0.0))).xyz;
    FfxFloat32x3 world_space_normal             = LoadWorldNormal(pixel_coordinate);
    FfxFloat32x3 world_space_origin             = FFX_MATRIX_MULTIPLY(GetInverseViewMatrix(), FfxFloat32x4(view_space_ray, FfxFloat32(1.0))).xyz;
    FfxFloat32x3 view_space_surface_normal      = FFX_MATRIX_MULTIPLY(GetViewMatrix(), FfxFloat32x4(normalize(world_space_normal), 0)).xyz;
    FfxFloat32   roughness                      = LoadRoughness(pixel_coordinate);
    FfxFloat32x3 view_space_reflected_direction = FfxBrixelizerGISampleReflectionVector(view_space_ray_direction, view_space_surface_normal, roughness, FfxInt32x2(pixel_coordinate), noise_coord);
    
    FfxFloat32x3 world_space_ray_direction = FFX_MATRIX_MULTIPLY(GetInverseViewMatrix(), FfxFloat32x4(view_space_reflected_direction, FfxFloat32(0.0))).xyz;

    FfxRay ray;

    ray.roughness       = roughness;
    ray.valid           = !ffxIsBackground(z);
    ray.normal          = world_space_normal;
    ray.origin          = world_space_origin;
    ray.direction       = normalize(world_space_ray_direction);
    ray.major_direction = normalize(FFX_MATRIX_MULTIPLY(GetInverseViewMatrix(), FfxFloat32x4(reflect(view_space_ray_direction, view_space_surface_normal), FfxFloat32(0.0))).xyz);
    ray.camera_pos      = ffxViewSpaceToWorldSpace(FfxFloat32x4(0.0, 0.0, 0.0, 1.0));

    return ray;
}

FfxBoolean FfxBrixelizerGIIsCheckerboard(FfxUInt32x2 tid)
{
    FfxUInt32x2 qid   = (tid / 8) % 4;
    qid               = ffxWaveReadLaneFirstU2(qid); // scalar
    FfxUInt32   qidx  = qid.x + qid.y * 4;
    FfxUInt32   seed  = LoadTempRandomSeed(tid / FfxUInt32(8));
    FfxUInt32   shift = seed % FfxUInt32(16);
    return qidx == shift;
}

////////////////////////////
// Interface functions /////
////////////////////////////
void FfxBrixelizerGIClearCounters()
{
    StoreRaySwapIndirectArgs(0, 0);
}

FfxFloat32 FfxBrixelizerGIGetEps(FfxFloat32x3 position)
{
    FfxBrixelizerCascadeInfo CINFO = GetCascadeInfo(GetTracingConstantsStartCascade());
    return GetTracingConstantsRayPushoff() * CINFO.voxel_size * ffxMax(FfxFloat32(1.0e-3), length(position - GetCameraPosition()));
}

void FfxBrixelizerGISpawnScreenProbes(FfxUInt32x2 tid)
{
    if (any(FFX_GREATER_THAN_EQUAL(tid, GetTileBufferDimensions())))
        return;

    FfxUInt32x2     probe_screen_offset = tid * FfxUInt32(8);
    FfxUInt32x2     probe_coord         = tid;
    FfxUInt32       probe_idx           = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;
    const FfxUInt32 max_num_points      = FfxUInt32(64);
    FfxUInt32x2     seed_pixel          = FFX_BROADCAST_UINT32X2(-1);
    FfxFloat32      depth               = FfxBrixelizerGIFrontendConstants_BackgroundDepth;
    const FfxUInt32 max_attempts        = FfxUInt32(8);

    {
        FfxUInt32 seed = FfxBrixelizerGIpcg(GetFrameIndex() + FfxBrixelizerGIpcg(probe_screen_offset.x + FfxBrixelizerGIpcg(probe_screen_offset.y)));
        StoreTempRandomSeed(tid, seed & FfxUInt32(0xff));
    }

    for (FfxUInt32 i = 0; i < max_attempts; i++) 
    {
        FfxFloat32x2 seed_jitter = FfxBrixelizerGIHammersley(
            (                                                                                                                                                 //
                FfxBrixelizerGIpcg(GetFrameIndex()) + FfxBrixelizerGIpcg(probe_screen_offset.x + FfxBrixelizerGIpcg(probe_screen_offset.y + FfxBrixelizerGIpcg(i))) //
                )                                                                                                                                             //
                & (max_num_points - FfxUInt32(1)),
            max_num_points);
        FfxUInt32x2 try_seed_pixel = probe_screen_offset + FfxUInt32x2(floor(FfxFloat32(8.0) * seed_jitter));
        FfxFloat32  try_depth      = LoadDepth(try_seed_pixel);
        if (ffxIsBackground(try_depth))
            continue;
        depth      = try_depth;
        seed_pixel = try_seed_pixel;

        break;
    }

    if (seed_pixel.x == FfxUInt32(-1)) 
    {
        StoreTempProbeInfo(probe_idx, FfxBrixelizerGIProbeSpawnInfoInvalidPack());
        return;
    }
    
    FfxFloat32x3 normal       = LoadWorldNormal(seed_pixel);
    FfxBoolean   is_sky_pixel = ffxIsBackground(depth);
    
    if (is_sky_pixel) 
    {
        StoreTempProbeInfo(probe_idx, FfxBrixelizerGIProbeSpawnInfoInvalidPack());
        return;
    }

    FfxBrixelizerGIProbeSpawnInfo pinfo;

    pinfo.seed_pixel         = seed_pixel;
    pinfo.normal             = normal;
    pinfo.depth              = depth;
    FfxUInt32  g_starting_cascade  = GetTracingConstantsStartCascade();
    FfxUInt32  g_end_cascade       = GetTracingConstantsEndCascade();
    FfxFloat32 ray_pushoff         = GetTracingConstantsRayPushoff();
    FfxFloat32 xi                  = FfxFloat32((FfxBrixelizerGIpcg(GetFrameIndex()) + FfxBrixelizerGIpcg(tid.x + FfxBrixelizerGIpcg(tid.y))) & 0xff) / FfxFloat32(255.0);
    pinfo.pushoff                  = ray_pushoff * FfxBrixelizerGIGetVoxelSize(FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(pinfo), g_starting_cascade, g_end_cascade, xi);
    FfxBrixelizerCascadeInfo CINFO = GetCascadeInfo(g_starting_cascade);
    pinfo.eps                      = FfxBrixelizerGIGetEps(FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(pinfo));
    StoreTempProbeInfo(probe_idx, FfxBrixelizerGIProbeSpawnInfoPack(pinfo));
}

FFX_GROUPSHARED FfxUInt32  lds_reprojected_probe_score;
FFX_GROUPSHARED FfxFloat32 lds_tile_weights[3 * 3];
FFX_GROUPSHARED FfxUInt32  lds_reprojected_reprojected_cnt;
FFX_GROUPSHARED FfxUInt32  lds_reprojected_probe_x[8u * 8u];
FFX_GROUPSHARED FfxUInt32  lds_reprojected_probe_y[8u * 8u];
FFX_GROUPSHARED FfxUInt32  lds_reprojected_probe_z[8u * 8u];
FFX_GROUPSHARED FfxUInt32  lds_reprojected_probe_w[8u * 8u];
FFX_GROUPSHARED FfxUInt32  lds_reprojected_probe_num_samples[8u * 8u];
FFX_GROUPSHARED FfxUInt32  lds_reprojected_probe_coords[8u * 8u];

void FfxBrixelizerGILDS_StoreRadiance(FfxInt32x2 c, FfxFloat32x4 r)
{
    FfxInt32 cell_idx                 = c.x + c.y * 8;
    lds_reprojected_probe_x[cell_idx] = ffxPackF32(r.xy);
    lds_reprojected_probe_y[cell_idx] = ffxPackF32(r.zw);
}

FfxFloat32x4 FfxBrixelizerGILDS_LoadRadiance(FfxInt32x2 c)
{
    FfxInt32 cell_idx = c.x + c.y * 8;
    return FfxFloat32x4(ffxUnpackF32(lds_reprojected_probe_x[cell_idx]), ffxUnpackF32(lds_reprojected_probe_y[cell_idx]));
}

#define FFX_BRIXELIZER_GI_RADIANCE_QUANTIZE_K FfxFloat32(1.0e4)
FfxUInt32x4 FfxBrixelizerGIQuantizeRadiance(FfxFloat32x4 radiance)
{
    return FfxUInt32x4(floor(FFX_BRIXELIZER_GI_RADIANCE_QUANTIZE_K * radiance));
}

FfxFloat32x4 FfxBrixelizerGIRecoverRadiance(FfxUInt32x4 quantized_radiance)
{
    return FfxFloat32x4(quantized_radiance) / FFX_BRIXELIZER_GI_RADIANCE_QUANTIZE_K;
}

FfxBoolean FfxBrixelizerGISampleWorldSDF(FfxFloat32x3 world_pos, inout FfxFloat32 voxel_size, inout FfxFloat32 sdf, inout FfxFloat32x3 grad)
{
    const FfxUInt32 g_starting_cascade = GetTracingConstantsStartCascade();
    const FfxUInt32 g_end_cascade      = GetTracingConstantsEndCascade();

    for (FfxUInt32 cascade_id = g_starting_cascade; cascade_id <= g_end_cascade; cascade_id++) 
    {
        FfxBrixelizerCascadeInfo CINFO = GetCascadeInfo(cascade_id);
        FfxFloat32               size  = CINFO.grid_max.x - CINFO.grid_min.x;
        
        if (all(FFX_GREATER_THAN(world_pos, CINFO.grid_min)) && all(FFX_LESS_THAN(world_pos, CINFO.grid_max))) 
        {
            FfxFloat32x3 rel_pos      = world_pos - CINFO.grid_min;
            FfxInt32x3   voxel_offset = FfxInt32x3(rel_pos / CINFO.voxel_size);
            FfxFloat32x3 uvw          = (rel_pos - FfxFloat32x3(voxel_offset) * CINFO.voxel_size) / CINFO.voxel_size;
            FfxUInt32    voxel_idx    = FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(FfxInt32x3(CINFO.clipmap_offset), FFX_BRIXELIZER_CASCADE_WRAP_MASK, FfxUInt32x3(voxel_offset)), FFX_BRIXELIZER_CASCADE_DEGREE);
            FfxUInt32    brick_id     = LoadCascadeBrickMapArrayUniform(cascade_id, voxel_idx);
            
            if (FfxBrixelizerIsValidID(brick_id)) 
            {
                FfxFloat32x3 brick_offset = FfxBrixelizerGetSDFAtlasOffset(brick_id);
                FfxFloat32x3 uvw_min      = (brick_offset + FFX_BROADCAST_FLOAT32X3(0.5)) / FfxFloat32(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE);
                FfxFloat32x3 uvw_max      = uvw_min + FFX_BROADCAST_FLOAT32X3(7.0) / FfxFloat32(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE);
                FfxFloat32x3 atlas_uvw    = ffxLerp(uvw_min, uvw_max, uvw);
                sdf                       = SampleSDFAtlas(atlas_uvw);
                grad                      = FfxBrixelizerGetBrixelGrad(uvw_min, uvw_max, uvw);
                voxel_size                = CINFO.voxel_size;
                return true;
            }
        }
    }
    return false;
}

FfxFloat32x2 FfxBrixelizerGIFindClosestMotionVector(FfxUInt32x2 tid)
{
    FfxFloat32x2 motion_vector;
    FfxFloat32   closest_depth = FfxBrixelizerGIFrontendConstants_BackgroundDepth;

    for (FfxInt32 y = -1; y <= 1; ++y) 
    {
        for (FfxInt32 x = -1; x <= 1; ++x) 
        {
            FfxInt32x2 coord = FfxInt32x2(tid) + FfxInt32x2(x, y);

            if (any(FFX_LESS_THAN(coord, FFX_BROADCAST_INT32X2(0))) || any(FFX_GREATER_THAN_EQUAL(coord, GetBufferDimensions())))
                continue;

            FfxFloat32 depth = LoadDepth(coord);

            if (FfxBrixelizerGIIsDepthACloserThanB(/* A */ depth, /* B */ closest_depth)) 
            {
                motion_vector = LoadMotionVector(coord);
                closest_depth = depth;
            }
        }
    }

    return motion_vector;
}

//  8x8 group
// This function reprojects the screen probes from the previous frame and does irradiance sharing iteration
void FfxBrixelizerGIReprojectScreenProbes(FfxUInt32x2 tid, FfxUInt32x2 gid)
{
    FfxUInt32x2 probe_screen_offset = FfxBrixelizerGIRoundDown(tid, 8u);
    FfxUInt32x2 probe_coord         = tid / 8u;
    probe_coord                     = ffxWaveReadLaneFirstU2(probe_coord); // scalar

    FfxUInt32      probe_idx   = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;
    FfxUInt32      sh_index    = gid.x + gid.y * 8u;
    FfxUInt32      cell_idx    = gid.x + gid.y * 8u;
    FfxUInt32      qidx        = (gid.x % 2) + (gid.y % 2) * 2;
    FfxBrixelizerGIProbeSpawnInfo pinfo       = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadTempProbeInfo(probe_idx));
    
    if (!FfxBrixelizerGIProbeSpawnInfoIsValid(pinfo)) // scalar
    { 
        if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X2(0)))) 
        {
            StoreTempSpawnMask(probe_coord, 0);
            StoreStaticScreenProbesStat(probe_coord, FFX_BROADCAST_FLOAT32X4(0.0));
        }
        StoreStaticScreenProbes(tid, FFX_BROADCAST_FLOAT32X4(0.0));
        return;
    }

    if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X2(0)))) 
    {
        lds_reprojected_probe_score     = 0xffffffff;//(FfxUInt32)-1;
        lds_reprojected_reprojected_cnt = 0;
    }

    lds_reprojected_probe_x[cell_idx]           = FfxUInt32(0);
    lds_reprojected_probe_y[cell_idx]           = FfxUInt32(0);
    lds_reprojected_probe_z[cell_idx]           = FfxUInt32(0);
    lds_reprojected_probe_w[cell_idx]           = FfxUInt32(0);
    lds_reprojected_probe_num_samples[cell_idx] = 0;
    FFX_GROUP_MEMORY_BARRIER;

    FfxFloat32x3 pixel_normal    = LoadWorldNormal(tid);
    FfxFloat32   pixel_depth     = LoadDepth(tid);
    FfxFloat32x2 pixel_uv        = FfxBrixelizerGIGetUV(tid);
    FfxFloat32x3 pixel_world_pos = ffxGetWorldPosition(pixel_uv, pixel_depth);

    FfxFloat32x3 probe_pos  = FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(pinfo);
    FfxFloat32   eps_size   = pinfo.eps * FfxFloat32(4.0); // ffxPow(FfxFloat32(2.0), pinfo.eps) / FfxFloat32(2.0);
    FfxFloat32x2 history_uv = FfxBrixelizerGIGetUV(tid) + FfxBrixelizerGIFindClosestMotionVector(tid);
    
    if (any(FFX_LESS_THAN(history_uv, FFX_BROADCAST_FLOAT32X2(0.0))) || any(FFX_GREATER_THAN(history_uv, FFX_BROADCAST_FLOAT32X2(1.0)))) 
    {
    } 
    else 
    {
        FfxUInt32x2    sample_pixel_coord = FfxUInt32x2(floor(history_uv * GetBufferDimensionsF32()));
        FfxUInt32x2    sample_probe_coord = sample_pixel_coord / 8u;
        FfxUInt32      sample_probe_idx   = sample_probe_coord.x + sample_probe_coord.y * GetTileBufferDimensions().x;
        FfxBrixelizerGIProbeSpawnInfo sample_pinfo       = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadStaticProbeInfo(sample_probe_idx));
        
        if (FfxBrixelizerGIProbeSpawnInfoIsValid(sample_pinfo)) 
        {
            FfxFloat32x3 sample_normal         = sample_pinfo.normal;
            FfxFloat32x3 sample_world_position = FfxBrixelizerGIProbeSpawnInfoGetPrevSpawnPosition(sample_pinfo);
            FfxFloat32   weight                = FfxBrixelizerGIWeight( //
                pinfo.normal,                //
                sample_world_position,       //
                pinfo.normal,                //
                probe_pos,                   //
                eps_size,                    //
                FfxFloat32(4.0),             //
                FfxFloat32(1.0)
            );
            if (weight > FfxFloat32(0.1)) 
            {
                FfxUInt32 pack = FfxBrixelizerGIPackUnorm16(weight) << 16u;
                
                pack |= cell_idx;
                lds_reprojected_probe_coords[cell_idx] = sample_probe_coord.x | (sample_probe_coord.y << 16);
                FFX_ATOMIC_MIN(lds_reprojected_probe_score, pack);
            }
        }
    }

    FFX_GROUP_MEMORY_BARRIER;
    
    FfxUInt32x2 cell = gid;

    if ((lds_reprojected_probe_score >> 16u) == 0xffffu) // fail; scalar
    { 
        if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X2(0)))) 
        {
            StoreTempSpawnMask(probe_coord, 0);
            StoreStaticScreenProbesStat(probe_coord, FFX_BROADCAST_FLOAT32X4(0.0));
        }
        StoreStaticScreenProbes(tid, FFX_BROADCAST_FLOAT32X4(0.0));
    } 
    else 
    {
        FfxUInt32    pack                    = lds_reprojected_probe_score;
        FfxUInt32    pack_probe_coords       = lds_reprojected_probe_coords[pack & 0xffffu];
        FfxUInt32x2  base_sample_probe_coord = FfxUInt32x2(pack_probe_coords & 0xffffu, (pack_probe_coords >> 16u) & 0xffffu);
        FfxInt32     radius                  = 1;
        FfxFloat32x4 sh_acc                  = FFX_BROADCAST_FLOAT32X4(0.0);
        FfxFloat32   weight_acc              = FfxFloat32(0.0);
        ////////////////////////
        // Irradiance sharing //
        ////////////////////////
        if (all(FFX_LESS_THAN(gid, FFX_BROADCAST_UINT32X2(3)))) 
        {
            FfxInt32   x                  = FfxInt32(gid.x) - 1;
            FfxInt32   y                  = FfxInt32(gid.y) - 1;
            FfxInt32x2 sample_probe_coord = FfxInt32x2(base_sample_probe_coord) + FfxInt32x2(x, y);

            if (any(FFX_LESS_THAN(sample_probe_coord, FFX_BROADCAST_INT32X2(0))) || any(FFX_GREATER_THAN_EQUAL(sample_probe_coord, GetTileBufferDimensions()))) 
            {
                lds_tile_weights[gid.x + gid.y * 3] = FfxFloat32(0.0);
            } 
            else 
            {
                FfxUInt32      sample_probe_idx = sample_probe_coord.x + sample_probe_coord.y * GetTileBufferDimensions().x;
                FfxBrixelizerGIProbeSpawnInfo sample_pinfo     = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadStaticProbeInfo(sample_probe_idx));

                if (FfxBrixelizerGIProbeSpawnInfoIsValid(sample_pinfo)) 
                {
                    FfxFloat32x3 sample_normal         = sample_pinfo.normal;
                    FfxFloat32x3 sample_world_position = FfxBrixelizerGIProbeSpawnInfoGetPrevSpawnPosition(sample_pinfo);
                    FfxFloat32   weight                = FfxBrixelizerGIWeight(pinfo.normal,                //
                                                                          sample_world_position,       //
                                                                          pinfo.normal,                //
                                                                          probe_pos,                   //
                                                                          eps_size * FfxFloat32(2.0),  //
                                                                          FfxFloat32(16.0),            //
                                                                          FfxFloat32(16.0)             //
                    );
                    lds_tile_weights[gid.x + gid.y * 3] = weight;
                } 
                else
                    lds_tile_weights[gid.x + gid.y * 3] = FfxFloat32(0.0);
            }
        }
        FFX_GROUP_MEMORY_BARRIER;

        for (FfxInt32 y = -radius; y <= radius; y++) // scalar 
        {     
            for (FfxInt32 x = -radius; x <= radius; x++) // scalar
            { 
                FfxFloat32 weight = lds_tile_weights[(x + 1) + (y + 1) * 3];
                if (weight < FfxFloat32(0.1))
                    continue; // scalar

                FfxInt32x2 sample_probe_coord = FfxInt32x2(base_sample_probe_coord) + FfxInt32x2(x, y);
                if (any(FFX_LESS_THAN(sample_probe_coord, FFX_BROADCAST_INT32X2(0))) || any(FFX_GREATER_THAN_EQUAL(sample_probe_coord, GetTileBufferDimensions())))
                    continue; // scalar

                FfxUInt32      sample_probe_idx = sample_probe_coord.x + sample_probe_coord.y * GetTileBufferDimensions().x;
                FfxBrixelizerGIProbeSpawnInfo sample_pinfo     = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadStaticProbeInfo(sample_probe_idx));
                
                FFX_ATOMIC_ADD(lds_reprojected_reprojected_cnt, 1u);
                weight_acc += weight;
                
                // Parallax corrected reprojection
                FfxFloat32x3 sample_world_position      = FfxBrixelizerGIProbeSpawnInfoGetPrevSpawnPosition(sample_pinfo);
                FfxFloat32x3 sample_ray_direction       = FfxBrixelizerGIProbeSpawnInfoGetRayUnjitteredDirection(sample_pinfo, cell);
                FfxFloat32x4 probe_radiance             = LoadStaticScreenProbesSRV(sample_probe_coord * 8u + cell);
                FfxFloat32x3 hit_point                  = sample_world_position + sample_ray_direction * probe_radiance.w;
                FfxFloat32x3 reprojected_dir            = hit_point - probe_pos;
                FfxFloat32x3 hemisphere_transformed_dir = FfxBrixelizerGIProbeSpawnInfoProjectOnHemisphere(pinfo, normalize(reprojected_dir));
                
                if (hemisphere_transformed_dir.z > FfxFloat32(0.0)) 
                {
                    FfxFloat32x2 uv                 = FfxBrixelizerGIHemioctToUV(hemisphere_transformed_dir);
                    uv = clamp(uv, 0.0f, 0.99f);
                    FfxUInt32x2  sample_cell_coord  = FfxUInt32x2(floor(uv * FfxFloat32(8.0)));
                    FfxFloat32   quantized_weight   = FfxFloat32(FfxUInt32(FfxFloat32(weight) * FfxFloat32(1.0e6))) * FfxFloat32(1.0e-6);
                    FfxUInt32x4  quantized_radiance = FfxBrixelizerGIQuantizeRadiance(quantized_weight * FfxFloat32x4(probe_radiance.xyz, length(reprojected_dir)));
                    FfxUInt32    sample_cell_idx    = sample_cell_coord.x + sample_cell_coord.y * 8u;
                    FFX_ATOMIC_ADD(lds_reprojected_probe_x[sample_cell_idx], quantized_radiance.x);
                    FFX_ATOMIC_ADD(lds_reprojected_probe_y[sample_cell_idx], quantized_radiance.y);
                    FFX_ATOMIC_ADD(lds_reprojected_probe_z[sample_cell_idx], quantized_radiance.z);
                    FFX_ATOMIC_ADD(lds_reprojected_probe_w[sample_cell_idx], quantized_radiance.w);
                    FFX_ATOMIC_ADD(lds_reprojected_probe_num_samples[sample_cell_idx], FfxUInt32(FfxFloat32(quantized_weight) * 1.0e6));
                }
                if (sh_index < 9)
                    sh_acc += weight * ffxUnpackF32x2(LoadStaticProbeSHBuffer(9 * sample_probe_idx + sh_index));
            }
        }

        FFX_GROUP_MEMORY_BARRIER;
        
        FfxBoolean any_reproj = lds_reprojected_reprojected_cnt != 0;

        if (any_reproj) // scalar
        { 
            if (sh_index < 9)
                StoreTempProbeSHBuffer(9 * probe_idx + sh_index, ffxPackF32x2(sh_acc / ffxMax(weight_acc, FfxFloat32(1.0e-3))));
            
            FfxFloat32x4 resolved_history = FfxBrixelizerGIRecoverRadiance(FfxUInt32x4(
                                          lds_reprojected_probe_x[cell_idx],
                                          lds_reprojected_probe_y[cell_idx],
                                          lds_reprojected_probe_z[cell_idx],
                                          lds_reprojected_probe_w[cell_idx]))
                                      / ffxMax(FfxFloat32(1.0e-6), FfxFloat32(lds_reprojected_probe_num_samples[cell_idx]) * FfxFloat32(1.0e-6));
            FFX_GROUP_MEMORY_BARRIER;
            
            FfxBrixelizerGILDS_StoreRadiance(FfxInt32x2(gid.xy), FfxFloat32x4(resolved_history.xyz, resolved_history.w < FfxFloat32(1.0e-3) ? FfxFloat32(0.0f) : FfxFloat32(1.0f)));

            FFX_GROUP_MEMORY_BARRIER;

            for (FfxInt32 i = FfxInt32(2); i <= FfxInt32(8); i = i * 2) 
            {
                FfxInt32 ox = FfxInt32(gid.x) * i;
                FfxInt32 oy = FfxInt32(gid.y) * i;
                FfxInt32 ix = FfxInt32(gid.x) * i + i / FfxInt32(2);
                FfxInt32 iy = FfxInt32(gid.y) * i + i / FfxInt32(2);
                
                if (ix < 8 && iy < 8) 
                {
                    FfxFloat32x4 rad_weight00 = FfxBrixelizerGILDS_LoadRadiance(FfxInt32x2(ox, oy));
                    FfxFloat32x4 rad_weight10 = FfxBrixelizerGILDS_LoadRadiance(FfxInt32x2(ox, iy));
                    FfxFloat32x4 rad_weight01 = FfxBrixelizerGILDS_LoadRadiance(FfxInt32x2(ix, oy));
                    FfxFloat32x4 rad_weight11 = FfxBrixelizerGILDS_LoadRadiance(FfxInt32x2(ix, iy));
                    FfxFloat32x4 sum          = rad_weight00 + rad_weight01 + rad_weight10 + rad_weight11;
                    FfxBrixelizerGILDS_StoreRadiance(FfxInt32x2(ox, oy), sum.xyzw);
                }

                FFX_GROUP_MEMORY_BARRIER;
            }

            FfxFloat32x4 stat = FfxBrixelizerGILDS_LoadRadiance(FfxInt32x2(0, 0));

            if (resolved_history.w < FfxFloat32(1.0e-1))
                resolved_history = stat.xyzw / ffxMax(FfxFloat32(1.0e-3), stat.w);

            StoreStaticScreenProbes(tid, resolved_history);

            if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X2(0)))) 
            {
                StoreStaticScreenProbesStat(probe_coord, stat.xyzw / ffxMax(FfxFloat32(1.0e-3), stat.w));
                StoreTempSpawnMask(probe_coord, 1);
            }
        } 
        else 
        {
            if (all(FFX_EQUAL(gid, FFX_BROADCAST_UINT32X2(0)))) 
            {
                StoreStaticScreenProbesStat(probe_coord, FFX_BROADCAST_FLOAT32X4(0));
                StoreTempSpawnMask(probe_coord, 0);
            }
            StoreStaticScreenProbes(tid, FFX_BROADCAST_FLOAT32X4(0.0));
        }
    }
}

struct RaySetup
{
    FfxUInt32x2  target_pixel_coord;
    FfxFloat32x3 ray_direction;
    FfxFloat32x3 ray_origin;
    FfxFloat32x3 normal;
    FfxBoolean   valid;
};

RaySetup FfxBrixelizerGIGetRaySetup(FfxUInt32x2 tid)
{
    FfxUInt32x2 cell                = tid % 8;
    FfxUInt32x2 target_pixel_coord  = tid;
    FfxUInt32x2 probe_screen_offset = FfxBrixelizerGIRoundDown(target_pixel_coord, 8u);
    FfxUInt32x2 probe_coord         = target_pixel_coord / 8u;
    FfxUInt32   probe_idx           = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;
    probe_idx                       = ffxWaveReadLaneFirstU1(probe_idx);
    FfxUInt32x4 probe_pack          = LoadTempProbeInfo(probe_idx);

    if (all(FFX_EQUAL(cell, FFX_BROADCAST_UINT32X2(0))))
        StoreStaticProbeInfo(probe_idx, probe_pack);

    FfxBrixelizerGIProbeSpawnInfo pinfo = FfxBrixelizerGIProbeSpawnInfoUnpack(probe_pack);

    FfxFloat32x3   ray_direction = FfxBrixelizerGIProbeSpawnInfoGetRayDirection(pinfo, probe_coord, cell);
    FfxFloat32x3   world_pos     = FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(pinfo);

    RaySetup rs;
    rs.ray_direction      = ray_direction;
    rs.ray_origin         = world_pos;
    rs.target_pixel_coord = target_pixel_coord;
    rs.normal             = pinfo.normal;
    rs.valid              = FfxBrixelizerGIProbeSpawnInfoIsValid(pinfo);

    return rs;
}

// 8x8 group or 2x32 waves
void FfxBrixelizerGIFillScreenProbes(FfxUInt32x2 tid)
{
    FfxUInt32x2     gid         = tid % 8;
    RaySetup        rs          = FfxBrixelizerGIGetRaySetup(tid);
    FfxUInt32x2     pixel_coord = rs.target_pixel_coord;
    FfxUInt32x2     probe_coord = pixel_coord / 8u;
    probe_coord                 = ffxWaveReadLaneFirstU2(probe_coord); // scalar
    FfxUInt32      probe_idx    = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;
    FfxBrixelizerGIProbeSpawnInfo pinfo        = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadStaticProbeInfo(probe_idx));

    if (!FfxBrixelizerGIProbeSpawnInfoIsValid(pinfo)) // scalar
        return;

    FfxFloat32x4 history = LoadStaticScreenProbesSRV(rs.target_pixel_coord);

    FfxBoolean successfull_reproj = LoadTempSpawnMask(probe_coord) == 1;
    FfxBoolean skip               = ffxWaveReadLaneFirstB1(!FfxBrixelizerGIIsCheckerboard(tid) && successfull_reproj);
    
    if (skip) // Scalar early out if reprojection was successful
    { 
        StoreStaticScreenProbes(rs.target_pixel_coord, history);
        return;
    }

    FfxBrixelizerHitRaw hit;

    FfxUInt32  g_starting_cascade = GetTracingConstantsStartCascade();
    FfxUInt32  g_end_cascade      = GetTracingConstantsEndCascade();
    FfxFloat32 total_pushoff      = pinfo.pushoff;

    FfxUInt32 start_cascade_idx = g_starting_cascade;

    FfxBrixelizerRayDesc ray;
    ray.start_cascade_id = start_cascade_idx;
    ray.end_cascade_id   = g_end_cascade;  
    ray.t_min            = GetTracingConstantsTMin();           
    ray.t_max            = GetTracingConstantsTMax();           
    ray.origin           = rs.ray_origin + (rs.normal * total_pushoff);
    ray.direction        = rs.ray_direction;

    FfxFloat32x3  cursor = ray.origin;

    FfxFloat32 origin_distance = length(ray.origin - GetCameraPosition());
    // leak check
    FfxFloat32x3 ray_origin_uvz = ffxProjectPosition(ray.origin, GetViewProjectionMatrix());
    ray_origin_uvz.xy           = ffxSaturate(ray_origin_uvz.xy); // clamp to border
    FfxFloat32 depth            = SampleDepth(ray_origin_uvz.xy);

    FfxFloat32x3 screen_world_pos  = ffxGetWorldPosition(ray_origin_uvz.xy, depth);
    FfxFloat32   screen_ray_length = length(screen_world_pos - GetCameraPosition());

    s_sdf_solve_eps = GetTracingConstantsSDFSolveEpsilon() / FfxFloat32(8.0);

    FfxBoolean   ray_hit  = FfxBrixelizerTraverseRaw(ray, hit); 
    FfxFloat32x4 radiance = FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0));
    FfxUInt32x2  pack     = FfxUInt32x2(hit.brick_id, hit.uvwc);
    FfxUInt32    brick_id = pack.x;

    {
        // XXX -- the following should be if (!ray_hit) { ... } else { ... }
        // but has been changed to if (!ray_hit) { ... } if (ray_hit) { ... }
        // to avoid a compiler bug resulting in a TDR on certain driver versions

        if (!ray_hit)
            radiance = FfxFloat32x4(SampleEnvironmentMap(rs.ray_direction), FfxFloat32(128.0));

        if (ray_hit)
        {
            FfxFloat32x3 uvw = FfxFloat32x3(                            //
                FfxBrixelizerUnpackUnsigned8Bits((pack.y >> 0) & 0xff), //
                FfxBrixelizerUnpackUnsigned8Bits((pack.y >> 8) & 0xff), //
                FfxBrixelizerUnpackUnsigned8Bits((pack.y >> 16) & 0xff) //
            );
            uvw += FFX_BROADCAST_FLOAT32X3(1.0 / 512.0);
            FfxUInt32                  voxel_id         = LoadBricksVoxelMap(FfxBrixelizerBrickGetIndex(brick_id));
            FfxUInt32                  cascade_id       = FfxBrixelizerGetVoxelCascade(voxel_id);
            FfxBrixelizerCascadeInfo   cinfo            = GetCascadeInfo(cascade_id);
            FfxUInt32                  voxel_offset     = FfxBrixelizerVoxelGetIndex(voxel_id);
            FfxUInt32x3                voxel_coord      = FfxBrixelizerUnflattenPOT(voxel_offset, FFX_BRIXELIZER_CASCADE_DEGREE);
            FfxFloat32x3               voxel_min        = FfxFloat32x3(voxel_coord) * cinfo.voxel_size + cinfo.grid_min;
            FfxFloat32x3               hit_world_offset = voxel_min + uvw * cinfo.voxel_size;
            FfxBxAtlasBounds atlas_bounds     = FfxBrixelizerGetAtlasBounds(brick_id);

            radiance.xyz = FfxBrixelizerGISampleRadianceCacheSH(brick_id, rs.ray_direction);

            radiance = FfxFloat32x4(radiance.xyz, length(hit_world_offset - rs.ray_origin));
        }
    }

    FfxFloat32 temporal_blend = FfxFloat32(1.0 / 4.0); // UPDATED!

    if (ffxAsUInt32(history.w) == FfxUInt32(0))
        temporal_blend = FfxFloat32(1.0);

    radiance.xyzw = ffxLerp(history.xyzw, radiance.xyzw, temporal_blend);
    
    FfxFloat32x4 stat = LoadStaticScreenProbesStat(probe_coord);

    if (any(FFX_NOT_EQUAL(ffxAsUInt32(stat), FFX_BROADCAST_UINT32X4(0))))
        radiance.xyz = FfxBrixelizerGIClipAABB(radiance.xyz, stat.xyz, FfxFloat32(0.3)); // crude firefly removal
    
    StoreStaticScreenProbes(rs.target_pixel_coord, radiance);
}

// Transform 8x8 octahedral encoding into spherical harmonics
void FfxBrixelizerGIReprojectGI(FfxUInt32x2 tid)
{
    if (all(FFX_LESS_THAN(tid, GetBufferDimensions()))) 
    {

        FfxFloat32x2 mv         = FfxBrixelizerGIFindClosestMotionVector(tid);
        FfxFloat32x2 history_uv = FfxBrixelizerGIGetUV(tid) + mv;
        
        if (any(FFX_LESS_THAN(history_uv, FFX_BROADCAST_FLOAT32X2(0.0))) || any(FFX_GREATER_THAN(history_uv, FFX_BROADCAST_FLOAT32X2(1.0)))) 
        {
            StoreStaticGITarget(tid, FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0)));
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
            StoreSpecularTarget(tid, FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0)));
#endif
            return;
        }

        FfxFloat32x2 history_uv_scaled = history_uv * GetBufferDimensionsF32();

        FfxFloat32x3 pixel_normal = LoadWorldNormal(tid);
        FfxFloat32   pixel_depth  = LoadDepth(tid);
        
        if (ffxIsBackground(pixel_depth)) 
        {
            StoreStaticGITarget(tid, FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0)));
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0            
            StoreSpecularTarget(tid, FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0)));
#endif            
            return;
        }

        FfxFloat32x2 pixel_uv        = FfxBrixelizerGIGetUV(tid);
        FfxFloat32x3 pixel_world_pos = ffxGetWorldPosition(pixel_uv, pixel_depth);

        FfxBoolean disoccluded = LoadDisocclusionMask(tid) > 0;

        if (disoccluded) 
        {
            StoreStaticGITarget(tid, FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0)));
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0            
            StoreSpecularTarget(tid, FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0)));
#endif            
        } 
        else 
        {
            StoreStaticGITarget(tid, SampleStaticGITargetSRV(history_uv));
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0            
            StoreSpecularTarget(tid, SampleSpecularTargetSRV(history_uv));
#endif            
        }
    }
}

FFX_GROUPSHARED FfxUInt32x2 lds_probe_sh_buffer[9 * 8 * 8]; // (1, 3, 5) coefficients for 3 bands of SH

// 8x8 group
FfxUInt32 FfxBrixelizerGIGetSHIndex(FfxUInt32x2 xy)
{
    return xy.x + xy.y * 8;
}

FfxUInt32 FfxBrixelizerGIGetSHLDSIndex(FfxUInt32x2 xy, FfxUInt32 sh_index)
{
    return 9 * FfxBrixelizerGIGetSHIndex(xy) + sh_index;
}

// Transform 8x8 octahedral encoding into spherical harmonics
// void FfxBrixelizerGIProjectScreenProbes(FfxUInt32 group_idx, FfxUInt32x2 gid)
void FfxBrixelizerGIProjectScreenProbes(FfxUInt32x2 tid, FfxUInt32x2 gid)
{
    FfxUInt32x2    probe_screen_offset = FfxBrixelizerGIRoundDown(tid, 8u);
    FfxUInt32x2    probe_coord         = tid / 8u;
    FfxUInt32      probe_idx           = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;
    FfxBrixelizerGIProbeSpawnInfo pinfo               = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadTempProbeInfo(probe_idx));

    if (!FfxBrixelizerGIProbeSpawnInfoIsValid(pinfo)) // scalar
        return;
    
    FfxFloat32x3 ray_direction = FfxBrixelizerGIProbeSpawnInfoGetRayDirection(pinfo, probe_coord, gid);
    FfxFloat32x3 world_pos     = FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(pinfo);
    FfxFloat32x4 radiance      = LoadStaticScreenProbesSRV(tid);

    FfxFloat32x4 stat = LoadStaticScreenProbesStat(probe_coord);

    if (all(FFX_EQUAL(ffxAsUInt32(radiance), FFX_BROADCAST_UINT32X4(0))))
        radiance = stat;

    FfxFloat32 direction_sh[9];
    FfxBrixelizerGISHGetCoefficients(ray_direction, direction_sh);

    FfxFloat32 c = dot(ray_direction, pinfo.normal);
    
    for (FfxUInt32 j = 0; j < 9; j++)
        lds_probe_sh_buffer[FfxBrixelizerGIGetSHLDSIndex(gid, j)] = ffxPackF32x2(FfxFloat32x4(direction_sh[j] * radiance.xyz * c, FfxFloat32(1.0)));

    FFX_GROUP_MEMORY_BARRIER;

    FfxUInt32 sh_offset = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;

    for (FfxInt32 sh_index = 0; sh_index < 9; sh_index++) 
    {
        for (FfxInt32 i = 0; i < 3; i++) 
        {
            FfxUInt32 stride = 1 << (i + 1);  // 2 4 8

            if (all(FFX_LESS_THAN(gid, FFX_BROADCAST_UINT32X2(8 / stride)))) // 4 2 1
            { 
                FFX_MIN16_F4 a00 = FFX_MIN16_F4(ffxUnpackF32x2(lds_probe_sh_buffer[FfxBrixelizerGIGetSHLDSIndex(gid.xy * stride + FfxUInt32x2(0, 0), sh_index)]));
                FFX_MIN16_F4 a10 = FFX_MIN16_F4(ffxUnpackF32x2(lds_probe_sh_buffer[FfxBrixelizerGIGetSHLDSIndex(gid.xy * stride + FfxUInt32x2(stride / 2, 0), sh_index)]));
                FFX_MIN16_F4 a01 = FFX_MIN16_F4(ffxUnpackF32x2(lds_probe_sh_buffer[FfxBrixelizerGIGetSHLDSIndex(gid.xy * stride + FfxUInt32x2(0, stride / 2), sh_index)]));
                FFX_MIN16_F4 a11 = FFX_MIN16_F4(ffxUnpackF32x2(lds_probe_sh_buffer[FfxBrixelizerGIGetSHLDSIndex(gid.xy * stride + FfxUInt32x2(stride / 2, stride / 2), sh_index)]));
                lds_probe_sh_buffer[FfxBrixelizerGIGetSHLDSIndex(gid.xy * stride, sh_index)] = ffxPackF32x2(FfxFloat32x4(a00 + a01 + a10 + a11));
            }
        }
    }
    FFX_GROUP_MEMORY_BARRIER;

    FfxUInt32 thread_sh_index = gid.x + gid.y * 8;
    
    if (thread_sh_index < 9) 
    {
        FfxFloat32x4 irradiance_history = ffxUnpackF32x2(LoadTempProbeSHBuffer(9 * sh_offset + thread_sh_index));
        FfxFloat32x4 irradiance_sh      = ffxUnpackF32x2(lds_probe_sh_buffer[FfxBrixelizerGIGetSHLDSIndex(FfxUInt32x2(0, 0), thread_sh_index)]);
        
        irradiance_sh.w = FfxFloat32(1.0);
        
        StoreTempProbeSHBuffer(9 * sh_offset + thread_sh_index, ffxPackF32x2(irradiance_sh));
    }
}

void FfxBrixelizerGIEmitIrradianceCache(FfxUInt32x2 tid)
{
    FfxUInt32           g_starting_cascade = GetTracingConstantsStartCascade();
    FfxUInt32           g_end_cascade      = GetTracingConstantsEndCascade();
    FfxUInt32x2         probe_coord        = tid;
    FfxUInt32           probe_idx          = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;
    FfxBrixelizerGIProbeSpawnInfo      pinfo              = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadTempProbeInfo(probe_idx));

    if (!FfxBrixelizerGIProbeSpawnInfoIsValid(pinfo)) // scalar
        return;
    
    FfxFloat32x3 world_pos = FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(pinfo);
    FfxFloat32x3 ray       = normalize(world_pos - GetCameraPosition());
    FfxFloat32   xi        = FfxFloat32(FfxBrixelizerGIpcg(tid.x + FfxBrixelizerGIpcg(tid.y + FfxBrixelizerGIpcg(GetFrameIndex()))) & 0xffu) / FfxFloat32(255.0);
    FfxUInt32    sh_offset = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;
    
    FfxFloat32x4 input_sh[9];

    for (FfxInt32 i = 0; i < 9; i++)
        input_sh[i] = ffxUnpackF32x2(LoadTempProbeSHBuffer(9 * sh_offset + i));

    FfxBrixelizerGIEmitIrradiance(world_pos, pinfo.normal, normalize(-ray + pinfo.normal), input_sh, xi, g_starting_cascade, g_end_cascade);

    for (FfxUInt32 i = 0; i < 9; i++)
        StoreStaticProbeSHBuffer(9 * sh_offset + i, ffxPackF32x2(input_sh[i]));
}

void FfxBrixelizerGIBlendSH(FfxUInt32x2 tid)
{
    if (any(FFX_GREATER_THAN_EQUAL(tid, GetTileBufferDimensions())))
        return;

    FfxInt32    radius = 0;
    FfxFloat32x4 acc[9];

    for (FfxUInt32 i = 0; i < 9; i++)
        acc[i] = FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0));

    FfxFloat32 weight_sum = FfxFloat32(0.0);
    
    for (FfxInt32 y = -radius; y <= radius; y++) 
    {
        for (FfxInt32 x = -radius; x <= radius; x++) 
        {
            FfxInt32x2 sample_coord = FfxInt32x2(tid) + FfxInt32x2(x, y);

            if (any(FFX_LESS_THAN(sample_coord, FFX_BROADCAST_INT32X2(0))) || any(FFX_GREATER_THAN(sample_coord, GetTileBufferDimensions())))
                continue;

            FfxUInt32  sh_offset = sample_coord.x + sample_coord.y * GetTileBufferDimensions().x;
            FfxFloat32 weight    = FfxFloat32(1.0) / (1.0 + FfxFloat32(x * x) + FfxFloat32(y * y));

            for (FfxUInt32 i = 0; i < 9; i++)
                acc[i] += weight * ffxUnpackF32x2(LoadTempProbeSHBuffer(9 * sh_offset + i));

            weight_sum += weight;
        }
    }

    FfxUInt32 sh_offset = tid.x + tid.y * GetTileBufferDimensions().x;

    for (FfxUInt32 i = 0; i < 9; i++) 
    {
        acc[i] /= ffxMax(FfxFloat32(1.0e-3), weight_sum);
        acc[i].w = FfxFloat32(1.0);

        StoreStaticProbeSHBuffer(9 * sh_offset + i, ffxPackF32x2(acc[i]));
    }
}

#define FfxBrixelizerGIMAX_SAMPLES FfxFloat32(64.0)
#define FfxBrixelizerGIMAX_SPECULAR_SAMPLES FfxFloat32(32.0)

FfxFloat32 FfxBrixelizerGIComputeTemporalVariance(FfxFloat32x3 history_radiance, FfxFloat32x3 radiance)
{
    FfxFloat32 history_luminance = FfxBrixelizerGIGetLuminance(history_radiance);
    FfxFloat32 luminance         = FfxBrixelizerGIGetLuminance(radiance);
    FfxFloat32 diff              = abs(history_luminance - luminance) / ffxMax(ffxMax(history_luminance, luminance), FfxFloat32(0.5));
    return diff * diff;
}

void FfxBrixelizerGISpecularPreTrace(FfxUInt32x2 quarter_res_tid)
{
    FfxUInt32       g_starting_cascade = GetTracingConstantsStartCascade();
    FfxUInt32       g_end_cascade      = GetTracingConstantsEndCascade();
    const FfxUInt32 max_num_points     = 64;
    FfxUInt32x2     full_res_tid       = quarter_res_tid * 4 + FfxUInt32x2(FfxBrixelizerGIHammersley( //
                                     (FfxBrixelizerGIpcg(GetFrameIndex()) + FfxBrixelizerGIpcg(quarter_res_tid.x + FfxBrixelizerGIpcg(quarter_res_tid.y))) & (max_num_points - 1),
                                     max_num_points)
                                 * FfxFloat32(4.0));

    FfxRay      base_ray      = FfxBrixelizerGIGenReflectionRay(full_res_tid, quarter_res_tid);
    FfxFloat32  ray_pushoff   = GetTracingConstantsSpecularRayPushoff();
    FfxFloat32  total_pushoff = ray_pushoff * FfxBrixelizerGIGetVoxelSize(base_ray.origin, g_starting_cascade, g_end_cascade, FfxFloat32(0.0));
    
    if (base_ray.valid) 
    {
        FfxBrixelizerRayDesc ray;
        ray.start_cascade_id = g_starting_cascade;
        ray.end_cascade_id   = g_end_cascade;  
        ray.t_min            = GetTracingConstantsTMin();           
        ray.t_max            = GetTracingConstantsTMax();  
        ray.direction        = base_ray.direction;
        ray.origin           = base_ray.origin + (base_ray.normal) * total_pushoff; // + aoray.direction
        s_sdf_solve_eps      = GetTracingConstantsSpecularSDFSolveEpsilon() / FfxFloat32(8.0);
        
        FfxBrixelizerHitRaw hit;         
        hit.brick_id      = FFX_BRIXELIZER_INVALID_ID;
        hit.uvwc          = 0;

        FfxBoolean out_of_budget = !FfxBrixelizerTraverseRaw(ray, hit);

        if (FfxBrixelizerIsValidID(hit.brick_id))
            StoreTempSpecularPretraceTarget(quarter_res_tid * FfxUInt32x2(1, 1), FfxUInt32x4((1 << 24u) | hit.brick_id, 0, 0, 0));
        else
            StoreTempSpecularPretraceTarget(quarter_res_tid * FfxUInt32x2(1, 1), FFX_BROADCAST_UINT32X4(out_of_budget ? 0xffffffffu : 0));

    } 
    else
        StoreTempSpecularPretraceTarget(quarter_res_tid * FfxUInt32x2(1, 1), FFX_BROADCAST_UINT32X4(0));
}

void PushSpecularRay(FfxUInt32x2 tid)
{
    FfxUInt32 pack = tid.x | (tid.y << 16u);
    FfxUInt32 offset;
    IncrementRaySwapIndirectArgs(0, 1, offset);
    StoreTempSpecularRaySwap(offset, pack);
}

FfxUInt32x2 LoadSpecularRay(FfxUInt32 tid)
{
    FfxUInt32 pack = LoadTempSpecularRaySwap(tid);
    return FfxUInt32x2(pack & 0xffffu, (pack >> 16u) & 0xffffu);
}

void FfxBrixelizerGISpecularTrace(FfxUInt32x2 tid)
{
    FfxFloat32 xi = FfxFloat32((FfxBrixelizerGIpcg(GetFrameIndex()) + FfxBrixelizerGIpcg(tid.x + FfxBrixelizerGIpcg(tid.y))) & 0xff) / FfxFloat32(255.0);

    FfxRay      base_ray           = FfxBrixelizerGIGenReflectionRay(tid, tid);
    FfxFloat32  ray_pushoff        = GetTracingConstantsSpecularRayPushoff();
    FfxUInt32   g_starting_cascade = GetTracingConstantsStartCascade() + 2;
    FfxUInt32   g_end_cascade      = GetTracingConstantsEndCascade();
    FfxFloat32  total_pushoff      = ray_pushoff * FfxBrixelizerGIGetVoxelSize(base_ray.origin, g_starting_cascade, g_end_cascade, FfxFloat32(0.0));
    
    s_sdf_solve_eps = GetTracingConstantsSpecularSDFSolveEpsilon() / FfxFloat32(8.0);
    
    FfxFloat32x3    ray_direction  = base_ray.direction;
    FfxFloat32x3    ray_origin     = base_ray.origin + (base_ray.normal) * total_pushoff;
    FfxFloat32x3    ray_idirection = FfxFloat32(1.0) / ray_direction;
    const FfxInt32  NUM_TAP_COORDS = 5;

    FfxInt32x2 coords[NUM_TAP_COORDS] = {
        FfxInt32x2(0, 0),  //
        FfxInt32x2(1, 0),  //
        FfxInt32x2(-1, 0), //
        FfxInt32x2(0, 1),  //
        FfxInt32x2(0, -1), //
    };
    const FfxUInt32 MAX_TAPS = 5;
    
    FfxUInt32x2 packs[MAX_TAPS];
    FfxUInt32   num_samples = 0;

    FfxInt32x2 xy = FfxInt32x2(tid / 4);
    FfxUInt32x4 pretrace = LoadTempSpecularPretraceTarget(xy * FfxUInt32x2(1, 1));

    if (pretrace.x != 0 && pretrace.x != 0xffffffffu) 
    {
        FfxUInt32                brick_id        = pretrace.x & 0x00ffffffu;
        FfxUInt32                voxel_id        = LoadBricksVoxelMap(FfxBrixelizerBrickGetIndex(brick_id));
        FfxUInt32                cascade_id      = FfxBrixelizerGetVoxelCascade(voxel_id);
        FfxBrixelizerCascadeInfo CINFO           = GetCascadeInfoNonUniform(cascade_id);
        FfxUInt32                voxel_offset    = FfxBrixelizerVoxelGetIndex(voxel_id);
        FfxUInt32x3              voxel_coord     = FfxBrixelizerUnflattenPOT(voxel_offset, FFX_BRIXELIZER_CASCADE_DEGREE);
        FfxFloat32x3             voxel_min       = FfxFloat32x3(voxel_coord) * CINFO.voxel_size + CINFO.grid_min;
        FfxUInt32                brick_aabb_pack = LoadBricksAABB(FfxBrixelizerBrickGetIndex(brick_id));
        FfxUInt32x3              brick_aabb_umin = FfxBrixelizerUnflattenPOT(brick_aabb_pack & ((1 << 9) - 1), 3);
        FfxUInt32x3              brick_aabb_umax = FfxBrixelizerUnflattenPOT((brick_aabb_pack >> 9) & ((1 << 9) - 1), 3) + FFX_BROADCAST_UINT32X3(1);
        FfxFloat32x3             brick_aabb_min  = voxel_min - FFX_BROADCAST_FLOAT32X3(CINFO.voxel_size / FfxFloat32(2.0 * 7.0)) + FfxFloat32x3(brick_aabb_umin) * (CINFO.voxel_size / FfxFloat32(7.0));
        FfxFloat32x3             brick_aabb_max  = voxel_min - FFX_BROADCAST_FLOAT32X3(CINFO.voxel_size / FfxFloat32(2.0 * 7.0)) + FfxFloat32x3(brick_aabb_umax) * (CINFO.voxel_size / FfxFloat32(7.0));
        
        brick_aabb_min           = clamp(brick_aabb_min, voxel_min, voxel_min + FFX_BROADCAST_FLOAT32X3(CINFO.voxel_size));
        brick_aabb_max           = clamp(brick_aabb_max, voxel_min, voxel_min + FFX_BROADCAST_FLOAT32X3(CINFO.voxel_size));
        FfxFloat32 brick_hit_min = FfxFloat32(0.0);
        FfxFloat32 brick_hit_max = FfxFloat32(0.0);
        
        if (FfxBrixelizerIntersectAABB(ray_origin, ray_idirection, brick_aabb_min, brick_aabb_max,
                /* out */ brick_hit_min,
                /* out */ brick_hit_max))
        {
            FfxFloat32x3 ray_cursor   = ray_origin + ray_direction * (brick_hit_min);
            FfxFloat32x3 uvw          = (ray_cursor - voxel_min) * CINFO.ivoxel_size;
            FfxFloat32   dist         = FfxFloat32(1.0);
            FfxFloat32x3 brick_offset = FfxBrixelizerGetSDFAtlasOffset(brick_id);
            FfxFloat32x3 uvw_min      = (brick_offset + FFX_BROADCAST_FLOAT32X3(FfxFloat32(0.5))) / FfxFloat32(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE);
            FfxFloat32x3 uvw_max      = uvw_min + FFX_BROADCAST_FLOAT32X3(FfxFloat32(7.0)) / FfxFloat32(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE);
            
            for (FfxUInt32 i = 0; i < 8; i++) 
            {
                dist = FfxBrixelizerSampleBrixelDistance(uvw_min, uvw_max, uvw) - FFX_BRIXELIZER_TRAVERSAL_EPS;
                
                if (dist < FFX_BRIXELIZER_TRAVERSAL_EPS) 
                {
                    StoreSpecularTarget(tid, FfxFloat32x4(FfxBrixelizerGISampleRadianceCache(brick_id, uvw), FfxFloat32(1.0)));
                    return;
                }
                
                uvw += ray_direction * dist;
                
                if (any(FFX_GREATER_THAN(abs(uvw - FFX_BROADCAST_FLOAT32X3(0.5)), FFX_BROADCAST_FLOAT32X3(0.501))))
                    break;
            }
        }
        else        
            StoreSpecularTarget(tid, FfxFloat32x4(0.0, 0.0, 0.0, 0.0));
    } 
    else        
        StoreSpecularTarget(tid, FfxFloat32x4(0.0, 0.0, 0.0, 0.0));
}

void FfxBrixelizerGISpecularLoadNeighborhood(
    FfxInt32x2       pixel_coordinate,
    out FFX_MIN16_F4 radiance,
    out FFX_MIN16_F3 normal,
    out FfxFloat32   depth,
    FfxUInt32x2      screen_size)
{
    radiance        = FFX_MIN16_F4(LoadSpecularTarget(pixel_coordinate));
    normal          = FFX_MIN16_F3(LoadWorldNormal(pixel_coordinate));
    FfxFloat32x2 uv = (pixel_coordinate.xy + (0.5).xx) / FfxFloat32x2(screen_size.xy);
    depth           = LoadDepth(pixel_coordinate);
}

FFX_GROUPSHARED FfxUInt32x4 g_ffx_dnsr_shared[16][16];
FFX_GROUPSHARED FfxFloat32 g_ffx_dnsr_shared_depth[16][16];

struct FfxBrixelizerGISpecularNeighborhoodSample
{
    FFX_MIN16_F4 radiance;
    FFX_MIN16_F3 normal;
    FfxFloat32   depth;
};

FfxFloat32x3 SpecularNeighborhoodSampleGetWorldPos(in FfxBrixelizerGISpecularNeighborhoodSample s, FfxFloat32x2 uv)
{
    return ffxGetWorldPosition(uv, s.depth);
}

FfxBrixelizerGISpecularNeighborhoodSample FfxBrixelizerGISpecularLoadFromGroupSharedMemory(FfxInt32x2 idx)
{
    FfxUInt32x2  packed_radiance          = FfxUInt32x2(g_ffx_dnsr_shared[idx.y][idx.x].x, g_ffx_dnsr_shared[idx.y][idx.x].y);
    FFX_MIN16_F4 unpacked_radiance        = FFX_MIN16_F4(ffxUnpackF32x2(packed_radiance));
    FfxUInt32x2  packed_normal_variance   = FfxUInt32x2(g_ffx_dnsr_shared[idx.y][idx.x].z, g_ffx_dnsr_shared[idx.y][idx.x].w);
    FFX_MIN16_F4 unpacked_normal_variance = FFX_MIN16_F4(ffxUnpackF32x2(packed_normal_variance));

    FfxBrixelizerGISpecularNeighborhoodSample specSample;
    specSample.radiance = unpacked_radiance.xyzw;
    specSample.normal   = unpacked_normal_variance.xyz;
    specSample.depth    = g_ffx_dnsr_shared_depth[idx.y][idx.x];
    return specSample;
}

void FfxBrixelizerGISpecularStoreInGroupSharedMemory(FfxInt32x2 group_thread_id, FFX_MIN16_F4 radiance, FFX_MIN16_F3 normal, FfxFloat32 depth)
{
    g_ffx_dnsr_shared[group_thread_id.y][group_thread_id.x].x     = ffxPackF32(FfxFloat32x2(radiance.xy));
    g_ffx_dnsr_shared[group_thread_id.y][group_thread_id.x].y     = ffxPackF32(FfxFloat32x2(radiance.zw));
    g_ffx_dnsr_shared[group_thread_id.y][group_thread_id.x].z     = ffxPackF32(FfxFloat32x2(normal.xy));
    g_ffx_dnsr_shared[group_thread_id.y][group_thread_id.x].w     = ffxPackF32(FfxFloat32x2(normal.z, FFX_MIN16_F(0.0)));
    g_ffx_dnsr_shared_depth[group_thread_id.y][group_thread_id.x] = depth;
}

void FfxBrixelizerGISpecularInitializeGroupSharedMemory(FfxInt32x2 dispatch_thread_id, FfxInt32x2 group_thread_id, FfxInt32x2 screen_size)
{
    // Load 16x16 region into shared memory using 4 8x8 blocks.
    FfxInt32x2 offset[4] = { FfxInt32x2(0, 0), FfxInt32x2(8, 0), FfxInt32x2(0, 8), FfxInt32x2(8, 8) };

    // Intermediate storage registers to cache the result of all loads
    FFX_MIN16_F4 radiance[4];
    FFX_MIN16_F3 normal[4];
    FfxFloat32 depth[4];

    // Start in the upper left corner of the 16x16 region.
    dispatch_thread_id -= 4;

    // First store all loads in registers
    for (FfxInt32 i = 0; i < 4; ++i)
        FfxBrixelizerGISpecularLoadNeighborhood(dispatch_thread_id + offset[i], radiance[i], normal[i], depth[i], screen_size);

    // Then move all registers to FFX_GROUPSHARED memory
    for (FfxInt32 j = 0; j < 4; ++j)
        FfxBrixelizerGISpecularStoreInGroupSharedMemory(group_thread_id + offset[j], radiance[j], normal[j], depth[j]); // X
}

FFX_MIN16_F3 FfxBrixelizerGISampleWorldGI(FfxFloat32x3 world_pos, FfxFloat32 xi, FFX_MIN16_F cosine_sh[9])
{
    FfxUInt32  g_starting_cascade = GetTracingConstantsStartCascade();
    FfxUInt32  g_end_cascade      = GetTracingConstantsEndCascade();

    FFX_MIN16_F4 probe_sh[9];
    
    if (FfxBrixelizerGIInterpolateBrickSH(world_pos, g_starting_cascade, g_end_cascade, xi, /* inout */ probe_sh)) 
    {
        FFX_MIN16_F3 cur_irradiance = FFX_BROADCAST_MIN_FLOAT16X3(0.0);

        for (FfxUInt32 i = 0; i < 9; i++)
            cur_irradiance += cosine_sh[i] * probe_sh[i].xyz;

        return cur_irradiance / FFX_MIN16_F(FFX_BRIXELIZER_GI_PI * FfxFloat32(2.0));
    }

    return FFX_BROADCAST_MIN_FLOAT16X3(FFX_MIN16_F(0.0));
}

// 8x8 group
// Project 2x2 closest SH probes onto GBuffer normals, use irradiance ambient cache when projection fails
void FfxBrixelizerGIInterpolateScreenProbes(FfxUInt32x2 tid, FfxUInt32x2 gid)
{
    FfxUInt32x2    probe_screen_offset = FfxBrixelizerGIRoundDown(tid, 8u);
    FfxUInt32x2    probe_coord         = tid / 8u;
    FfxUInt32      probe_idx           = probe_coord.x + probe_coord.y * GetTileBufferDimensions().x;
    FfxBrixelizerGIProbeSpawnInfo pinfo               = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadTempProbeInfo(probe_idx));

    FfxFloat32x3 pixel_normal       = LoadWorldNormal(tid);
    FfxFloat32   pixel_depth        = LoadDepth(tid);
    FfxFloat32x2 pixel_uv           = FfxBrixelizerGIGetUV(tid);
    FfxFloat32x3 world_pos          = ffxGetWorldPosition(pixel_uv, pixel_depth);
    FfxFloat32x3 view               = normalize(world_pos - GetCameraPosition());
    FfxFloat32   eps_size           = FfxBrixelizerGIGetEps(world_pos);
    FfxBoolean   is_sky_pixel       = ffxIsBackground(pixel_depth);
    FfxUInt32    g_starting_cascade = GetTracingConstantsStartCascade();
    FfxUInt32    g_end_cascade      = GetTracingConstantsEndCascade();
    FfxFloat32 xi = FfxFloat32(FfxBrixelizerGIpcg(tid.x + FfxBrixelizerGIpcg(tid.y + FfxBrixelizerGIpcg(GetFrameIndex()))) & 0xffu) / FfxFloat32(255.0);

    if (is_sky_pixel) 
    {
        StoreStaticGITarget(tid, FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0)));
        StoreSpecularTarget(tid, FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0)));
        return;
    }

    FfxFloat32x4 w      = FFX_BROADCAST_FLOAT32X4(0.0);
    FfxUInt32x4  probes = FFX_BROADCAST_UINT32X4(FFX_BRIXELIZER_GI_INVALID_ID);

    FfxInt32x2 offset = FfxInt32x2(tid.x < pinfo.seed_pixel.x ? -1 : 1, tid.y < pinfo.seed_pixel.y ? -1 : 1);
    probes.x    = FfxBrixelizerGIScreenProbes_FindClosestProbe(probe_coord, FfxInt32x2(0, 0));
    probes.y    = FfxBrixelizerGIScreenProbes_FindClosestProbe(probe_coord, FfxInt32x2(offset.x, 0));
    probes.z    = FfxBrixelizerGIScreenProbes_FindClosestProbe(probe_coord, FfxInt32x2(0, offset.y));
    probes.w    = FfxBrixelizerGIScreenProbes_FindClosestProbe(probe_coord, offset);

    if (probes.y == probes.x)
        probes.y = FFX_BRIXELIZER_GI_INVALID_ID;
    if (probes.z == probes.y || probes.z == probes.x)
        probes.z = FFX_BRIXELIZER_GI_INVALID_ID;
    if (probes.w == probes.z || probes.w == probes.y || probes.w == probes.x)
        probes.w = FFX_BRIXELIZER_GI_INVALID_ID;

    for (FfxUInt32 i = 0; i < 4; i++) 
    {
        if (probes[i] != FFX_BRIXELIZER_GI_INVALID_ID) 
        {
            FfxUInt32x2    pos                  = FfxUInt32x2(probes[i] & 0xffffu, (probes[i] >> 16u) & 0xffffu);
            FfxUInt32      probe_idx            = pos.x + pos.y * GetTileBufferDimensions().x;
            FfxBrixelizerGIProbeSpawnInfo pinfo                = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadTempProbeInfo(probe_idx));
            FfxFloat32x3   probe_spawn_position = FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(pinfo);
            FfxFloat32     dist                 = length(probe_spawn_position - world_pos);

            if (abs(dot(probe_spawn_position - world_pos, pixel_normal)) > eps_size)
                w[i] = FfxFloat32(0.0);
            else
                w[i] = ffxPow(ffxSaturate(dot(pixel_normal, pinfo.normal)) * ffxSaturate(FfxFloat32(1.0) - dist / eps_size), FfxFloat32(8.0));
        }
    }

    FfxRay base_ray = FfxBrixelizerGIGenReflectionRay(tid, tid);
    FFX_MIN16_F cosine_sh[9];
    FFX_MIN16_F reflection_sh[9];

    FfxFloat32 roughness = LoadRoughness(tid);
    FfxBrixelizerGISHGetCoefficients_ClampedCosine16(pixel_normal, cosine_sh);
    FfxBrixelizerGISHGetCoefficients_ClampedCosine16(base_ray.major_direction, reflection_sh);

    FfxFloat32x3 irradiance          = FFX_BROADCAST_FLOAT32X3(0.0);
    FfxFloat32x3 specular_irradiance = FFX_BROADCAST_FLOAT32X3(0.0);
    FfxFloat32   num_diffuse_samples = 0;

#if FFX_BRIXELIZER_GI_OPTION_DISABLE_DENOISER == 0
    FfxFloat32x4 reprojected     = LoadStaticGITargetSRV(tid);
    num_diffuse_samples          = ffxMin(FfxBrixelizerGIMAX_SAMPLES - FfxFloat32(1.0), reprojected.w);
    FfxFloat32   temporal_weight = FfxFloat32(1.0) / (FfxFloat32(1.0) + num_diffuse_samples); // sample count weight, purely temporal
#endif

    FfxFloat32 weight_sum = FfxFloat32(0.0);

    for (FfxUInt32 j = 0; j < 4; j++) 
    {
        if (probes[j] != FFX_BRIXELIZER_GI_INVALID_ID) 
        {
            FfxUInt32x2         pos            = FfxUInt32x2(probes[j] & 0xffffu, (probes[j] >> 16u) & 0xffffu);
            FfxUInt32           probe_idx      = pos.x + pos.y * GetTileBufferDimensions().x;
            FfxBrixelizerGIProbeSpawnInfo      pinfo          = FfxBrixelizerGIProbeSpawnInfoUnpack(LoadTempProbeInfo(probe_idx));
            FfxFloat32x3        cur_irradiance = FFX_BROADCAST_FLOAT32X3(0.0);

            FfxFloat32x4 probe_sh[9];
            for (FfxUInt32 i = 0; i < 9; i++)
                probe_sh[i] = ffxUnpackF32x2(LoadStaticProbeSHBuffer(9 * probe_idx + i)).xyzw;
            
            for (FfxUInt32 i = 0; i < 9; i++) 
                cur_irradiance += FfxFloat32(cosine_sh[i]) * probe_sh[i].xyz;

            FfxFloat32   weight               = FfxFloat32(1.0); 
            FfxFloat32x3 probe_spawn_position = FfxBrixelizerGIProbeSpawnInfoGetSpawnPosition(pinfo);

            irradiance += w[j] * cur_irradiance * weight;
            weight_sum += w[j] * weight;
        }
    }

    const FfxFloat32 EPS = FfxFloat32(1.0e-2);
    irradiance /= (ffxMax(weight_sum, EPS) * FFX_BRIXELIZER_GI_PI * FfxFloat32(2.0));
    FfxFloat32 size_weight           = ffxSaturate(eps_size / (GetContextInfo().cascades[0].voxel_size * FfxFloat32(64.0)));
    FfxFloat32 total_sh_cache_weight = ffxSaturate(ffxLerp(FfxFloat32(1.0), FfxFloat32(0.0), size_weight) - ffxSaturate(FfxFloat32(1.0) - dot(pixel_normal, -view)));

    FFX_MIN16_F4 probe_sh[9];
    StoreDebugTarget(tid, FfxFloat32x4(0.0, 0.0, 0.0, 1.0));
    
    FfxBoolean has_world_probe = FfxBrixelizerGIInterpolateBrickSH(world_pos, g_starting_cascade, g_end_cascade, xi, /* inout */ probe_sh);
    
    if (has_world_probe) 
    {
        // if (probe_sh[0].w > FfxFloat32(4.0))
        {
            if (weight_sum < FfxFloat32(1.0e-3))
                total_sh_cache_weight = FfxFloat32(0.0);

            FfxFloat32x3 cur_irradiance = FFX_BROADCAST_FLOAT32X3(0.0);
            FfxFloat32x3 cur_specular_irradiance = FFX_BROADCAST_FLOAT32X3(0.0);

            for (FfxUInt32 i = 0; i < 9; i++) 
            {
                cur_irradiance += cosine_sh[i] * probe_sh[i].xyz;
                cur_specular_irradiance += reflection_sh[i] * probe_sh[i].xyz;
            }

            irradiance = cur_irradiance / (FFX_BRIXELIZER_GI_PI * FfxFloat32(2.0));
            specular_irradiance = cur_specular_irradiance / (FFX_BRIXELIZER_GI_PI * FfxFloat32(2.0));
        }
    }
    if (!has_world_probe && weight_sum < FfxFloat32(1.0e-3)) 
    {
        StoreStaticGITarget(tid, FFX_BROADCAST_FLOAT32X4(0.0));
        StoreDebugTarget(tid, FfxFloat32x4(1.0, 0.0, 0.0, 1.0));
    } 
    else 
    {
        irradiance = ffxMax(irradiance, FFX_BROADCAST_FLOAT32X3(0.0));

#if FFX_BRIXELIZER_GI_OPTION_DISABLE_DENOISER == 0
        reprojected.xyz = FfxBrixelizerGIClipAABB(reprojected.xyz, irradiance.xyz, FfxFloat32(0.2));

        irradiance = ffxLerp(reprojected.xyz, irradiance, temporal_weight);
#endif

        FfxFloat32x4 result = FfxFloat32x4(irradiance, num_diffuse_samples + FfxFloat32(1.0));

        if (any(isnan(result)))
            result = FFX_BROADCAST_FLOAT32X4(0.0);

        StoreStaticGITarget(tid, result);
    }

#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
    // Specular part
    {
        FfxFloat32   num_specular_samples = 0;
        FfxFloat32x4 specular_output      = LoadSpecularTarget(tid);

#if FFX_BRIXELIZER_GI_OPTION_DISABLE_DENOISER == 0
        FfxFloat32x4 specular_history = LoadSpecularTargetSRV(tid.xy);
        num_specular_samples          = specular_history.w;
#endif

        FfxFloat32   roughness   = LoadRoughness(tid);
        FfxFloat32   max_samples = ffxLerp(FfxFloat32(8.0), FfxFloat32(64.0), ffxSqrt(roughness));
        num_specular_samples     = ffxMin(num_specular_samples, max_samples);
        FfxFloat32 weight        = FfxFloat32(1.0) / (FfxFloat32(1.0) + num_specular_samples);

        if (specular_output.w < FfxFloat32(1.0)) 
        {    
            if (any(isnan(specular_irradiance)))
                specular_irradiance = FFX_BROADCAST_FLOAT32X3(0.0);

             specular_output.xyz = ffxLerp(specular_irradiance.xyz, specular_output.xyz, specular_output.w);
             specular_output.xyz = FfxBrixelizerGIClipAABB(specular_output.xyz, specular_irradiance.xyz, FfxFloat32(0.1));
             specular_output.w   = FfxFloat32(1.0);
        }

#if FFX_BRIXELIZER_GI_OPTION_DISABLE_DENOISER == 0
        if (specular_output.w > FfxFloat32(1.0e-3))
            specular_history.xyz = FfxBrixelizerGIClipAABB(specular_history.xyz, specular_output.xyz, FfxFloat32(0.5f));
        
        specular_output.xyz = ffxLerp(specular_history.xyz, specular_output.xyz, ffxSaturate(weight * specular_output.w));
#endif

        StoreSpecularTarget(tid.xy, FfxFloat32x4(specular_output.xyz, num_specular_samples + FfxFloat32(1.0)));
    }
#endif
}

void FfxBrixelizerGISpecularSpatialFilter(FfxUInt32x2 tid, FfxUInt32x2 gid)
{
    FfxBrixelizerGISpecularInitializeGroupSharedMemory(FfxInt32x2(tid), FfxInt32x2(gid), FfxInt32x2(GetBufferDimensions()));
    FFX_GROUP_MEMORY_BARRIER;

    gid += 4; // Center threads in FFX_GROUPSHARED memory

    const FfxUInt32 sample_count = 0;
    // +---+---+---+---+---+---+---+
    // | X |   |   |   |   |   |   |
    // +---+---+---+---+---+---+---+
    // |   |   |   | X | X |   |   |
    // +---+---+---+---+---+---+---+
    // |   | X |   | X |   | X |   |
    // +---+---+---+---+---+---+---+
    // | X |   |   | X | X |   | X |
    // +---+---+---+---+---+---+---+
    // |   |   | X |   |   |   | X |
    // +---+---+---+---+---+---+---+
    // |   | X | X |   |   |   |   |
    // +---+---+---+---+---+---+---+
    // |   |   |   | X |   | X |   |
    // +---+---+---+---+---+---+---+
    const FfxInt32x2 sample_offsets[] = { //
        FfxInt32x2(0, 1),                 //
        FfxInt32x2(-2, 1),                //
        FfxInt32x2(2, -3),                //
        FfxInt32x2(-3, 0),                //
        FfxInt32x2(1, 2),                 //
        FfxInt32x2(-1, -2),               //
        FfxInt32x2(3, 0),                 //
        FfxInt32x2(-3, 3),                //
        FfxInt32x2(0, -3),                //
        FfxInt32x2(-1, -1),               //
        FfxInt32x2(2, 1),                 //
        FfxInt32x2(-2, -2),               //
        FfxInt32x2(1, 0),                 //
        FfxInt32x2(0, 2),                 //
        FfxInt32x2(3, -1)
    };

    FfxFloat32x2                         uv         = FfxBrixelizerGIGetUV(tid);
    FfxBrixelizerGISpecularNeighborhoodSample center     = FfxBrixelizerGISpecularLoadFromGroupSharedMemory(FfxInt32x2(gid));
    FFX_MIN16_F                           eps_size   = FFX_MIN16_F(4.0) * FFX_MIN16_F(length(SpecularNeighborhoodSampleGetWorldPos(center, uv) - GetCameraPosition()));
    FFX_MIN16_F4                         signal_sum = FFX_BROADCAST_MIN_FLOAT16X4(FFX_MIN16_F(0.0));
    FFX_MIN16_F                           weight_sum = FFX_MIN16_F(0.0);

    if (ffxAsUInt32(FfxFloat32(center.radiance.w)) != 0) 
    {
        signal_sum = center.radiance;
        weight_sum = FFX_MIN16_F(1.0);
    }

    for (FfxUInt32 i = 0; i < sample_count; i++) 
    {
        FfxInt32x2 coord = FfxInt32x2(gid) + sample_offsets[i];

        if ((GetFrameIndex() & FfxUInt32(1)) == FfxUInt32(1))
            coord = FfxInt32x2(gid) + sample_offsets[i].yx;

        FfxBrixelizerGISpecularNeighborhoodSample specSample = FfxBrixelizerGISpecularLoadFromGroupSharedMemory(coord);

        if (ffxIsBackground(specSample.depth) || ffxAsUInt32(specSample.depth) == 0)
            continue;

        if (ffxAsUInt32(FfxFloat32(specSample.radiance.w)) == 0)
            continue;
        
        FFX_MIN16_F weight = FfxBrixelizerGIWeightMin16(
            specSample.normal,          //
            SpecularNeighborhoodSampleGetWorldPos(specSample, uv), //
            center.normal,          //
            SpecularNeighborhoodSampleGetWorldPos(center, uv), //
            eps_size,               //
            FFX_MIN16_F(8.0));

        weight *= FFX_MIN16_F(FfxBrixelizerGIGetLuminanceWeight(FfxFloat32x3(specSample.radiance.xyz), FfxFloat32(2.0))); //
        {
            signal_sum += specSample.radiance * weight;
            weight_sum += weight;
        }
    }

    StoreSpecularTarget(tid, signal_sum / ffxMax(FFX_MIN16_F(1.0e-3), weight_sum));
}

void FfxBrixelizerGIBlurGI(FfxUInt32x2 tid, FfxUInt32x2 gid)
{
    FfxFloat32 num_samples = ffxMin(LoadStaticGITargetSRV(tid).w, LoadSpecularTargetSRV(tid).w);

    FfxInt32 radius = FfxInt32(floor(FfxFloat32(8.0) * ffxSaturate(FfxFloat32(1.0) - ffxPow(num_samples / FfxBrixelizerGIMAX_SAMPLES, FfxFloat32(4.0)))));

    if (radius == 0) 
    {
        FfxFloat32x4 diffuse  = LoadStaticGITargetSRV(tid);
        FfxFloat32x4 specular = LoadSpecularTargetSRV(tid);

        StoreStaticGITarget(tid, diffuse);
        StoreSpecularTarget(tid, specular);
        return;
    }

#if defined(FfxBrixelizerGIBlurGI_PASS_0)
    FfxInt32x2 dir = FfxInt32x2(1, 0);
#else  // !#if defined(FfxBrixelizerGIBlurGI_PASS_0)
    FfxInt32x2 dir = FfxInt32x2(0, 1);
#endif // !#if defined(FfxBrixelizerGIBlurGI_PASS_0)

    FfxFloat32x4 acc                 = FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0));
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
    FfxFloat32x4 specular_acc        = FFX_BROADCAST_FLOAT32X4(FfxFloat32(0.0));
#endif
    FfxFloat32   weight_acc          = FfxFloat32(0.0);
    FfxFloat32   specular_weight_acc = FfxFloat32(0.0);
    FfxFloat32x3 pixel_normal        = LoadWorldNormal(tid);
    FfxFloat32   pixel_depth         = LoadDepth(tid);

    if (ffxIsBackground(pixel_depth)) 
    {
        StoreStaticGITarget(tid, FFX_BROADCAST_FLOAT32X4(0.0));
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
        StoreSpecularTarget(tid, FFX_BROADCAST_FLOAT32X4(0.0));
#endif
        return;
    }

    FfxFloat32x2 pixel_uv        = FfxBrixelizerGIGetUV(tid);
    FfxFloat32x3 pixel_world_pos = ffxGetWorldPosition(pixel_uv, pixel_depth);
    FfxFloat32   eps_size        = FfxFloat32(1.0) * length(pixel_world_pos - GetCameraPosition());

    for (FfxInt32 c = -radius; c <= radius; c++) 
    {
        FfxInt32x2 coord = FfxInt32x2(tid) + dir * c;

        if (any(FFX_LESS_THAN(coord, FFX_BROADCAST_INT32X2(0))) || any(FFX_GREATER_THAN_EQUAL(coord, GetBufferDimensions())))
            continue;

        FfxFloat32x3 sample_normal = LoadWorldNormal(coord);
        FfxFloat32   sample_depth  = LoadDepth(coord);

        if (ffxIsBackground(sample_depth))
            continue;

        FfxFloat32x2 sample_uv        = FfxBrixelizerGIGetUV(coord);
        FfxFloat32x3 sample_world_pos = ffxGetWorldPosition(sample_uv, sample_depth);
        FfxFloat32   weight           = FfxBrixelizerGIWeight(
            pixel_normal,     //
            pixel_world_pos,  //
            sample_normal,    //
            sample_world_pos, //
            eps_size,         //
            FfxFloat32(16.0), //
            FfxFloat32(16.0)  //
        );                    //

        FfxFloat32x4 diffuse_gi_sample  = LoadStaticGITargetSRV(coord);
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
        FfxFloat32x4 specular_gi_sample = LoadSpecularTargetSRV(coord);
#endif

        if (any(FFX_GREATER_THAN(diffuse_gi_sample, FFX_BROADCAST_FLOAT32X4(1.0e-6)))) 
        {
            FfxFloat32 diff_weight = weight * FfxBrixelizerGIGetLuminanceWeight(diffuse_gi_sample.xyz, FfxFloat32(2.5));
            weight_acc += diff_weight;
            acc += diffuse_gi_sample * diff_weight;
        }

#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
        if (specular_gi_sample.w > FfxFloat32(1.0e-6)) 
        {
            FfxFloat32 spec_weight = weight * FfxFloat32(0.1) / FfxFloat32(1 + c * c) * FfxBrixelizerGIGetLuminanceWeight(specular_gi_sample.xyz, FfxFloat32(2.5));
            specular_acc += specular_gi_sample * spec_weight;
            specular_weight_acc += spec_weight;
        }
#endif        
    }
    acc /= ffxMax(FfxFloat32(1.0e-6), weight_acc);

#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
    specular_acc /= ffxMax(FfxFloat32(1.0e-6), specular_weight_acc);
#endif

    if (any(isnan(acc))) 
    {
        acc          = FFX_BROADCAST_FLOAT32X4(0.0);
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
        specular_acc = FFX_BROADCAST_FLOAT32X4(0.0);
#endif
    }

    StoreStaticGITarget(tid, acc);
#if FFX_BRIXELIZER_GI_OPTION_DISABLE_SPECULAR == 0
    StoreSpecularTarget(tid, specular_acc);
#endif
}

void FfxBrixelizerGIDebugVisualization(FfxUInt32x2 tid)
{
    FfxFloat32x2 uv = (FfxFloat32x2(tid.xy) + FFX_BROADCAST_FLOAT32X2(0.5).xx) / FfxFloat32x2(GetGIConstants().target_width, GetGIConstants().target_height);
    FfxFloat32   z  = LoadDepth(tid.xy);

    if (ffxIsBackground(z))
    {
        StoreDebugVisualization(tid, FfxFloat32x4(0.0f, 0.0f, 0.0f, 0.0f));
        return;
    }
    
    FfxUInt32    g_starting_cascade = GetGIConstants().tracing_constants.start_cascade;
    FfxUInt32    g_end_cascade      = GetGIConstants().tracing_constants.end_cascade;
    FfxFloat32x3 world_pos          = ffxGetWorldPosition(uv, z);
    FfxFloat32x3 world_normal       = LoadWorldNormal(tid);

    if (GetGIConstants().debug_type == 0)
    {
        FfxFloat32x3 radiance = FFX_BROADCAST_FLOAT32X3(0.0f);

        FfxBrixelizerGISampleRadianceCache(world_pos, world_normal, FFX_BROADCAST_FLOAT32X3(0.0f), g_starting_cascade, g_end_cascade, radiance);

        StoreDebugVisualization(tid, FfxFloat32x4(radiance, 1.0f));
    }
    else
    {
        FfxFloat32 xi = FfxFloat32(FfxBrixelizerGIpcg(tid.x + FfxBrixelizerGIpcg(tid.y + FfxBrixelizerGIpcg(GetFrameIndex()))) & 0xffu) / FfxFloat32(255.0);

        FFX_MIN16_F cosine_sh[9];
        FfxBrixelizerGISHGetCoefficients_ClampedCosine16(world_normal, cosine_sh);

        FFX_MIN16_F4 probe_sh[9];
        FfxBoolean has_world_probe = FfxBrixelizerGIInterpolateBrickSH(world_pos, g_starting_cascade, g_end_cascade, xi, /* inout */ probe_sh);
        
        FfxFloat32x3 irradiance = FFX_BROADCAST_FLOAT32X3(0.0f);
        
        if (has_world_probe) 
        {
            for (FfxUInt32 i = 0; i < 9; i++)
                irradiance += cosine_sh[i] * probe_sh[i].xyz;
        }

        StoreDebugVisualization(tid, FfxFloat32x4(irradiance, 1.0f));
    }
}

#define FFX_DISOCCLUSION_THRESHOLD 0.9
FFX_MIN16_F FfxBrixelizerGIGetDisocclusionFactor(FFX_MIN16_F3 normal, FfxFloat32 linear_depth, FfxFloat32x3 world_position, FFX_MIN16_F3 history_normal, FfxFloat32x3 history_world_position) 
{
    FFX_MIN16_F factor = FFX_MIN16_F(FfxFloat32(1.0)                                                                   //
                  * exp(-abs(FfxFloat32(1.0) - ffxMax(0.0, dot(normal, history_normal))) * FfxFloat32(1.4)) //
                  * exp(-length(world_position - history_world_position) / linear_depth * FfxFloat32(1.0)));
    return factor;
}

void FfxBrixelizerGIGenerateDisocclusionMask(FfxUInt32x2 tid)
{
    FfxUInt32x2 screen_size = FfxUInt32x2(GetGIConstants().target_width, GetGIConstants().target_height);

    if (all(FFX_LESS_THAN(tid, screen_size)))
    {
        FfxFloat32x2 uv                   = (FfxFloat32x2(tid.xy) + FfxFloat32(0.5).xx) / FfxFloat32x2(screen_size.xy);
        FFX_MIN16_F3 normal               = FFX_MIN16_F3(LoadWorldNormal(tid));
        FfxFloat32   depth                = LoadDepth(tid.xy);
        FfxFloat32   linear_depth         = ffxGetLinearDepth(uv, depth);
        FfxFloat32x2 history_uv           = uv + LoadMotionVector(tid.xy);
        FFX_MIN16_F3 history_normal       = FFX_MIN16_F3(SamplePrevWorldNormal(history_uv));
        FfxFloat32   depth_history        = SamplePrevDepth(history_uv);
        FfxFloat32   history_linear_depth = ffxGetLinearDepth(history_uv, depth_history);

        FfxFloat32x3 world_position = ffxGetWorldPosition(uv, depth);
        FfxFloat32x3 prev_world_position = ffxGetWorldPosition(history_uv, depth_history);

        if (any(FFX_LESS_THAN(history_uv, FFX_BROADCAST_FLOAT32X2(0.0))) || any(FFX_GREATER_THAN(history_uv, FFX_BROADCAST_FLOAT32X2(1.0))) || FfxBrixelizerGIGetDisocclusionFactor(normal, linear_depth, world_position, history_normal, prev_world_position) < FFX_DISOCCLUSION_THRESHOLD)
            StoreDisocclusionMask(tid.xy, FfxFloat32(1.0));
        else
            StoreDisocclusionMask(tid.xy, FfxFloat32(0.0));
    }
}

void FfxBrixelizerGIDownsample(FfxUInt32x2 tid)
{
    FfxUInt32x2 screen_size = GetScalingConstants().downsampledSize;

    if (all(FFX_LESS_THAN(tid, screen_size)))
    {
        FfxFloat32x2 uv = (FfxFloat32x2(tid.xy) + float(0.5).xx) / FfxFloat32x2(screen_size.xy);

        const FfxFloat32x4 depthGather = GatherSourceDepth(uv);
        const FfxFloat32x4 depthGatherPrev = GatherSourcePrevDepth(uv);

        StoreDownsampledDepth(tid, FfxBrixelizerGIDepthCloserOp(FfxBrixelizerGIDepthCloserOp(depthGather.x, depthGather.y), FfxBrixelizerGIDepthCloserOp(depthGather.z, depthGather.w)));
        StoreDownsampledPrevDepth(tid, FfxBrixelizerGIDepthCloserOp(FfxBrixelizerGIDepthCloserOp(depthGatherPrev.x, depthGatherPrev.y), FfxBrixelizerGIDepthCloserOp(depthGatherPrev.z, depthGatherPrev.w)));
        StoreDownsampledNormal(tid, SampleSourceNormal(uv));
        StoreDownsampledPrevNormal(tid, SampleSourcePrevNormal(uv));
        StoreDownsampledRoughness(tid, SampleSourceRoughness(uv));
        StoreDownsampledMotionVector(tid, SampleSourceMotionVector(uv));
        StoreDownsampledPrevLitOutput(tid, SampleSourcePrevLitOutput(uv));
    }
}

float FfxBrixelizerGINormalEdgeStoppingWeight(FfxFloat32x3 center_normal, FfxFloat32x3 sample_normal, float power)
{
    return pow(clamp(dot(center_normal, sample_normal), 0.0f, 1.0f), power);
}

float FfxBrixelizerGIDepthEdgeStoppingWeight(float center_depth, float sample_depth, float phi)
{
    return exp(-abs(center_depth - sample_depth) / phi);
}

float FfxBrixelizerGIComputeEdgeStoppingWeight(
                      float center_depth,
                      float sample_depth,
                      float phi_z, 
                      FfxFloat32x3  center_normal,
                      FfxFloat32x3  sample_normal,
                      float phi_normal)
{
    const float wZ      = FfxBrixelizerGIDepthEdgeStoppingWeight(center_depth, sample_depth, phi_z);   
    const float wNormal = FfxBrixelizerGINormalEdgeStoppingWeight(center_normal, sample_normal, phi_normal);
    const float wL = 1.0f;

    const float w = exp(0.0 - max(wL, 0.0) - max(wZ, 0.0)) * wNormal;

    return w;
}

void FfxBrixelizerGIUpsample(FfxUInt32x2 tid)
{
    FfxUInt32x2  low_res_screen_size = FfxUInt32x2(GetScalingConstants().downsampledSize);
    FfxFloat32x2 low_res_texel_size = 1.0f / FfxFloat32x2(low_res_screen_size);
    
    FfxUInt32x2  hi_res_screen_size = FfxUInt32x2(GetScalingConstants().sourceSize);
    FfxFloat32x2 uv                 = (FfxFloat32x2(tid.xy) + float(0.5).xx) / FfxFloat32x2(hi_res_screen_size.xy);

    if (all(FFX_LESS_THAN(tid, hi_res_screen_size)))
    {
        FfxFloat32 hi_res_depth = LoadSourceDepth(tid);

        if (ffxIsBackground(hi_res_depth))
        {
            StoreUpsampledDiffuseGI(tid, FfxFloat32x3(0.0f, 0.0f, 0.0f));
            StoreUpsampledSpecularGI(tid, FfxFloat32x3(0.0f, 0.0f, 0.0f));
            return;
        }

        FfxFloat32x3 hi_res_normal = LoadSourceNormal(tid);

        FfxFloat32x3 upsampled_diffuse  = FFX_BROADCAST_FLOAT32X3(0.0f);
        FfxFloat32x3 upsampled_specular = FFX_BROADCAST_FLOAT32X3(0.0f);
        FfxFloat32   total_w            = 0.0f;

        const FfxFloat32 FLT_EPS = 0.00000001;

        const FfxFloat32x2 g_kernel[4] =  {
            FfxFloat32x2(0.0f, 1.0f),
            FfxFloat32x2(1.0f, 0.0f),
            FfxFloat32x2(-1.0f, 0.0f),
            FfxFloat32x2(0.0, -1.0f)
        };

        for (FfxUInt32 i = 0; i < 4; i++)
        {
            FfxFloat32x2 coarse_tex_coord = uv + g_kernel[i] * low_res_texel_size;
            FfxFloat32   coarse_depth     = SampleDepth(coarse_tex_coord);

            // If depth belongs to skybox, skip
            if (ffxIsBackground(coarse_depth))
                continue;

            FfxFloat32x3 coarse_normal = SampleWorldNormal(coarse_tex_coord);

            FfxFloat32 w = FfxBrixelizerGIComputeEdgeStoppingWeight(hi_res_depth,
                                                        coarse_depth,
                                                        1.0f,
                                                        hi_res_normal,
                                                        coarse_normal,
                                                        32.0f);

            upsampled_diffuse += SampleDownsampledDiffuseGI(coarse_tex_coord) * w;
            upsampled_specular += SampleDownsampledSpecularGI(coarse_tex_coord) * w;
            total_w += w;
        }

        upsampled_diffuse = upsampled_diffuse / max(total_w, FLT_EPS);
        upsampled_specular = upsampled_specular / max(total_w, FLT_EPS);

        // Store
        StoreUpsampledDiffuseGI(tid, upsampled_diffuse);
        StoreUpsampledSpecularGI(tid, upsampled_specular);
    }
}

#endif // FFX_BRIXELIZER_GI_MAIN_H