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

void PrepareIndirectArgs() {
    { // Prepare intersection args
        FfxUInt32 ray_count = FFX_SSSR_GetRayCounter(0);

        FFX_SSSR_WriteIntersectIndirectArgs(0, (ray_count + 63) / 64);
        FFX_SSSR_WriteIntersectIndirectArgs(1, 1);
        FFX_SSSR_WriteIntersectIndirectArgs(2, 1);

        FFX_SSSR_WriteRayCounter(0, 0);
        FFX_SSSR_WriteRayCounter(1, ray_count);
    }
    { // Prepare denoiser args
        FfxUInt32 tile_count = FFX_SSSR_GetRayCounter(2);

        FFX_SSSR_WriteIntersectIndirectArgs(3, tile_count);
        FFX_SSSR_WriteIntersectIndirectArgs(4, 1);
        FFX_SSSR_WriteIntersectIndirectArgs(5, 1);

        FFX_SSSR_WriteRayCounter(2, 0);
        FFX_SSSR_WriteRayCounter(3, tile_count);
    }
}
