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

#define FFX_WAVE 1

#define SSSR_BIND_SRV_INPUT_NORMAL                  0
#define SSSR_BIND_SRV_INPUT_MATERIAL_PARAMETERS     1
#define SSSR_BIND_SRV_INPUT_ENVIRONMENT_MAP         2
#define SSSR_BIND_SRV_DEPTH_HIERARCHY               3
#define SSSR_BIND_SRV_VARIANCE                      4

#define SSSR_BIND_UAV_RADIANCE                        0
#define SSSR_BIND_UAV_RAY_LIST                        1
#define SSSR_BIND_UAV_DENOISER_TILE_LIST              2
#define SSSR_BIND_UAV_RAY_COUNTER                     3
#define SSSR_BIND_UAV_EXTRACTED_ROUGHNESS             4

#define SSSR_BIND_CB_SSSR   0

#include "sssr/ffx_sssr_callbacks_hlsl.h"
#include "sssr/ffx_sssr_classify_tiles.h"

#ifndef FFX_SSSR_THREAD_GROUP_WIDTH
#define FFX_SSSR_THREAD_GROUP_WIDTH 64
#endif // #ifndef FFX_SSSR_THREAD_GROUP_WIDTH
#ifndef FFX_SSSR_THREAD_GROUP_HEIGHT
#define FFX_SSSR_THREAD_GROUP_HEIGHT 1
#endif // FFX_SSSR_THREAD_GROUP_HEIGHT
#ifndef FFX_SSSR_THREAD_GROUP_DEPTH
#define FFX_SSSR_THREAD_GROUP_DEPTH 1
#endif // #ifndef FFX_SSSR_THREAD_GROUP_DEPTH
#ifndef FFX_SSSR_NUM_THREADS
#define FFX_SSSR_NUM_THREADS [numthreads(FFX_SSSR_THREAD_GROUP_WIDTH, FFX_SSSR_THREAD_GROUP_HEIGHT, FFX_SSSR_THREAD_GROUP_DEPTH)]
#endif // #ifndef FFX_SSSR_NUM_THREADS

FFX_PREFER_WAVE64
FFX_SSSR_NUM_THREADS
FFX_SSSR_EMBED_ROOTSIG_CONTENT
void CS(FfxUInt32 LocalThreadIndex : SV_GroupIndex, FfxUInt32x3 WorkGroupId : SV_GroupID)
{
    FfxUInt32x2 group_thread_id = ffxRemapForWaveReduction(LocalThreadIndex);
    FfxUInt32x2 dispatch_thread_id = WorkGroupId.xy * 8 + group_thread_id;
    FfxFloat32 roughness = LoadRoughnessFromMaterialParametersInput(FfxInt32x3(dispatch_thread_id, 0));

    ClassifyTiles(dispatch_thread_id, group_thread_id, roughness);

    // Extract only the channel containing the roughness to avoid loading all 4 channels in the follow up passes.
    StoreExtractedRoughness(dispatch_thread_id, roughness);
}
