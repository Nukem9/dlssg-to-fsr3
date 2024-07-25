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

#ifndef FFX_BRIXELIZER_GI_HOST_INTERFACE_H
#define FFX_BRIXELIZER_GI_HOST_INTERFACE_H

#include "../ffx_core.h"

#define FFX_BRIXELIZER_GI_INVALID_ID 0x00FFFFFFu

struct FfxBrixelizerGITracingConstants
{
    FfxUInt32 start_cascade;
    FfxUInt32 end_cascade;
    FfxUInt32 debug_state;
    FfxUInt32 debug_traversal_state;

    FfxFloat32 ray_pushoff;
    FfxFloat32 sdf_solve_eps;
    FfxFloat32 specular_ray_pushoff;
    FfxFloat32 specular_sdf_solve_eps;

    FfxFloat32 preview_ray_pushoff;
    FfxFloat32 preview_sdf_solve_eps;
    FfxFloat32 t_min;
    FfxFloat32 t_max;
};

struct FfxBrixelizerGIConstants
{
    FfxFloat32x4x4 view;
    FfxFloat32x4x4 view_proj;
    FfxFloat32x4x4 inv_view;
    FfxFloat32x4x4 inv_proj;
    FfxFloat32x4x4 inv_view_proj;
    FfxFloat32x4x4 prev_view_proj;
    FfxFloat32x4x4 prev_inv_view;
    FfxFloat32x4x4 prev_inv_proj;

    FfxUInt32   target_height;
    FfxUInt32   target_width;
    FfxFloat32  environmentMapIntensity;
    FfxUInt32   roughnessChannel;

    FfxUInt32    isRoughnessPerceptual;
    FfxFloat32   roughnessThreshold;
    FfxFloat32   normalsUnpackMul;
    FfxFloat32   normalsUnpackAdd;

    FfxFloat32x2 motionVectorScale;
    FfxUInt32    debug_type;
    FfxFloat32  _padding;

    FfxUInt32x2 buffer_dimensions;
    FfxUInt32x2 probe_buffer_dimensions;

    FfxFloat32x2 buffer_dimensions_f32;
    FfxFloat32x2 ibuffer_dimensions;

    FfxFloat32x2 probe_buffer_dimensions_f32;
    FfxFloat32x2 iprobe_buffer_dimensions;

    FfxUInt32x2  tile_buffer_dimensions;
    FfxFloat32x2 tile_buffer_dimensions_f32;

    FfxUInt32x2  brick_tile_buffer_dimensions;
    FfxFloat32x2 brick_tile_buffer_dimensions_f32;

    FfxFloat32x3 camera_position;
    FfxUInt32    frame_index;

    FfxBrixelizerGITracingConstants tracing_constants;
};

struct FfxBrixelizerGIPassConstants 
{
    FfxUInt32   cascade_idx;
    FfxFloat32  energy_decay_k;
    FfxUInt32x2 _padding;
};

struct FfxBrixelizerGIScalingConstants
{
    FfxUInt32x2 sourceSize;
    FfxUInt32x2 downsampledSize;
    FfxUInt32   roughnessChannel;
    FfxUInt32x3 _padding;
};

#endif // FFX_BRIXELIZER_GI_HOST_INTERFACE_H
