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

#ifndef FFX_BRIXELIZER_GI_PROBE_SHADING_H
#define FFX_BRIXELIZER_GI_PROBE_SHADING_H

#include "../ffx_core.h"

FfxFloat32 FfxBrixelizerGIGetLuminance(FfxFloat32x3 color)
{
    return dot(color, FfxFloat32x3(FfxFloat32(0.299), FfxFloat32(0.587), FfxFloat32(0.114)));
}

#define FFX_BRIXELIZER_GI_LUMINANCE_WEIGHT FfxFloat32(0.2)
FfxFloat32 FfxBrixelizerGIGetLuminanceWeight(FfxFloat32x3 radiance, FfxFloat32 weight_k)
{
    return exp(-weight_k * FfxBrixelizerGIGetLuminance(radiance));
}

#define FFX_BRIXELIZER_GI_PI FfxFloat32(3.141592653589793238463)
// Reference:
// https://en.wikipedia.org/wiki/Table_of_spherical_harmonics#Real_spherical_harmonics with |r| == 1
// https://cseweb.ucsd.edu/~ravir/papers/envmap/envmap.pdf
#define FFX_BRIXELIZER_GI_SH_C0 FfxFloat32(0.2820947917738781) // 0.5 * math.sqrt(1.0 / math.pi)
#define FFX_BRIXELIZER_GI_SH_C1 FfxFloat32(0.4886025119029199) // 0.5 * math.sqrt(3.0 / math.pi)
#define FFX_BRIXELIZER_GI_SH_2_PI_3 ((FfxFloat32(2.0) * FFX_BRIXELIZER_GI_PI) / FfxFloat32(3.0))
#define FFX_BRIXELIZER_GI_SH_PI_4 (FFX_BRIXELIZER_GI_PI / FfxFloat32(4.0))
#define FFX_BRIXELIZER_GI_SH_C4 FfxFloat32(1.0925484305920792)  //  0.5 * math.sqrt(15.0 / math.pi)
#define FFX_BRIXELIZER_GI_SH_C5 FfxFloat32(0.31539156525252005) //  0.25 * math.sqrt(5.0 / math.pi))
#define FFX_BRIXELIZER_GI_SH_C6 FfxFloat32(0.5462742152960396)  //  0.25 * math.sqrt(15.0 / math.pi)
#define FFX_BRIXELIZER_GI_SH_C7 FfxFloat32(0.24770795610037571) //  math.pi / 4.0 * 0.25 * math.sqrt(5.0 / (math.pi))
#define FFX_BRIXELIZER_GI_SH_C8 FfxFloat32(1.023326707946489)   //  math.sqrt(math.pi / 3)
#define FFX_BRIXELIZER_GI_SH_C9 FfxFloat32(0.8862269254527579)  // math.sqrt(math.pi / 3)
#define FFX_BRIXELIZER_GI_SH_C10 FfxFloat32(0.8580855308097834) // math.sqrt(math.pi * 15.0 / (16.0 * 4.0))
#define FFX_BRIXELIZER_GI_SH_C11 FfxFloat32(0.4290427654048917) // math.sqrt(3.0 * 2.0 * math.pi) / math.sqrt(64.0 * 8.0 / 5.0)

// We use 3 bands(9 coefficients)
void FfxBrixelizerGISHGetCoefficients(FfxFloat32x3 direction, out FfxFloat32 coefficients[9])
{
    coefficients[0] = FfxFloat32(1.0);

    coefficients[1] = direction.y;
    coefficients[2] = direction.z;
    coefficients[3] = direction.x;

    coefficients[4] = direction.x * direction.y;
    coefficients[5] = direction.y * direction.z;
    coefficients[6] = FfxFloat32(3.0) * direction.z * direction.z - FfxFloat32(1.0);
    coefficients[7] = direction.x * direction.z;
    coefficients[8] = direction.x * direction.x - direction.y * direction.y;

    //

    coefficients[0] = coefficients[0] * FFX_BRIXELIZER_GI_SH_C0;

    coefficients[1] = coefficients[1] * FFX_BRIXELIZER_GI_SH_C1;
    coefficients[2] = coefficients[2] * FFX_BRIXELIZER_GI_SH_C1;
    coefficients[3] = coefficients[3] * FFX_BRIXELIZER_GI_SH_C1;

    coefficients[4] = coefficients[4] * FFX_BRIXELIZER_GI_SH_C4;
    coefficients[5] = coefficients[5] * FFX_BRIXELIZER_GI_SH_C4;
    coefficients[6] = coefficients[6] * FFX_BRIXELIZER_GI_SH_C5;
    coefficients[7] = coefficients[7] * FFX_BRIXELIZER_GI_SH_C4;
    coefficients[8] = coefficients[8] * FFX_BRIXELIZER_GI_SH_C6;
}

void FfxBrixelizerGISHGetCoefficients16(FfxFloat32x3 direction, out FFX_MIN16_F coefficients[9])
{
    coefficients[0] = FFX_MIN16_F(1.0);

    coefficients[1] = FFX_MIN16_F(direction.y);
    coefficients[2] = FFX_MIN16_F(direction.z);
    coefficients[3] = FFX_MIN16_F(direction.x);

    coefficients[4] = FFX_MIN16_F(direction.x * direction.y);
    coefficients[5] = FFX_MIN16_F(direction.y * direction.z);
    coefficients[6] = FFX_MIN16_F(3.0) * FFX_MIN16_F(direction.z * direction.z) - FFX_MIN16_F(1.0);
    coefficients[7] = FFX_MIN16_F(direction.x * direction.z);
    coefficients[8] = FFX_MIN16_F(direction.x * direction.x - direction.y * direction.y);

    //

    coefficients[0] = coefficients[0] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C0);

    coefficients[1] = coefficients[1] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C1);
    coefficients[2] = coefficients[2] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C1);
    coefficients[3] = coefficients[3] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C1);

    coefficients[4] = coefficients[4] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C4);
    coefficients[5] = coefficients[5] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C4);
    coefficients[6] = coefficients[6] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C5);
    coefficients[7] = coefficients[7] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C4);
    coefficients[8] = coefficients[8] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_C6);
}

void FfxBrixelizerGISHGetCoefficients_ClampedCosine(FfxFloat32x3 cosine_lobe_dir, out FfxFloat32 coefficients[9])
{
    FfxBrixelizerGISHGetCoefficients(cosine_lobe_dir, /* out */ coefficients);
    coefficients[0] = coefficients[0] * FFX_BRIXELIZER_GI_PI;

    coefficients[1] = coefficients[1] * FFX_BRIXELIZER_GI_SH_2_PI_3;
    coefficients[2] = coefficients[2] * FFX_BRIXELIZER_GI_SH_2_PI_3;
    coefficients[3] = coefficients[3] * FFX_BRIXELIZER_GI_SH_2_PI_3;

    coefficients[4] = coefficients[4] * FFX_BRIXELIZER_GI_SH_PI_4;
    coefficients[5] = coefficients[5] * FFX_BRIXELIZER_GI_SH_PI_4;
    coefficients[6] = coefficients[6] * FFX_BRIXELIZER_GI_SH_PI_4;
    coefficients[7] = coefficients[7] * FFX_BRIXELIZER_GI_SH_PI_4;
    coefficients[8] = coefficients[8] * FFX_BRIXELIZER_GI_SH_PI_4;
}

void FfxBrixelizerGISHGetCoefficients_ClampedCosine16(FfxFloat32x3 cosine_lobe_dir, out FFX_MIN16_F coefficients[9])
{
    FfxBrixelizerGISHGetCoefficients16(cosine_lobe_dir, /* out */ coefficients);
    coefficients[0] = coefficients[0] * FFX_MIN16_F(FFX_BRIXELIZER_GI_PI);

    coefficients[1] = coefficients[1] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_2_PI_3);
    coefficients[2] = coefficients[2] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_2_PI_3);
    coefficients[3] = coefficients[3] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_2_PI_3);

    coefficients[4] = coefficients[4] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_PI_4);
    coefficients[5] = coefficients[5] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_PI_4);
    coefficients[6] = coefficients[6] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_PI_4);
    coefficients[7] = coefficients[7] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_PI_4);
    coefficients[8] = coefficients[8] * FFX_MIN16_F(FFX_BRIXELIZER_GI_SH_PI_4);
}

void FfxBrixelizerGILoadBrickSH(FfxUInt32 brick_id, inout FfxFloat32x4 input_sh[9])
{
    for (FfxInt32 i = 0; i < 9; i++) {
        input_sh[i] = ffxUnpackF32x2(LoadBricksSH(FfxBrixelizerBrickGetIndex(brick_id) * 9 + i));
    }
}

void FfxBrixelizerGILoadBrickSH16(FfxUInt32 brick_id, inout FFX_MIN16_F4 input_sh[9])
{
    for (FfxInt32 i = 0; i < 9; i++) {
        input_sh[i] = FFX_MIN16_F4(ffxUnpackF32x2(LoadBricksSH(FfxBrixelizerBrickGetIndex(brick_id) * 9 + i)));
    }
}

void FfxBrixelizerGIStoreBrickSH(FfxUInt32 brick_id, FfxFloat32x4 input_sh[9])
{
    for (FfxInt32 i = 0; i < 9; i++) {
        StoreBricksSH(FfxBrixelizerBrickGetIndex(brick_id) * 9 + i, ffxFloat16x4ToUint32x2(FFX_MIN16_F4(input_sh[i])));
    }
}

void FfxBrixelizerGILoadBrickDirectSH(FfxUInt32 brick_id, inout FfxFloat32x4 input_sh[9])
{
    for (FfxInt32 i = 0; i < 9; i++) {
        input_sh[i] = ffxUnpackF32x2(LoadBricksDirectSH(FfxBrixelizerBrickGetIndex(brick_id) * 9 + i));
    }
}

void FfxBrixelizerGILoadBrickDirectSH16(FfxUInt32 brick_id, inout FFX_MIN16_F4 input_sh[9])
{
    for (FfxInt32 i = 0; i < 9; i++) {
        input_sh[i] = FFX_MIN16_F4(ffxUnpackF32x2(LoadBricksDirectSH(FfxBrixelizerBrickGetIndex(brick_id) * 9 + i)));
    }
}

void FfxBrixelizerGIStoreBrickDirectSH(FfxUInt32 brick_id, FfxFloat32x4 input_sh[9])
{
    for (FfxInt32 i = 0; i < 9; i++) {
        StoreBricksDirectSH(FfxBrixelizerBrickGetIndex(brick_id) * 9 + i, ffxFloat16x4ToUint32x2(FFX_MIN16_F4(input_sh[i])));
    }
}

// Pick the current voxel size for pushoff
FfxFloat32 FfxBrixelizerGIGetVoxelSize(FfxFloat32x3 world_pos, FfxUInt32 g_starting_cascade, FfxUInt32 g_end_cascade, FfxFloat32 xi)
{
    FfxBrixelizerCascadeInfo CINFO = GetCascadeInfo(g_starting_cascade);
    FfxFloat32               size  = CINFO.grid_max.x - CINFO.grid_min.x;
    FfxFloat32               r     = length(world_pos - CINFO.grid_mid) / (size * FfxFloat32(0.25)); // FfxFloat32(0.288675134594812));
    FfxFloat32               x     = log2(ffxMax(r, FfxFloat32(1.0)));
    return CINFO.voxel_size * ffxMax(FfxFloat32(1.0), r);
}

FfxFloat32x3 FfxBrixelizerGISampleRadianceCache(FfxUInt32 brick_id, FfxFloat32x3 uvw)
{
    FfxUInt32x3  brick_offset = FfxBrixelizerGetSDFAtlasOffset(brick_id);
    FfxFloat32x3 uvw_min      = (FfxFloat32x3(brick_offset / 2) + FFX_BROADCAST_FLOAT32X3(FfxFloat32(0.5))) / FfxFloat32(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE / 2);
    FfxFloat32x3 uvw_max      = (FfxFloat32x3(brick_offset / 2) + FFX_BROADCAST_FLOAT32X3(FfxFloat32(3.5))) / FfxFloat32(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE / 2);
    FfxFloat32x3 sample_uvw   = ffxLerp(uvw_min, uvw_max, ffxSaturate(uvw));
    FfxFloat32x3 radiance     = SampleRadianceCacheSRV(sample_uvw);

    if (any(isnan(radiance)))
        radiance = FFX_BROADCAST_FLOAT32X3(0.0f);

    return radiance;
}

FfxFloat32x3 FfxBrixelizerGISampleRadianceCacheSH(FfxUInt32 brick_id, FfxFloat32x3 ray_direction)
{
    FFX_MIN16_F cosine_sh[9];
    FfxBrixelizerGISHGetCoefficients_ClampedCosine16(-ray_direction, cosine_sh);

    FFX_MIN16_F4 shs[9];
    FfxBrixelizerGILoadBrickDirectSH16(brick_id, shs);

    FfxFloat32x3 radiance = FFX_BROADCAST_FLOAT32X3(0.0f);
        
    for (FfxUInt32 i = 0; i < 9; i++)
        radiance += cosine_sh[i] * shs[i].xyz;

    if (any(isnan(radiance)))
        radiance = FFX_BROADCAST_FLOAT32X3(0.0f);

    return radiance;
}

FfxFloat32x3 FfxBrixelizerGISampleRadianceCache(FfxUInt32 cascade_id, FfxFloat32x3 dir, FfxBrixelizerCascadeInfo sdfCINFO, FfxFloat32x3 world_pos)
{
    FfxFloat32x3 rel_pos      = world_pos - sdfCINFO.grid_min;
    FfxInt32x3   voxel_offset = FfxInt32x3(rel_pos / sdfCINFO.voxel_size);
    FfxFloat32x3 uvw          = (rel_pos - FfxFloat32x3(voxel_offset) * sdfCINFO.voxel_size) / sdfCINFO.voxel_size;
    FfxUInt32    voxel_idx    = FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(FfxInt32x3(sdfCINFO.clipmap_offset), FFX_BRIXELIZER_CASCADE_WRAP_MASK, FfxUInt32x3(voxel_offset)), FFX_BRIXELIZER_CASCADE_DEGREE);
    FfxUInt32    brick_id     = LoadCascadeBrickMapArrayUniform(cascade_id, voxel_idx);

    if (FfxBrixelizerIsValidID(brick_id)) 
    {
        FfxUInt32x4  sample_sh_state = LoadBricksSHState(FfxBrixelizerBrickGetIndex(brick_id));
        FFX_MIN16_F4 sample_dir_w    = FFX_MIN16_F4(ffxUnpackF32x2(sample_sh_state.xy));

        FfxUInt32x3   brick_offset = FfxBrixelizerGetSDFAtlasOffset(brick_id);
        FfxFloat32x3  uvw_min      = (FfxFloat32x3(brick_offset / 2) + FFX_BROADCAST_FLOAT32X3(FfxFloat32(0.5))) / FfxFloat32(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE / 2);
        FfxFloat32x3  uvw_max      = (FfxFloat32x3(brick_offset / 2) + FFX_BROADCAST_FLOAT32X3(FfxFloat32(3.5))) / FfxFloat32(FFX_BRIXELIZER_STATIC_CONFIG_SDF_ATLAS_SIZE / 2);
        uvw                        = ffxLerp(uvw_min, uvw_max, ffxSaturate(uvw));
        FfxFloat32x3  radiance     = SampleRadianceCacheSRV(uvw);

        if (any(isnan(radiance)))
            radiance = FFX_BROADCAST_FLOAT32X3(0.0f);

        return radiance;
    }
    
    return FFX_BROADCAST_FLOAT32X3(0.0);
}
FfxFloat32x3 FfxBrixelizerGISampleRadianceCacheSH(FfxUInt32 cascade_id, FfxFloat32x3 world_normal, FfxBrixelizerCascadeInfo sdfCINFO, FfxFloat32x3 world_pos)
{
    FfxFloat32x3 rel_pos      = world_pos - sdfCINFO.grid_min;
    FfxInt32x3   voxel_offset = FfxInt32x3(rel_pos / sdfCINFO.voxel_size);
    FfxUInt32    voxel_idx    = FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(FfxInt32x3(sdfCINFO.clipmap_offset), FFX_BRIXELIZER_CASCADE_WRAP_MASK, FfxUInt32x3(voxel_offset)), FFX_BRIXELIZER_CASCADE_DEGREE);
    FfxUInt32    brick_id     = LoadCascadeBrickMapArrayUniform(cascade_id, voxel_idx);
    
    if (FfxBrixelizerIsValidID(brick_id)) 
    {
        FFX_MIN16_F cosine_sh[9];
        FfxBrixelizerGISHGetCoefficients_ClampedCosine16(world_normal, cosine_sh);

        FFX_MIN16_F4 shs[9];
        FfxBrixelizerGILoadBrickDirectSH16(brick_id, shs);

        FfxFloat32x3 radiance = FFX_BROADCAST_FLOAT32X3(0.0f);
        
        for (uint i = 0; i < 9; i++)
            radiance += cosine_sh[i] * shs[i].xyz;

        if (any(isnan(radiance)))
            radiance = FFX_BROADCAST_FLOAT32X3(0.0f);

        return radiance;
    }
    
    return FFX_BROADCAST_FLOAT32X3(0.0);
}

FfxBoolean FfxBrixelizerGISampleRadianceCache(FfxFloat32x3 world_offset, FfxFloat32x3 dir, FfxFloat32x3 grad, FfxUInt32 g_starting_cascade, FfxUInt32 g_end_cascade, inout FfxFloat32x3 cache, FfxFloat32 depth_eps)
{
    FfxFloat32x3 world_pos = world_offset;
    for (FfxUInt32 cascade_id = g_starting_cascade; cascade_id <= g_end_cascade; cascade_id++) 
    {    
        FfxBrixelizerCascadeInfo sdfCINFO = GetCascadeInfo(cascade_id);

        if (sdfCINFO.is_enabled == 1 && all(FFX_GREATER_THAN(world_pos, sdfCINFO.grid_min)) && all(FFX_LESS_THAN(world_pos, sdfCINFO.grid_max))) 
        {
            FfxFloat32x3     brick_sample = FfxBrixelizerGISampleRadianceCache(cascade_id, dir, sdfCINFO, world_pos);
            const FfxFloat32 EPS          = FfxFloat32(1.0e-3);
            // Causes light leaks if the brick_sample is fully unlit.
            //if (dot(brick_sample, brick_sample) > EPS * EPS) 
            {
                cache = brick_sample;
                return true;
            }
        }
    }
    cache = FFX_BROADCAST_FLOAT32X3(0.0f);
    return false;
}
FfxBoolean FfxBrixelizerGISampleRadianceCache(FfxFloat32x3 world_offset, FfxFloat32x3 dir, FfxFloat32x3 grad, FfxUInt32 g_starting_cascade, FfxUInt32 g_end_cascade, inout FfxFloat32x3 cache)
{
    return FfxBrixelizerGISampleRadianceCache(world_offset, dir, grad, g_starting_cascade, g_end_cascade, cache, FfxFloat32(1.0 / 8.0));
}
FfxBoolean FfxBrixelizerGISampleRadianceCacheSH(FfxFloat32x3 world_pos, FfxFloat32x3 world_normal, FfxUInt32 g_starting_cascade, FfxUInt32 g_end_cascade, inout FfxFloat32x3 cache)
{

    for (FfxUInt32 cascade_id = g_starting_cascade; cascade_id <= g_end_cascade; cascade_id++) 
    {
        FfxBrixelizerCascadeInfo sdfCINFO = GetCascadeInfo(cascade_id);

        if (sdfCINFO.is_enabled == 1 && all(FFX_GREATER_THAN(world_pos, sdfCINFO.grid_min)) && all(FFX_LESS_THAN(world_pos, sdfCINFO.grid_max))) 
        {
            FfxFloat32x3 brick_sample = FfxBrixelizerGISampleRadianceCacheSH(cascade_id, world_normal, sdfCINFO, world_pos);
            
            cache = brick_sample;
            return true;
        }
    }
    cache = FFX_BROADCAST_FLOAT32X3(0.0f);
    return false;
}
FfxBoolean FfxBrixelizerGILoadBrickSH(FfxFloat32x3 world_pos, FfxUInt32 g_starting_cascade, FfxUInt32 g_end_cascade, inout FfxFloat32x4 input_sh[9])
{
    for (FfxUInt32 cascade_id = g_starting_cascade; cascade_id <= g_end_cascade; cascade_id++) 
    {
        FfxBrixelizerCascadeInfo CINFO   = GetCascadeInfo(cascade_id);
        FfxFloat32x3             rel_pos = world_pos - CINFO.grid_min;
        FfxFloat32               size    = CINFO.grid_max.x - CINFO.grid_min.x;

        if (CINFO.is_enabled == 1 && all(FFX_GREATER_THAN(rel_pos, FFX_BROADCAST_FLOAT32X3(0.0))) && all(FFX_LESS_THAN(rel_pos, FFX_BROADCAST_FLOAT32X3(size)))) 
        {
            FfxInt32x3 voxel_offset = FfxInt32x3(rel_pos * CINFO.ivoxel_size);
            FfxUInt32  voxel_idx    = FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(FfxInt32x3(CINFO.clipmap_offset), FFX_BRIXELIZER_CASCADE_WRAP_MASK, FfxUInt32x3(voxel_offset)), FFX_BRIXELIZER_CASCADE_DEGREE);
            FfxUInt32  brick_id     = LoadCascadeBrickMapArrayUniform(cascade_id, voxel_idx);
            
            if (FfxBrixelizerIsValidID(brick_id)) 
            {
                FfxBrixelizerGILoadBrickSH(brick_id, /* inout */ input_sh);
                return true;
            }
        }
    }
    return false;
}

#define FfxBrixelizerGIGOLDEN_RATIO FfxFloat32(1.61803398875)
FfxBoolean FfxBrixelizerGIInterpolateBrickSH(FfxFloat32x3 world_pos, FfxUInt32 g_starting_cascade, FfxUInt32 g_end_cascade, FfxFloat32 xi, inout FFX_MIN16_F4 input_sh[9])
{
    FfxUInt32 bricks[8] = {
        FFX_BRIXELIZER_UNINITIALIZED_ID, //
        FFX_BRIXELIZER_UNINITIALIZED_ID, //
        FFX_BRIXELIZER_UNINITIALIZED_ID, //
        FFX_BRIXELIZER_UNINITIALIZED_ID, //
        FFX_BRIXELIZER_UNINITIALIZED_ID, //
        FFX_BRIXELIZER_UNINITIALIZED_ID, //
        FFX_BRIXELIZER_UNINITIALIZED_ID, //
        FFX_BRIXELIZER_UNINITIALIZED_ID, //
    };

    FFX_MIN16_F3 uvw;

    for (FfxUInt32 cascade_id = g_starting_cascade; cascade_id <= g_end_cascade; cascade_id++) 
    {
        FfxBrixelizerCascadeInfo CINFO   = GetCascadeInfo(cascade_id);
        FfxFloat32x3             rel_pos = world_pos - CINFO.grid_min;
        FfxFloat32               size    = CINFO.grid_max.x - CINFO.grid_min.x;
        FfxFloat32               falloff = length(world_pos - CINFO.grid_mid) / (size / FfxFloat32(2.0));
        FfxBoolean               skip    = falloff > (FfxFloat32(1.0) - (xi * FfxFloat32(8.0) + FfxFloat32(0.0)) / FfxFloat32(FFX_BRIXELIZER_CASCADE_RESOLUTION));
        
        if (CINFO.is_enabled == 1 && !skip && all(FFX_GREATER_THAN(rel_pos, FFX_BROADCAST_FLOAT32X3(0.0))) && all(FFX_LESS_THAN(rel_pos, FFX_BROADCAST_FLOAT32X3(size)))) 
        {
            FfxFloat32x3 scaled_pos = rel_pos * CINFO.ivoxel_size - FFX_BROADCAST_FLOAT32X3(0.5);
            uvw                     = FFX_MIN16_F3(ffxFract(scaled_pos));
            FfxInt32x3 base_coord   = FfxInt32x3(floor(scaled_pos));
            FfxInt32x3 coords[8]    = {
                FfxInt32x3(scaled_pos) + FfxInt32x3(0, 0, 0), //
                FfxInt32x3(scaled_pos) + FfxInt32x3(1, 0, 0), //
                FfxInt32x3(scaled_pos) + FfxInt32x3(0, 1, 0), //
                FfxInt32x3(scaled_pos) + FfxInt32x3(1, 1, 0), //
                FfxInt32x3(scaled_pos) + FfxInt32x3(0, 0, 1), //
                FfxInt32x3(scaled_pos) + FfxInt32x3(1, 0, 1), //
                FfxInt32x3(scaled_pos) + FfxInt32x3(0, 1, 1), //
                FfxInt32x3(scaled_pos) + FfxInt32x3(1, 1, 1), //
            };

            for (FfxInt32 i = 0; i < 8; i++) 
            {
                if (any(FFX_LESS_THAN(coords[i], FFX_BROADCAST_INT32X3(0))) && any(FFX_GREATER_THAN_EQUAL(coords[i], FFX_BROADCAST_INT32X3(FFX_BRIXELIZER_CASCADE_RESOLUTION))))
                    continue;

                FfxUInt32 voxel_idx = FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(FfxInt32x3(CINFO.clipmap_offset), FFX_BRIXELIZER_CASCADE_WRAP_MASK, FfxUInt32x3(coords[i])), FFX_BRIXELIZER_CASCADE_DEGREE);
                FfxUInt32 brick_id  = LoadCascadeBrickMapArrayNonUniform(cascade_id, voxel_idx);
                
                if (FfxBrixelizerIsValidID(brick_id))
                    bricks[i] = brick_id;
            }
            break;
        }
        xi = ffxFract(xi + FfxFloat32(cascade_id) * FfxBrixelizerGIGOLDEN_RATIO);
    }

    FFX_MIN16_F weights[8] = {
        (FFX_MIN16_F(1.0) - uvw.x) * (FFX_MIN16_F(1.0) - uvw.y) * (FFX_MIN16_F(1.0) - uvw.z), //
        uvw.x * (FFX_MIN16_F(1.0) - uvw.y) * (FFX_MIN16_F(1.0) - uvw.z),               //
        (FFX_MIN16_F(1.0) - uvw.x) * uvw.y * (FFX_MIN16_F(1.0) - uvw.z),               //
        uvw.x * uvw.y * (FFX_MIN16_F(1.0) - uvw.z),                             //
        (FFX_MIN16_F(1.0) - uvw.x) * (FFX_MIN16_F(1.0) - uvw.y) * uvw.z,               //
        uvw.x * (FFX_MIN16_F(1.0) - uvw.y) * uvw.z,                             //
        (FFX_MIN16_F(1.0) - uvw.x) * uvw.y * uvw.z,                             //
        uvw.x * uvw.y * uvw.z,                                           //
    };

    FFX_MIN16_F weight_sum = FFX_MIN16_F(0.0);
    
    for (FfxInt32 j = 0; j < 9; j++)
        input_sh[j] = FFX_BROADCAST_MIN_FLOAT16X4(FFX_MIN16_F(0.0));

    for (FfxInt32 i = 0; i < 8; i++) 
    {
        if (bricks[i] == FFX_BRIXELIZER_UNINITIALIZED_ID)
            continue;

        FFX_MIN16_F4 shs[9];
        FfxBrixelizerGILoadBrickSH16(bricks[i], /* inout */ shs);

        if (shs[0].w < FfxFloat32(1.0))
            continue; // skip if less than 16 samples of data as too noisy

        for (FfxInt32 j = 0; j < 9; j++)
            input_sh[j] += shs[j] * weights[i];

        weight_sum += weights[i];
    }
    for (FfxInt32 j = 0; j < 9; j++)
        input_sh[j] /= ffxMax(weight_sum, FFX_MIN16_F(1.0e-6));

    return ffxAsUInt32(FfxFloat32(weight_sum)) != 0;
}

void FfxBrixelizerGIEmitIrradiance(FfxFloat32x3 world_pos, FfxFloat32x3 probe_direction, FfxFloat32x3 ray_direction, FfxFloat32x4 input_sh[9], FfxFloat32 xi, FfxUInt32 g_starting_cascade, FfxUInt32 g_end_cascade)
{
    FfxFloat32 DoV = ffxSaturate(dot(-ray_direction, probe_direction));
    FfxFloat32 jk  = ffxLerp(FfxFloat32(1.0), FfxFloat32(1.0 / 8.0), DoV);
    xi             = ffxLerp(-jk, jk, xi); // voxel jitter

    for (FfxUInt32 cascade_id = g_starting_cascade; cascade_id <= g_end_cascade; cascade_id++) 
    {
        FfxFloat32               max_num_samples = FfxFloat32(64.0) * ffxPow(FfxFloat32(1.3), FfxFloat32(cascade_id - g_starting_cascade));
        FfxBrixelizerCascadeInfo CINFO           = GetCascadeInfo(cascade_id);
        FfxFloat32x3             rel_pos         = world_pos - (CINFO.voxel_size) * ray_direction * xi - CINFO.grid_min;
        FfxFloat32               size            = CINFO.grid_max.x - CINFO.grid_min.x;

        if (CINFO.is_enabled == 1 && all(FFX_GREATER_THAN(rel_pos, FFX_BROADCAST_FLOAT32X3(0.0))) && all(FFX_LESS_THAN(rel_pos, FFX_BROADCAST_FLOAT32X3(size)))) 
        {
            FfxInt32x3 voxel_offset = FfxInt32x3(rel_pos * CINFO.ivoxel_size);
            FfxUInt32  voxel_idx    = FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(FfxInt32x3(CINFO.clipmap_offset), FFX_BRIXELIZER_CASCADE_WRAP_MASK, FfxUInt32x3(voxel_offset)), FFX_BRIXELIZER_CASCADE_DEGREE);
            FfxUInt32  brick_id     = LoadCascadeBrickMapArrayUniform(cascade_id, voxel_idx);
            
            if (FfxBrixelizerIsValidID(brick_id)) 
            {
                FfxFloat32x4 src_sh[9];
                FfxBrixelizerGILoadBrickSH(brick_id, /* inout */ src_sh);
                FfxFloat32 num_samples = ffxMin(max_num_samples, src_sh[0].w);

                FfxUInt32x4  sh_state = LoadBricksSHState(FfxBrixelizerBrickGetIndex(brick_id));
                FFX_MIN16_F4 dir_w    = FFX_MIN16_F4(ffxUnpackF32x2(sh_state.xy));
                if (any(isinf(dir_w)) || any(isnan(dir_w)))
                    dir_w = FFX_BROADCAST_MIN_FLOAT16X4(FFX_MIN16_F(0.0));

                FfxFloat32 weight = FfxFloat32(1.0) - FfxFloat32(1.0) / (FfxFloat32(1.0) + num_samples);

                FfxFloat32x4 shs[9];
                for (FfxInt32 i = 0; i < 9; i++) 
                {
                    FfxFloat32x4      src = src_sh[i];
                    const FfxFloat32  EPS = FfxFloat32(1.0e-4);
                    FfxFloat32x4 new_value = ffxLerp(input_sh[i], src, weight);
                    new_value.w      = num_samples + FfxFloat32(1.0);

                    if (any(isnan(new_value)))
                        new_value = FFX_BROADCAST_FLOAT32X4(0.0);
                    
                    shs[i] = new_value;
                }

                dir_w.xyz = normalize(ffxLerp(FFX_MIN16_F3(probe_direction), dir_w.xyz, FFX_MIN16_F(weight)));

                sh_state.xy = ffxFloat16x4ToUint32x2(dir_w);
                StoreBricksSHState(FfxBrixelizerBrickGetIndex(brick_id), sh_state);

                FfxBrixelizerGIStoreBrickSH(brick_id, shs);
            }
        }
    }
}

FfxInt32x3 FfxBrixelizerGIPickMajorDir(FfxFloat32x3 r)
{
    if (abs(r.x) > abs(r.y)) 
    {
        if (abs(r.x) > abs(r.z)) 
            return FfxInt32x3(r.x > FfxFloat32(0.0) ? 1 : -1, 0, 0);
        else
            return FfxInt32x3(0, 0, r.z > FfxFloat32(0.0) ? 1 : -1);
    } 
    else 
    {
        if (abs(r.y) > abs(r.z))
            return FfxInt32x3(0, r.y > FfxFloat32(0.0) ? 1 : -1, 0);
        else
            return FfxInt32x3(0, 0, r.z > FfxFloat32(0.0) ? 1 : -1);
    }
}

void FfxBrixelizerGIEmitRadiance(FfxUInt32 brick_id, FfxFloat32x3 uvw, FfxFloat32x3 direction, FfxFloat32x3 radiance, FfxFloat32 weight)
{
    FfxUInt32x3      brick_offset = FfxBrixelizerGetSDFAtlasOffset(brick_id) / 2;
    FfxInt32x3       coord        = FfxInt32x3(ffxSaturate(uvw) * FfxFloat32(4.0));
    FfxFloat32x3     src          = LoadRadianceCache(brick_offset + coord);
    const FfxFloat32 EPS          = FfxFloat32(1.0e-6);

    if (dot(src, src) < EPS * EPS)
        weight = FfxFloat32(0.0);

    FfxFloat32x3 new_value = ffxLerp(radiance, src, weight);

    if (any(isnan(new_value)))
        new_value = FFX_BROADCAST_FLOAT32X3(0.0);

    StoreRadianceCache(brick_offset + coord, new_value);

    {
        FfxFloat32x4 src_sh[9];
        FfxBrixelizerGILoadBrickDirectSH(brick_id, /* inout */ src_sh);

        FfxFloat32 direction_sh[9];
        FfxBrixelizerGISHGetCoefficients(direction, direction_sh);

        for (FfxUInt32 j = 0; j < 9; j++) 
        {
            src_sh[j].xyz = ffxLerp(direction_sh[j] * radiance.xyz, src_sh[j].xyz, weight);
            src_sh[j].w += FfxFloat32(1.0);
        }

        FfxBrixelizerGIStoreBrickDirectSH(brick_id, src_sh);
    }

    // g_rw_pctx_volume_cache[brick_offset + coord] = ffxLerp(color, src, FfxFloat32(0.95));
    // break; // early out and let the rest for Propagate pass
}

// xi - random [0, 1]
void FfxBrixelizerGIEmitRadiance(FfxFloat32x3 world_pos, FfxFloat32x3 world_normal, FfxFloat32x3 ray_direction, FfxFloat32x3 radiance, FfxFloat32 xi, FfxUInt32 g_starting_cascade, FfxUInt32 g_end_cascade)
{
    radiance  = clamp(radiance, FFX_BROADCAST_FLOAT32X3(0.0), FFX_BROADCAST_FLOAT32X3(8.0)); // Some clamping has to be here for specular highlights
    FfxFloat32 DoV = ffxSaturate(dot(-ray_direction, world_normal));
    FfxFloat32 jk  = ffxLerp(FfxFloat32(4.0), FfxFloat32(1.0 / 8.0), DoV); // TODO: Make this tunable
    
    for (FfxUInt32 cascade_id = g_starting_cascade; cascade_id <= g_end_cascade; cascade_id++) 
    {
        FfxBrixelizerCascadeInfo sdfCINFO = GetCascadeInfoNonUniform(cascade_id);
        FfxFloat32               random_jitter = ffxLerp(-jk, jk, xi) * GetCascadeInfoNonUniform(g_starting_cascade).voxel_size; // voxel jitter

        // Jitter along the camera primary ray to inject into neighbor voxels
        FfxFloat32x3 rel_pos = world_pos - ray_direction * random_jitter - sdfCINFO.grid_min;
        FfxFloat32   size    = sdfCINFO.grid_max.x - sdfCINFO.grid_min.x;

        // Check if relative position is within the cascade grid extents
        if (sdfCINFO.is_enabled == 1 && all(FFX_GREATER_THAN(rel_pos, FFX_BROADCAST_FLOAT32X3(0.0))) && all(FFX_LESS_THAN(rel_pos, FFX_BROADCAST_FLOAT32X3(size)))) 
        {

            // Convert the relative voxel position into a voxel grid coordinate
            FfxInt32x3 voxel_offset = FfxInt32x3(rel_pos / sdfCINFO.voxel_size);

            // Generate a 3D texture coordinate from 
            FfxFloat32x3 uvw = (rel_pos - FfxFloat32x3(voxel_offset) * sdfCINFO.voxel_size) / sdfCINFO.voxel_size;
            uvw = ffxSaturate(uvw);
            FfxFloat32x3 uvw_c = uvw - FFX_BROADCAST_FLOAT32X3(0.5);
            FfxInt32x3   md    = FfxBrixelizerGIPickMajorDir(uvw_c);

            FfxUInt32 voxel_idx = FfxBrixelizerFlattenPOT(FfxBrixelizerWrapCoords(FfxInt32x3(sdfCINFO.clipmap_offset), FFX_BRIXELIZER_CASCADE_WRAP_MASK, FfxUInt32x3(voxel_offset)), FFX_BRIXELIZER_CASCADE_DEGREE);
            FfxUInt32 brick_id  = LoadCascadeBrickMapArrayNonUniform(cascade_id, voxel_idx);
            
            if (FfxBrixelizerIsValidID(brick_id)) 
            {
                FfxFloat32 weight = FfxFloat32(1.0) - FfxFloat32(1.0 / (FfxFloat32(8.0) * ffxPow(FfxFloat32(2.0), FfxFloat32(cascade_id - g_starting_cascade))));
                FfxBrixelizerGIEmitRadiance(brick_id, uvw, world_normal, radiance, weight);
            }
        }
        xi = ffxFract(xi + FfxBrixelizerGIGOLDEN_RATIO);
    }
}

#endif // FFX_BRIXELIZER_GI_PROBE_SHADING_H