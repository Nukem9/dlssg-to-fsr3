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

#pragma once

#include <FidelityFX/host/ffx_parallelsort.h>
#include "../src/components/parallelsort/ffx_parallelsort_private.h"

namespace cauldron
{
    class CommandList;
    class ParameterSet;
    class PipelineObject;
    class RootSignature;
    class Buffer;
    class IndirectWorkload;

}  // namespace cauldron

union NumKeys
{
    uint32_t numKeys;
    const cauldron::Buffer* pNumKeysBuffer;
};

class ParallelSort
{
public:
    ParallelSort() {}
    ~ParallelSort();

    void Init(uint32_t maxEntries, bool bHasPayload, bool bIndirect);

    void Execute(cauldron::CommandList* pCmdList, NumKeys numKeysToSort, const cauldron::Buffer* pKeyBuffer, const cauldron::Buffer* pPayloadBuffer = nullptr);

private:
    uint32_t m_maxEntries  = 0;
    bool     m_bHasPayload = false;
    bool     m_bIndirect   = false;

    cauldron::RootSignature* m_pSetupIndirectArgsRootSignature                             = nullptr;
    cauldron::RootSignature* m_pCountRootSignature[FFX_PARALLELSORT_ITERATION_COUNT]       = {};
    cauldron::RootSignature* m_pCountReduceRootSignature[FFX_PARALLELSORT_ITERATION_COUNT] = {};
    cauldron::RootSignature* m_pScanRootSignature[FFX_PARALLELSORT_ITERATION_COUNT]        = {};
    cauldron::RootSignature* m_pScanAddRootSignature[FFX_PARALLELSORT_ITERATION_COUNT]     = {};
    cauldron::RootSignature* m_pScatterRootSignature[FFX_PARALLELSORT_ITERATION_COUNT]     = {};

    cauldron::PipelineObject* m_pSetupIndirectArgsPipelineObj                             = nullptr;
    cauldron::PipelineObject* m_pCountPipelineObj[FFX_PARALLELSORT_ITERATION_COUNT]       = {};
    cauldron::PipelineObject* m_pCountReducePipelineObj[FFX_PARALLELSORT_ITERATION_COUNT] = {};
    cauldron::PipelineObject* m_pScanPipelineObj[FFX_PARALLELSORT_ITERATION_COUNT]        = {};
    cauldron::PipelineObject* m_pScanAddPipelineObj[FFX_PARALLELSORT_ITERATION_COUNT]     = {};
    cauldron::PipelineObject* m_pScatterPipelineObj[FFX_PARALLELSORT_ITERATION_COUNT]     = {};

    cauldron::ParameterSet* m_pSetupIndirectArgsParameters                             = nullptr;
    cauldron::ParameterSet* m_pCountParameters[FFX_PARALLELSORT_ITERATION_COUNT]       = {};
    cauldron::ParameterSet* m_pCountReduceParameters[FFX_PARALLELSORT_ITERATION_COUNT] = {};
    cauldron::ParameterSet* m_pScanParameters[FFX_PARALLELSORT_ITERATION_COUNT]        = {};
    cauldron::ParameterSet* m_pScanAddParameters[FFX_PARALLELSORT_ITERATION_COUNT]     = {};
    cauldron::ParameterSet* m_pScatterParameters[FFX_PARALLELSORT_ITERATION_COUNT]     = {};

    const cauldron::Buffer* m_pSortScratchBuffer              = nullptr;
    const cauldron::Buffer* m_pPayloadScratchBuffer           = nullptr;
    const cauldron::Buffer* m_pScratchBuffer                  = nullptr;
    const cauldron::Buffer* m_pReducedScratchBuffer           = nullptr;
    const cauldron::Buffer* m_pIndirectCountScatterArgsBuffer = nullptr;
    const cauldron::Buffer* m_pIndirectReduceScanArgsBuffer   = nullptr;
    const cauldron::Buffer* m_pIndirectConstantBuffer         = nullptr;

    cauldron::IndirectWorkload* m_pIndirectWorkload = nullptr;
};
