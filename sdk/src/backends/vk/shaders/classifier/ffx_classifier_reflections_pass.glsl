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

#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

#define FFX_WAVE 1
#extension GL_KHR_shader_subgroup_basic : require

#define CLASSIFIER_BIND_SRV_INPUT_NORMAL              0
#define CLASSIFIER_BIND_SRV_INPUT_MATERIAL_PARAMETERS 1
#define CLASSIFIER_BIND_SRV_INPUT_ENVIRONMENT_MAP     2
#define CLASSIFIER_BIND_SRV_INPUT_MOTION_VECTORS      3
#define CLASSIFIER_BIND_SRV_VARIANCE_HISTORY          6
#define CLASSIFIER_BIND_SRV_HIT_COUNTER_HISTORY       7
#define CLASSIFIER_BIND_SRV_INPUT_DEPTH               8

#define CLASSIFIER_BIND_UAV_RADIANCE            2000
#define CLASSIFIER_BIND_UAV_RAY_LIST            2001
#define CLASSIFIER_BIND_UAV_HW_RAY_LIST         2002
#define CLASSIFIER_BIND_UAV_DENOISER_TILE_LIST  2003
#define CLASSIFIER_BIND_UAV_RAY_COUNTER         2004
#define CLASSIFIER_BIND_UAV_EXTRACTED_ROUGHNESS 2005
#define CLASSIFIER_BIND_UAV_HIT_COUNTER         2007

#define CLASSIFIER_BIND_CB_CLASSIFIER   3000

#include "classifier/ffx_classifier_reflections_callbacks_glsl.h"
#include "classifier/ffx_classifier_reflections.h"

#ifndef FFX_CLASSIFIER_THREAD_GROUP_WIDTH
#define FFX_CLASSIFIER_THREAD_GROUP_WIDTH 8
#endif // #ifndef FFX_CLASSIFIER_THREAD_GROUP_WIDTH
#ifndef FFX_CLASSIFIER_THREAD_GROUP_HEIGHT
#define FFX_CLASSIFIER_THREAD_GROUP_HEIGHT 8
#endif // FFX_CLASSIFIER_THREAD_GROUP_HEIGHT
#ifndef FFX_CLASSIFIER_THREAD_GROUP_DEPTH
#define FFX_CLASSIFIER_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_CLASSIFIER_THREAD_GROUP_DEPTH
#ifndef FFX_CLASSIFIER_NUM_THREADS
#define FFX_CLASSIFIER_NUM_THREADS layout (local_size_x = FFX_CLASSIFIER_THREAD_GROUP_WIDTH, local_size_y = FFX_CLASSIFIER_THREAD_GROUP_HEIGHT, local_size_z = FFX_CLASSIFIER_THREAD_GROUP_DEPTH) in;
#endif // #ifndef FFX_CLASSIFIER_NUM_THREADS

FFX_CLASSIFIER_NUM_THREADS
void main()
{
    FfxUInt32x2 group_thread_id = ffxRemapForWaveReduction(gl_LocalInvocationIndex);
    FfxUInt32x2 dispatch_thread_id = gl_WorkGroupID.xy * 8 + group_thread_id;

    FfxInt32x2 screen_size = FfxInt32x2(ReflectionWidth(), ReflectionHeight());
    FfxFloat32x3 world_space_normal = LoadWorldSpaceNormal(FfxInt32x2(dispatch_thread_id.xy));
    FfxFloat32 depth = GetInputDepth(dispatch_thread_id.xy);
    FfxFloat32x3 view_space_surface_normal = normalize(FFX_MATRIX_MULTIPLY(ViewMatrix(), FfxFloat32x4(world_space_normal, 0)).xyz);
    FfxFloat32 roughness = LoadRoughnessFromMaterialParametersInput(FfxInt32x3(dispatch_thread_id, 0));
    FfxUInt32 mask = Mask();

    // Classify tile
    ClassifyTiles(dispatch_thread_id,
        group_thread_id,
        roughness,
        view_space_surface_normal,
        depth,
        screen_size,
        SamplesPerQuad(),
        TemporalVarianceGuidedTracingEnabled(),
        (mask & FFX_CLASSIFIER_FLAGS_USE_HIT_COUNTER) > 0,
        (mask & FFX_CLASSIFIER_FLAGS_USE_SCREEN_SPACE) > 0,
        (mask & FFX_CLASSIFIER_FLAGS_USE_RAY_TRACING) > 0);

    // Extract only the channel containing the roughness to avoid loading all 4 channels in the follow up passes.
    StoreExtractedRoughness(dispatch_thread_id, roughness);
}
