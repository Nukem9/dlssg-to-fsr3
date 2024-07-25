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

#ifndef FFX_BRIXELIZER_GI_RADIANCE_CACHE_H
#define FFX_BRIXELIZER_GI_RADIANCE_CACHE_H

#include "../ffx_core.h"

#include "ffx_brixelizer_brick_common_private.h"
#include "ffx_brixelizergi_common.h"
#include "ffx_brixelizergi_probe_shading.h"

FfxFloat32x3 GetWorldPosition(FfxUInt32x2 pixel_coordinate)
{
    FfxFloat32x2 uv                         = (FfxFloat32x2(pixel_coordinate.xy) + (0.5).xx) / FfxFloat32x2(GetGIConstants().target_width, GetGIConstants().target_height);
    FfxFloat32   z                          = LoadDepth(pixel_coordinate);
    FfxFloat32x3 screen_uv_space_ray_origin = FfxFloat32x3(uv, z);
    FfxFloat32x3 view_space_position        = ffxScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 world_space_origin         = ffxViewSpaceToWorldSpace(FfxFloat32x4(view_space_position, FfxFloat32(1.0))).xyz;
    return world_space_origin;
}

// https://www.pcg-random.org/
FfxUInt32 FFX_pcg(FfxUInt32 v)
{
    FfxUInt32 state = v * 747796405u + 2891336453u;
    FfxUInt32 word  = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

void FfxBrixelizerGIEmitPrimaryRayRadiance(FfxUInt32x2 _tid)
{
    FfxUInt32x2 qid = FfxUInt32x2(GetGIConstants().frame_index & 3, (GetGIConstants().frame_index >> 2) & 3);
    FfxUInt32x2 tid = _tid * 4 + qid;

    if (any(FFX_GREATER_THAN_EQUAL(tid, FfxUInt32x2(GetGIConstants().target_width, GetGIConstants().target_height))))
        return;

    FfxFloat32x2 uv = (FfxFloat32x2(tid.xy) + FFX_BROADCAST_FLOAT32X2(0.5).xx) / FfxFloat32x2(GetGIConstants().target_width, GetGIConstants().target_height);
    FfxFloat32  z  = LoadDepth(tid.xy);

    if (ffxIsBackground(z))
        return;

    FfxFloat32x2 prev_uv = uv + SampleMotionVector(uv);

    if (any(FFX_GREATER_THAN(prev_uv, FFX_BROADCAST_FLOAT32X2(1.0))) || any(FFX_LESS_THAN(prev_uv, FFX_BROADCAST_FLOAT32X2(0.0))))
        return;

    FfxFloat32x3 ray_origin       = GetWorldPosition(tid.xy);
    FfxFloat32x3 primary_radiance = SamplePrevLitOutput(prev_uv);
    FfxFloat32   xi                         = FfxFloat32(FFX_pcg(tid.x + FFX_pcg(tid.y + FFX_pcg(GetGIConstants().frame_index))) & 0xffffu) / FfxFloat32(256 * 256 - 1);
    FfxFloat32x3 screen_uv_space_ray_origin = FfxFloat32x3(uv, FfxFloat32(0.5));
    FfxFloat32x3 view_space_ray             = ffxScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    FfxFloat32x3 view_space_ray_direction   = normalize(view_space_ray);
    FfxFloat32x3 ray_direction              = -normalize(ffxViewSpaceToWorldSpace(FfxFloat32x4(view_space_ray_direction, FfxFloat32(0.0))));
    FfxBoolean   disoccluded = LoadDisocclusionMask(tid.xy) > 0;

    if (disoccluded)
        return;

    FfxUInt32   g_starting_cascade = GetGIConstants().tracing_constants.start_cascade;
    FfxUInt32   g_end_cascade      = GetGIConstants().tracing_constants.end_cascade;
    FfxFloat32x3 world_normal      = LoadWorldNormal(tid);
    FfxBrixelizerGIEmitRadiance(ray_origin, world_normal, ray_direction, primary_radiance, xi, g_starting_cascade, g_end_cascade);
}

void FfxBrixelizerGIPrepareClearCache(FfxUInt32x3 tid)
{
    FfxUInt32 cnt = LoadContextCounter(FFX_BRIXELIZER_CONTEXT_COUNTER_CLEAR_BRICKS);
    StoreRaySwapIndirectArgs(0, cnt);
    StoreRaySwapIndirectArgs(1, 1);
    StoreRaySwapIndirectArgs(2, 1);
}

void FfxBrixelizerGIClearCache(FfxUInt32x3 tid)
{
    FfxUInt32   brick_offset = tid.x / 64;
    FfxUInt32   brick_id     = LoadBricksClearList(brick_offset);
    FfxUInt32x3 local_coord  = FfxBrixelizerUnflattenPOT(tid.x % 64, 2);

    if ((tid.x % 64) == 0) { // clear SH
        FfxFloat32x4 shs[9];
        for (FfxInt32 j = 0; j < 9; j++)
            shs[j] = FFX_BROADCAST_FLOAT32X4(0.0);
        FfxBrixelizerGIStoreBrickSH(brick_id, shs);
        FfxBrixelizerGIStoreBrickDirectSH(brick_id, shs);
        StoreBricksSHState(FfxBrixelizerBrickGetIndex(brick_id), FFX_BROADCAST_UINT32X4(0));
    }
    StoreRadianceCache(FfxBrixelizerGetSDFAtlasOffset(brick_id) / 2 + local_coord, FfxFloat32x3(0.0, 0.0, 0.0));
}

void FfxBrixelizerGIPropagateSH(FfxUInt32x3 tid)
{
    if (tid.x >= FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION * FFX_BRIXELIZER_CASCADE_RESOLUTION)
        return;

    FfxInt32x3               voxel         = FfxInt32x3(FfxBrixelizerUnflattenPOT(tid.x, FFX_BRIXELIZER_CASCADE_DEGREE));
    FfxBrixelizerCascadeInfo CINFO         = GetCascadeInfo(GetPassConstantsCascadeIndex());
    FfxUInt32                base_brick_id = LoadCascadeBrickMapArrayUniform(GetPassConstantsCascadeIndex(), WrapFlatCoords(CINFO, tid.x));
    if (!FfxBrixelizerIsValidID(base_brick_id))
        return;
    FfxFloat32x4 base_sh[9];
    FfxBrixelizerGILoadBrickSH(base_brick_id, /* inout */ base_sh);
    FfxFloat32 weight_acc = FfxFloat32(base_sh[0].w * base_sh[0].w);
    for (FfxInt32 j = 0; j < 9; j++)
        base_sh[j] *= weight_acc;
    FfxInt32 next_cascade_idx = -1;

    FfxUInt32x4  sh_state = LoadBricksSHState(FfxBrixelizerBrickGetIndex(base_brick_id));
    FFX_MIN16_F4 dir_w    = FFX_MIN16_F4(ffxUnpackF32x2(sh_state.xy));

    if (next_cascade_idx == -1) 
    {
        for (FfxInt32 z = -1; z <= 1; z++) 
        {
            for (FfxInt32 y = -1; y <= 1; y++) 
            {
                for (FfxInt32 x = -1; x <= 1; x++) 
                {
                    if (x == 0 && y == 0 && z == 0)
                        continue;

                    FfxInt32x3 sample_voxel = voxel + FfxInt32x3(x, y, z);

                    if (any(FFX_LESS_THAN(sample_voxel, FFX_BROADCAST_INT32X3(0))) || any(FFX_GREATER_THAN_EQUAL(sample_voxel, FFX_BROADCAST_INT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION))))
                        continue;

                    FfxUInt32 sample_brick_id = LoadCascadeBrickMapArrayUniform(GetPassConstantsCascadeIndex(), FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(CINFO, sample_voxel), FFX_BRIXELIZER_CASCADE_DEGREE));
                    
                    if (FfxBrixelizerIsInvalidID(sample_brick_id))
                        continue;

                    FfxUInt32x4  sample_sh_state = LoadBricksSHState(FfxBrixelizerBrickGetIndex(sample_brick_id));
                    FFX_MIN16_F4 sample_dir_w    = FFX_MIN16_F4(ffxUnpackF32x2(sample_sh_state.xy));

                    if (dot(sample_dir_w.xyz, dir_w.xyz) < FFX_MIN16_F(0.0))
                        continue;

                    FfxFloat32x4 shs[9];
                    FfxBrixelizerGILoadBrickSH(sample_brick_id, /* inout */ shs);
                    FfxFloat32 weight = FfxFloat32(1.0) / FfxFloat32(x * x + y * y + z * z);
                    for (FfxInt32 j = 0; j < 9; j++)
                        base_sh[j] += shs[j] * weight;
                    weight_acc += weight;
                }
            }
        }
    }
    for (FfxInt32 j = 0; j < 9; j++)
        base_sh[j] *= FfxFloat32(1.0) / ffxMax(weight_acc, FfxFloat32(1.0e-6));
    for (FfxInt32 j = 0; j < 9; j++)
        base_sh[j].w *= GetPassConstantsEnergyDecayK();
    FfxBrixelizerGIStoreBrickSH(base_brick_id, base_sh);
}

#endif // FFX_BRIXELIZER_GI_RADIANCE_CACHE_H