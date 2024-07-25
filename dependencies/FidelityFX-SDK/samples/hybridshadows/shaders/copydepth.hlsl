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

Texture2D<float4>   gbufferDepth : register(t0);
RWTexture2D<float4> depthCopy : register(u0);

[numthreads(32, 8, 1)] void main(uint group_index
                                 : SV_GroupIndex, uint3 group_id
                                 : SV_GroupID, uint3    dispatch_thread_id
                                 : SV_DispatchThreadID) {
    float2 depth_image_size = float2(0.0f, 0.0f);
    gbufferDepth.GetDimensions(depth_image_size.x, depth_image_size.y);

    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 8; ++j)
        {
            uint2 idx = uint2(2 * dispatch_thread_id.x + i, 8 * dispatch_thread_id.y + j);
            if (idx.x < depth_image_size.x && idx.y < depth_image_size.y)
            {
                depthCopy[idx] = gbufferDepth[idx];
            }
        }
    }
}
