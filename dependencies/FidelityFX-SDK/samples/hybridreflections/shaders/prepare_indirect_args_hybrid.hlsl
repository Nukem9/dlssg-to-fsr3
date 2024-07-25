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

#include "declarations.h"
#include "common_types.h"

RWByteAddressBuffer g_rw_ray_counter : register(u0);
RWByteAddressBuffer g_rw_indirect_args : register(u1);

ConstantBuffer<FrameInfo> g_frame_info_cb[1] : register(b0);

[numthreads(1, 1, 1)]
void main() {
    uint sw_ray_count = g_rw_ray_counter.Load(RAY_COUNTER_SW_OFFSET);
    uint hw_ray_count = g_rw_ray_counter.Load(RAY_COUNTER_HW_OFFSET);

    g_rw_indirect_args.Store(INDIRECT_ARGS_SW_OFFSET + 0, (sw_ray_count + 31) / 32);
    g_rw_indirect_args.Store(INDIRECT_ARGS_SW_OFFSET + 4, 1);
    g_rw_indirect_args.Store(INDIRECT_ARGS_SW_OFFSET + 8, 1);

    g_rw_ray_counter.Store(RAY_COUNTER_SW_OFFSET, 0);
    g_rw_ray_counter.Store(RAY_COUNTER_SW_HISTORY_OFFSET, sw_ray_count);

    uint denoise_tile_count = g_rw_ray_counter.Load(RAY_COUNTER_DENOISE_OFFSET);

    g_rw_indirect_args.Store(INDIRECT_ARGS_DENOISE_OFFSET + 0, denoise_tile_count);
    g_rw_indirect_args.Store(INDIRECT_ARGS_DENOISE_OFFSET + 4, 1);
    g_rw_indirect_args.Store(INDIRECT_ARGS_DENOISE_OFFSET + 8, 1);

    g_rw_ray_counter.Store(RAY_COUNTER_DENOISE_OFFSET, 0);
    g_rw_ray_counter.Store(RAY_COUNTER_DENOISE_HISTORY_OFFSET, denoise_tile_count);

    if (!(g_hsr_mask & HSR_FLAGS_USE_RAY_TRACING)) {
        g_rw_ray_counter.Store(RAY_COUNTER_HW_OFFSET, 0);
        g_rw_ray_counter.Store(RAY_COUNTER_HW_OFFSET + 4, 0);
        g_rw_indirect_args.Store(INDIRECT_ARGS_HW_OFFSET + 0, 0);
        g_rw_indirect_args.Store(INDIRECT_ARGS_HW_OFFSET + 4, 0);
        g_rw_indirect_args.Store(INDIRECT_ARGS_HW_OFFSET + 8, 0);
    }
}
