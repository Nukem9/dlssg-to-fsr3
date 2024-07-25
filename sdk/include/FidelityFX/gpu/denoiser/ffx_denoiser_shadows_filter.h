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

#ifndef FFX_DNSR_SHADOWS_FILTER_HLSL
#define FFX_DNSR_SHADOWS_FILTER_HLSL

#include "ffx_denoiser_shadows_util.h"

FFX_GROUPSHARED FfxUInt32 g_FFX_DNSR_Shadows_shared_input[16][16];
FFX_GROUPSHARED FfxFloat32 g_FFX_DNSR_Shadows_shared_depth[16][16];
FFX_GROUPSHARED FfxUInt32 g_FFX_DNSR_Shadows_shared_normals_xy[16][16];
FFX_GROUPSHARED FfxUInt32 g_FFX_DNSR_Shadows_shared_normals_zw[16][16];

#if FFX_HALF
FfxUInt32 FFX_DNSR_Shadows_PackFloat16(FfxFloat16x2 v)
{
    return ffxPackHalf2x16(FfxFloat32x2(v));
}

FfxFloat16x2 FFX_DNSR_Shadows_UnpackFloat16(FfxUInt32 a)
{
    return ffxUnpackF16(a);
}

FfxFloat16x2 FFX_DNSR_Shadows_LoadInputFromGroupSharedMemory(FfxInt32x2 idx)
{
    return FFX_DNSR_Shadows_UnpackFloat16(g_FFX_DNSR_Shadows_shared_input[idx.y][idx.x]);
}
#endif

FfxFloat32 FFX_DNSR_Shadows_LoadDepthFromGroupSharedMemory(FfxInt32x2 idx)
{
    return g_FFX_DNSR_Shadows_shared_depth[idx.y][idx.x];
}

#if FFX_HALF
FfxFloat16x3 FFX_DNSR_Shadows_LoadNormalsFromGroupSharedMemory(FfxInt32x2 idx)
{
    FfxFloat16x3 normals;
    normals.xy = FFX_DNSR_Shadows_UnpackFloat16(g_FFX_DNSR_Shadows_shared_normals_xy[idx.y][idx.x]);
    normals.z = FFX_DNSR_Shadows_UnpackFloat16(g_FFX_DNSR_Shadows_shared_normals_zw[idx.y][idx.x]).x;
    return normals;
}

void FFX_DNSR_Shadows_StoreInGroupSharedMemory(FfxInt32x2 idx, FfxFloat16x3 normals, FfxFloat16x2 inp, FfxFloat32 depth)
{
    g_FFX_DNSR_Shadows_shared_input[idx.y][idx.x] = FFX_DNSR_Shadows_PackFloat16(inp);
    g_FFX_DNSR_Shadows_shared_depth[idx.y][idx.x] = depth;
    g_FFX_DNSR_Shadows_shared_normals_xy[idx.y][idx.x] = FFX_DNSR_Shadows_PackFloat16(normals.xy);
    g_FFX_DNSR_Shadows_shared_normals_zw[idx.y][idx.x] = FFX_DNSR_Shadows_PackFloat16(FfxFloat16x2(normals.z, 0));
}

void FFX_DNSR_Shadows_LoadWithOffset(FfxInt32x2 did, FfxInt32x2 offset, out FfxFloat16x3 normals, out FfxFloat16x2 inp, out FfxFloat32 depth)
{
    did += offset;

    const FfxInt32x2 p = clamp(did, FfxInt32x2(0, 0), BufferDimensions() - 1);
    normals = FfxFloat16x3(LoadNormals(p));
    inp = LoadFilterInput(p);
    depth = LoadDepth(p);
}

void FFX_DNSR_Shadows_StoreWithOffset(FfxInt32x2 gtid, FfxInt32x2 offset, FfxFloat16x3 normals, FfxFloat16x2 inp, FfxFloat32 depth)
{
    gtid += offset;
    FFX_DNSR_Shadows_StoreInGroupSharedMemory(gtid, normals, inp, depth);
}

void FFX_DNSR_Shadows_InitializeGroupSharedMemory(FfxInt32x2 did, FfxInt32x2 gtid)
{
    FfxInt32x2 offset_0 = FfxInt32x2(0, 0);
    FfxInt32x2 offset_1 = FfxInt32x2(8, 0);
    FfxInt32x2 offset_2 = FfxInt32x2(0, 8);
    FfxInt32x2 offset_3 = FfxInt32x2(8, 8);

    FfxFloat16x3 normals_0;
    FfxFloat16x2 input_0;
    FfxFloat32 depth_0;

    FfxFloat16x3 normals_1;
    FfxFloat16x2 input_1;
    FfxFloat32 depth_1;

    FfxFloat16x3 normals_2;
    FfxFloat16x2 input_2;
    FfxFloat32 depth_2;

    FfxFloat16x3 normals_3;
    FfxFloat16x2 input_3;
    FfxFloat32 depth_3;

    /// XA
    /// BC

    did -= 4;
    FFX_DNSR_Shadows_LoadWithOffset(did, offset_0, normals_0, input_0, depth_0); // X
    FFX_DNSR_Shadows_LoadWithOffset(did, offset_1, normals_1, input_1, depth_1); // A
    FFX_DNSR_Shadows_LoadWithOffset(did, offset_2, normals_2, input_2, depth_2); // B
    FFX_DNSR_Shadows_LoadWithOffset(did, offset_3, normals_3, input_3, depth_3); // C

    FFX_DNSR_Shadows_StoreWithOffset(gtid, offset_0, normals_0, input_0, depth_0); // X
    FFX_DNSR_Shadows_StoreWithOffset(gtid, offset_1, normals_1, input_1, depth_1); // A
    FFX_DNSR_Shadows_StoreWithOffset(gtid, offset_2, normals_2, input_2, depth_2); // B
    FFX_DNSR_Shadows_StoreWithOffset(gtid, offset_3, normals_3, input_3, depth_3); // C
}
#else
// Not used, defined to make sure f32 path compiles
FfxUInt32    FFX_DNSR_Shadows_PackFloat16(FfxFloat32x2 v){return 0;}
FfxFloat32x2 FFX_DNSR_Shadows_UnpackFloat16(FfxUInt32 a){return FfxFloat32x2(0,0);}
FfxFloat32x2 FFX_DNSR_Shadows_LoadInputFromGroupSharedMemory(FfxInt32x2 idx){return FfxFloat32x2(0,0);}
FfxFloat32x3 FFX_DNSR_Shadows_LoadNormalsFromGroupSharedMemory(FfxInt32x2 idx){return FfxFloat32x3(0,0,0);}
void         FFX_DNSR_Shadows_StoreInGroupSharedMemory(FfxInt32x2 idx, FfxFloat32x3 normals, FfxFloat32x2 inp, FfxFloat32 depth){}
void         FFX_DNSR_Shadows_LoadWithOffset(FfxInt32x2 did, FfxInt32x2 offset, out FfxFloat32x3 normals, out FfxFloat32x2 inp, out FfxFloat32 depth){depth = 0;}
void         FFX_DNSR_Shadows_StoreWithOffset(FfxInt32x2 gtid, FfxInt32x2 offset, FfxFloat32x3 normals, FfxFloat32x2 inp, FfxFloat32 depth){}
void FFX_DNSR_Shadows_InitializeGroupSharedMemory(FfxInt32x2 did, FfxInt32x2 gtid){}
#endif

FfxFloat32 FFX_DNSR_Shadows_GetShadowSimilarity(FfxFloat32 x1, FfxFloat32 x2, FfxFloat32 sigma)
{
    return exp(-abs(x1 - x2) / sigma);
}

FfxFloat32 FFX_DNSR_Shadows_GetDepthSimilarity(FfxFloat32 x1, FfxFloat32 x2, FfxFloat32 sigma)
{
    return exp(-abs(x1 - x2) / sigma);
}

FfxFloat32 FFX_DNSR_Shadows_GetNormalSimilarity(FfxFloat32x3 x1, FfxFloat32x3 x2)
{
    return ffxPow(ffxSaturate(dot(x1, x2)), 32.0f);
}

FfxFloat32 FFX_DNSR_Shadows_GetLinearDepth(FfxUInt32x2 did, FfxFloat32 depth)
{
    const FfxFloat32x2 uv = (did + 0.5f) * InvBufferDimensions();
    const FfxFloat32x2 ndc = 2.0f * FfxFloat32x2(uv.x, 1.0f - uv.y) - 1.0f;

    FfxFloat32x4 projected = FFX_MATRIX_MULTIPLY(ProjectionInverse(), FfxFloat32x4(ndc, depth, 1));

    return abs(projected.z / projected.w);
}

FfxFloat32 FFX_DNSR_Shadows_FetchFilteredVarianceFromGroupSharedMemory(FfxInt32x2 pos)
{
    const FfxInt32 k = 1;
    FfxFloat32 variance = 0.0f;
    const FfxFloat32 kernel[2][2] =
    {
        { 1.0f / 4.0f, 1.0f / 8.0f  },
        { 1.0f / 8.0f, 1.0f / 16.0f }
    };
    for (FfxInt32 y = -k; y <= k; ++y)
    {
        for (FfxInt32 x = -k; x <= k; ++x)
        {
            const FfxFloat32 w = kernel[abs(x)][abs(y)];
            variance += w * FFX_DNSR_Shadows_LoadInputFromGroupSharedMemory(pos + FfxInt32x2(x, y)).y;
        }
    }
    return variance;
}

void FFX_DNSR_Shadows_DenoiseFromGroupSharedMemory(FfxUInt32x2 did, FfxUInt32x2 gtid, inout FfxFloat32 weight_sum, inout FfxFloat32x2 shadow_sum, FfxFloat32 depth, FfxUInt32 stepsize)
{
    // Load our center sample
    const FfxFloat32x2 shadow_center = FFX_DNSR_Shadows_LoadInputFromGroupSharedMemory(FfxInt32x2(gtid));
    const FfxFloat32x3 normal_center = FFX_DNSR_Shadows_LoadNormalsFromGroupSharedMemory(FfxInt32x2(gtid));

    weight_sum = 1.0f;
    shadow_sum = shadow_center;

    const FfxFloat32 variance = FFX_DNSR_Shadows_FetchFilteredVarianceFromGroupSharedMemory(FfxInt32x2(gtid));
    const FfxFloat32 std_deviation = sqrt(max(variance + 1e-9f, 0.0f));
    const FfxFloat32 depth_center = FFX_DNSR_Shadows_GetLinearDepth(did, depth);    // linearize the depth value

    // Iterate filter kernel
    const FfxInt32 k = 1;
    const FfxFloat32 kernel[3] = { 1.0f, 2.0f / 3.0f, 1.0f / 6.0f };

    for (FfxInt32 y = -k; y <= k; ++y)
    {
        for (FfxInt32 x = -k; x <= k; ++x)
        {
            // Should we process this sample?
            const FfxInt32x2 step = FfxInt32x2(x, y) * FfxInt32x2(stepsize, stepsize);
            const FfxInt32x2 gtid_idx = FfxInt32x2(gtid) + step;
            const FfxInt32x2 did_idx = FfxInt32x2(did) + step;

            FfxFloat32 depth_neigh = FFX_DNSR_Shadows_LoadDepthFromGroupSharedMemory(gtid_idx);
            FfxFloat32x3 normal_neigh = FFX_DNSR_Shadows_LoadNormalsFromGroupSharedMemory(gtid_idx);
            FfxFloat32x2 shadow_neigh = FFX_DNSR_Shadows_LoadInputFromGroupSharedMemory(gtid_idx);

            FfxFloat32 sky_pixel_multiplier = ((x == 0 && y == 0) || depth_neigh >= 1.0f || depth_neigh <= 0.0f) ? 0 : 1; // Zero weight for sky pixels

            // Fetch our filtering values
            depth_neigh = FFX_DNSR_Shadows_GetLinearDepth(did_idx, depth_neigh);

            // Evaluate the edge-stopping function
            FfxFloat32 w = kernel[abs(x)] * kernel[abs(y)];  // kernel weight
            w *= FFX_DNSR_Shadows_GetShadowSimilarity(shadow_center.x, shadow_neigh.x, std_deviation);
            w *= FFX_DNSR_Shadows_GetDepthSimilarity(depth_center, depth_neigh, DepthSimilaritySigma());
            w *= FFX_DNSR_Shadows_GetNormalSimilarity(normal_center, normal_neigh);
            w *= sky_pixel_multiplier;

            // Accumulate the filtered sample
            shadow_sum += FfxFloat32x2(w, w * w) * shadow_neigh;
            weight_sum += w;
        }
    }
}

FfxFloat32x2 FFX_DNSR_Shadows_ApplyFilterWithPrecache(FfxUInt32x2 did, FfxUInt32x2 gtid, FfxUInt32 stepsize)
{
    FfxFloat32 weight_sum = 1.0;
    FfxFloat32x2 shadow_sum = FfxFloat32x2(0, 0);

    FFX_DNSR_Shadows_InitializeGroupSharedMemory(FfxInt32x2(did), FfxInt32x2(gtid));
    FfxBoolean needs_denoiser = IsShadowReciever(did);
    FFX_GROUP_MEMORY_BARRIER;
    if (needs_denoiser)
    {
        FfxFloat32 depth = LoadDepth(FfxInt32x2(did));
        gtid += 4; // Center threads in groupshared memory
        FFX_DNSR_Shadows_DenoiseFromGroupSharedMemory(did, gtid, weight_sum, shadow_sum, depth, stepsize);
    }

    FfxFloat32 mean = shadow_sum.x / weight_sum;
    FfxFloat32 variance = shadow_sum.y / (weight_sum * weight_sum);
    return FfxFloat32x2(mean, variance);
}

void FFX_DNSR_Shadows_ReadTileMetaData(FfxUInt32x2 gid, out FfxBoolean is_cleared, out FfxBoolean all_in_light)
{
    FfxUInt32 meta_data = LoadTileMetaData(gid.y * FFX_DNSR_Shadows_RoundedDivide(BufferDimensions().x, 8) + gid.x);
    is_cleared = FfxBoolean(meta_data & FfxUInt32(TILE_META_DATA_CLEAR_MASK));
    all_in_light = FfxBoolean(meta_data & FfxUInt32(TILE_META_DATA_LIGHT_MASK));
}


FfxFloat32x2 FFX_DNSR_Shadows_FilterSoftShadowsPass(FfxUInt32x2 gid, FfxUInt32x2 gtid, FfxUInt32x2 did, out FfxBoolean bWriteResults, const FfxUInt32 pass, const FfxUInt32 stepsize)
{
    FfxBoolean is_cleared;
    FfxBoolean all_in_light;
    FFX_DNSR_Shadows_ReadTileMetaData(gid, is_cleared, all_in_light);

    bWriteResults = FFX_FALSE;
    FfxFloat32x2 results = FfxFloat32x2(0, 0);

    if (is_cleared)
    {
        if (pass != 1)
        {
            results.x = all_in_light ? 1.0 : 0.0;
            bWriteResults = FFX_TRUE;
        }
    }
    else
    {
        results = FFX_DNSR_Shadows_ApplyFilterWithPrecache(did, gtid, stepsize);
        bWriteResults = FFX_TRUE;
    }

    return results;
}

void DenoiserShadowsFilterPass0(FfxUInt32x2 gid, FfxUInt32x2 gtid, FfxUInt32x2 did)
{
    const uint PASS_INDEX = 0;
    const uint STEP_SIZE = 1;

    FfxBoolean bWriteOutput = FFX_FALSE;
    const FfxFloat32x2 results = FFX_DNSR_Shadows_FilterSoftShadowsPass(gid, gtid, did, bWriteOutput, PASS_INDEX, STEP_SIZE);

    if (bWriteOutput)
    {
        StoreHistory(did, results);
    }
}

void DenoiserShadowsFilterPass1(FfxUInt32x2 gid, FfxUInt32x2 gtid, FfxUInt32x2 did)
{
    const uint PASS_INDEX = 1;
    const uint STEP_SIZE = 2;

    FfxBoolean bWriteOutput = FFX_FALSE;
    const FfxFloat32x2 results = FFX_DNSR_Shadows_FilterSoftShadowsPass(gid, gtid, did, bWriteOutput, PASS_INDEX, STEP_SIZE);

    if (bWriteOutput)
    {
        StoreHistory(did, results);
    }
}

void DenoiserShadowsFilterPass2(FfxUInt32x2 gid, FfxUInt32x2 gtid, FfxUInt32x2 did)
{
    const uint PASS_INDEX = 2;
    const uint STEP_SIZE = 4;

    FfxBoolean bWriteOutput = FFX_FALSE;
    const FfxFloat32x2 results = FFX_DNSR_Shadows_FilterSoftShadowsPass(gid, gtid, did, bWriteOutput, PASS_INDEX, STEP_SIZE);

    // Recover some of the contrast lost during denoising
    const FfxFloat32 shadow_remap = max(1.2f - results.y, 1.0f);
    const FfxFloat32 mean = pow(abs(results.x), shadow_remap);

    if (bWriteOutput)
    {
        StoreFilterOutput(did, mean);
    }
}

#endif
