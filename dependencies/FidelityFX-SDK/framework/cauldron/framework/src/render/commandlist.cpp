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

#include "render/commandlist.h"
#include "render/device.h"
#include "render/texture.h"
#include "render/uploadheap.h"

#include "core/framework.h"

namespace cauldron
{
    CommandList::CommandList(CommandQueue queueType) :
        m_QueueType(queueType)
    {
    }

    void CommandList::BeginVRSRendering(const VariableShadingRateInfo* pVrsInfo)
    {
        if (pVrsInfo->VariableShadingMode > VariableShadingMode::VariableShadingMode_None)
        {
            if (pVrsInfo->VariableShadingMode > VariableShadingMode::VariableShadingMode_Per_Draw &&
                pVrsInfo->Combiners[1] != ShadingRateCombiner::ShadingRateCombiner_Passthrough)
            {
                const GPUResource* vrsImage = pVrsInfo->pShadingRateImage->GetResource();
                Barrier barrier = Barrier::Transition(vrsImage, 
                                                      ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource, 
                                                      ResourceState::ShadingRateSource);
                ResourceBarrier(this, 1, &barrier);

                SetShadingRate(this, pVrsInfo->BaseShadingRate, pVrsInfo->Combiners, vrsImage);
            }
            else
            {
                SetShadingRate(this, pVrsInfo->BaseShadingRate, pVrsInfo->Combiners);
            }
        }
        else
        {
            ShadingRateCombiner combiner[] = {ShadingRateCombiner::ShadingRateCombiner_Passthrough, ShadingRateCombiner::ShadingRateCombiner_Passthrough};
            SetShadingRate(this, ShadingRate::ShadingRate_1X1, combiner);
        }
    }

    void CommandList::EndVRSRendering(const VariableShadingRateInfo* pVrsInfo)
    {
        if (pVrsInfo->VariableShadingMode > VariableShadingMode::VariableShadingMode_None)
        {
            if (pVrsInfo->VariableShadingMode > VariableShadingMode::VariableShadingMode_Per_Draw &&
                pVrsInfo->Combiners[1] != ShadingRateCombiner::ShadingRateCombiner_Passthrough)
            {
                Barrier barrier = Barrier::Transition(pVrsInfo->pShadingRateImage->GetResource(),
                                                      ResourceState::ShadingRateSource,
                                                      ResourceState::NonPixelShaderResource | ResourceState::PixelShaderResource);
                ResourceBarrier(this, 1, &barrier);
            }

            // reset the combiners since VRS needs to be disabled
            ShadingRateCombiner combiners[2] = {ShadingRateCombiner::ShadingRateCombiner_Passthrough, ShadingRateCombiner::ShadingRateCombiner_Passthrough};
            SetShadingRate(this, ShadingRate::ShadingRate_1X1, combiners);
        }
    }

    UploadContext::~UploadContext()
    {
        for (auto t : m_TransferInfos)
        {
            GetUploadHeap()->EndResourceTransfer(t);
        }
    }
}
