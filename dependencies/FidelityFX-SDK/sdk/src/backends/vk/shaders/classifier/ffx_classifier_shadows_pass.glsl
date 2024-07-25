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

// Classifier pass
// CB   0 : cbClassifier
// SRV  0 : CLASSIFIER_Depth          : r_input_depth
// SRV  1 : CLASIFIER_Normals         : r_input_normal
// UAV  0 : CLASIFIER_Tiles           : rwsb_tiles
// UAV  1 : CLASIFIER_Tile_Count      : rwb_tileCount
// UAV  2 : CLASIFIER_RayHit          : rwt2d_rayHitResults

#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_nonuniform_qualifier : enable

#define FFX_WAVE 1
#extension GL_KHR_shader_subgroup_vote : require

#define FFX_CLASSIFIER_BIND_SRV_INPUT_DEPTH                0
#define FFX_CLASSIFIER_BIND_SRV_INPUT_NORMALS              1
#define FFX_CLASSIFIER_BIND_SRV_INPUT_SHADOW_MAPS          2

#define FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES          2000
#define FFX_CLASSIFIER_BIND_UAV_OUTPUT_WORK_TILES_COUNT    2001
#define FFX_CLASSIFIER_BIND_UAV_OUTPUT_RAY_HIT             2002

#define FFX_CLASSIFIER_BIND_CB_CLASSIFIER                  3000

#include "classifier/ffx_classifier_shadows_callbacks_glsl.h"
#include "classifier/ffx_classifier_shadows.h"

#ifndef FFX_CLASSIFIER_THREAD_GROUP_WIDTH
#define FFX_CLASSIFIER_THREAD_GROUP_WIDTH 8 * 4
#endif // #ifndef FFX_CLASSIFIER_THREAD_GROUP_WIDTH
#ifndef FFX_CLASSIFIER_THREAD_GROUP_HEIGHT
#define FFX_CLASSIFIER_THREAD_GROUP_HEIGHT 1
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
    FfxClassifyShadows(gl_LocalInvocationIndex, gl_WorkGroupID);
}
