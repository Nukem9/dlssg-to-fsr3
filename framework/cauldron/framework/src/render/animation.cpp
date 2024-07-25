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

#include "render/animation.h"

namespace cauldron
{
    const void* GetInterpolant(const AnimInterpolants* pInterpolant, int32_t index)
    {
        if (index >= pInterpolant->Count)
            index = pInterpolant->Count - 1;

        return (const char*)pInterpolant->Data.data() + pInterpolant->Stride * index;
    }

    int FindClosestInterpolant(const AnimInterpolants* pInterpolant, float value)
    {
        int32_t iter = 0;
        int32_t end  = pInterpolant->Count - 1;

        while (iter <= end)
        {
            int32_t mid         = (iter + end) / 2;
            float   interpolant = *(const float*)GetInterpolant(pInterpolant, mid);

            if (value < interpolant)
                end = mid - 1;
            else if (value > interpolant)
                iter = mid + 1;
            else
                return mid;
        }

        return end;
    }

    void AnimChannel::SampleLinear(const AnimSampler& sampler, float time, float* frac, float** pCurr, float** pNext) const
    {
        int curr_index = FindClosestInterpolant(&sampler.m_Time, time);
        int next_index = std::min<int>(curr_index + 1, sampler.m_Time.Count - 1);

        if (curr_index < 0)
            curr_index++;

        if (curr_index == next_index)
        {
            *frac = 0;
            *pCurr = (float*)GetInterpolant(&sampler.m_Value, curr_index);
            *pNext = (float*)GetInterpolant(&sampler.m_Value, next_index);
            return;
        }

        float curr_time = *(float*)GetInterpolant(&sampler.m_Time, curr_index);
        float next_time = *(float*)GetInterpolant(&sampler.m_Time, next_index);

        *pCurr = (float*)GetInterpolant(&sampler.m_Value, curr_index);
        *pNext = (float*)GetInterpolant(&sampler.m_Value, next_index);
        *frac = std::max(0.f, (time - curr_time) / (next_time - curr_time));
        CauldronAssert(ASSERT_CRITICAL, *frac >= 0 && *frac <= 1.0, L"Animation data out of bounds");
    }
}
