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

#include "parallelsort.h"

#include "core/framework.h"
#include "render/rootsignature.h"
#include "render/dynamicresourcepool.h"
#include "render/pipelineobject.h"
#include "render/parameterset.h"
#include "render/indirectworkload.h"

#include "shaders/parallelsort_common_ffx.h"

using namespace cauldron;

void ParallelSort::Init(uint32_t maxEntries, bool bHasPayload, bool bIndirect)
{
    m_maxEntries  = maxEntries;
    m_bHasPayload = bHasPayload;
    m_bIndirect   = bIndirect;

    uint32_t scratchBufferSize;
    uint32_t reducedScratchBufferSize;
    ffxParallelSortCalculateScratchResourceSize(m_maxEntries, scratchBufferSize, reducedScratchBufferSize);

    BufferDesc bufferDescSortScratchBuffer =
        BufferDesc::Data(L"ParallelSort_SortScratchBuffer", sizeof(uint32_t) * m_maxEntries, sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pSortScratchBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescSortScratchBuffer, ResourceState::UnorderedAccess);

    if (m_bHasPayload)
    {
        BufferDesc bufferDescPayloadScratchBuffer =
            BufferDesc::Data(L"ParallelSort_PayloadScratchBuffer", sizeof(uint32_t) * m_maxEntries, sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
        m_pPayloadScratchBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescPayloadScratchBuffer, ResourceState::UnorderedAccess);
    }

    BufferDesc bufferDescScratchBuffer =
        BufferDesc::Data(L"ParallelSort_ScratchBuffer", scratchBufferSize, sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pScratchBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescScratchBuffer, ResourceState::UnorderedAccess);

    BufferDesc bufferDescReducedScratchBuffer =
        BufferDesc::Data(L"ParallelSort_ReducedScratchBuffer", reducedScratchBufferSize, sizeof(uint32_t), 0, ResourceFlags::AllowUnorderedAccess);
    m_pReducedScratchBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescReducedScratchBuffer, ResourceState::UnorderedAccess);

    if (m_bIndirect)
    {
        BufferDesc bufferDescIndirectCountScatterArgsBuffer =
            BufferDesc::Data(L"ParallelSort_IndirectCountScatterArgsBuffer",
                             sizeof(uint32_t) * 3,
                             sizeof(uint32_t),
                             0,
                             static_cast<ResourceFlags>(ResourceFlags::AllowUnorderedAccess | ResourceFlags::AllowIndirect));
        m_pIndirectCountScatterArgsBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescIndirectCountScatterArgsBuffer, ResourceState::UnorderedAccess);

        BufferDesc bufferDescIndirectReduceScanArgsBuffer =
            BufferDesc::Data(L"ParallelSort_IndirectReduceScanArgsBuffer",
                             sizeof(uint32_t) * 3,
                             sizeof(uint32_t),
                             0,
                             static_cast<ResourceFlags>(ResourceFlags::AllowUnorderedAccess | ResourceFlags::AllowIndirect));
        m_pIndirectReduceScanArgsBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescIndirectReduceScanArgsBuffer, ResourceState::UnorderedAccess);

        BufferDesc bufferDescIndirectConstantBuffer = BufferDesc::Data(
            L"ParallelSort_IndirectConstantBuffer", sizeof(FfxParallelSortConstants), sizeof(FfxParallelSortConstants), 0, ResourceFlags::AllowUnorderedAccess | ResourceFlags::AllowConstantBuffer);
        m_pIndirectConstantBuffer = GetDynamicResourcePool()->CreateBuffer(&bufferDescIndirectConstantBuffer, ResourceState::UnorderedAccess);
    }

    // Sort - SetupIndirectArgs Pass
    if (m_bIndirect)
    {
        // root signature
        RootSignatureDesc signatureDesc;
        signatureDesc.AddBufferSRVSet(0, ShaderBindStage::Compute, 1);

        signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
        signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);
        signatureDesc.AddBufferUAVSet(2, ShaderBindStage::Compute, 1);

        signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);

        m_pSetupIndirectArgsRootSignature = RootSignature::CreateRootSignature(L"ParallelSort_RootSignature_SetupIndirectArgs", signatureDesc);

        // Setup the pipeline object
        PipelineDesc psoDesc;
        psoDesc.SetRootSignature(m_pSetupIndirectArgsRootSignature);

        DefineList defineList;

        // Setup the shaders to build on the pipeline object
        std::wstring    shaderPath  = L"parallelsort_setup_indirect_args.hlsl";
        ShaderBuildDesc shaderDesc  = ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS", ShaderModel::SM6_2, &defineList);
        shaderDesc.AdditionalParams = L"-Wno-for-redefinition -Wno-ambig-lit-shift";
        psoDesc.AddShaderDesc(shaderDesc);

        m_pSetupIndirectArgsPipelineObj = PipelineObject::CreatePipelineObject(L"ParallelSort_SetupIndirectArgs_PipelineObj", psoDesc);

        m_pSetupIndirectArgsParameters = ParameterSet::CreateParameterSet(m_pSetupIndirectArgsRootSignature);
        m_pSetupIndirectArgsParameters->SetRootConstantBufferResource(GetDynamicBufferPool()->GetResource(), sizeof(SetupIndirectCB), 0);
    }

    for (uint32_t i = 0; i < FFX_PARALLELSORT_ITERATION_COUNT; ++i)
    {
        // Sort - Sum Pass
        {
            // root signature
            RootSignatureDesc signatureDesc;
            signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
            signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);

            signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);

            m_pCountRootSignature[i] = RootSignature::CreateRootSignature((L"ParallelSort_RootSignature_Sum_" + std::to_wstring(i)).c_str(), signatureDesc);

            // Setup the pipeline object
            PipelineDesc psoDesc;
            psoDesc.SetRootSignature(m_pCountRootSignature[i]);

            DefineList defineList;
            if (m_bHasPayload)
                defineList.insert(std::make_pair(L"FFX_PARALLELSORT_OPTION_HAS_PAYLOAD", L"1"));

            // Setup the shaders to build on the pipeline object
            std::wstring    shaderPath  = L"parallelsort_sum_pass.hlsl";
            ShaderBuildDesc shaderDesc  = ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS", ShaderModel::SM6_2, &defineList);
            shaderDesc.AdditionalParams = L"-Wno-for-redefinition -Wno-ambig-lit-shift";
            psoDesc.AddShaderDesc(shaderDesc);

            m_pCountPipelineObj[i] = PipelineObject::CreatePipelineObject((L"ParallelSort_Sum_PipelineObj_" + std::to_wstring(i)).c_str(), psoDesc);

            m_pCountParameters[i] = ParameterSet::CreateParameterSet(m_pCountRootSignature[i]);
            m_pCountParameters[i]->SetRootConstantBufferResource(
                m_bIndirect ? m_pIndirectConstantBuffer->GetResource() : GetDynamicBufferPool()->GetResource(), sizeof(ParallelSortConstants), 0);
        }

        // Sort - Reduce Pass
        {
            // root signature
            RootSignatureDesc signatureDesc;
            signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
            signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);

            signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);

            m_pCountReduceRootSignature[i] =
                RootSignature::CreateRootSignature((L"ParallelSort_RootSignature_Reduce_" + std::to_wstring(i)).c_str(), signatureDesc);

            // Setup the pipeline object
            PipelineDesc psoDesc;
            psoDesc.SetRootSignature(m_pCountReduceRootSignature[i]);

            DefineList defineList;
            if (m_bHasPayload)
                defineList.insert(std::make_pair(L"FFX_PARALLELSORT_OPTION_HAS_PAYLOAD", L"1"));

            // Setup the shaders to build on the pipeline object
            std::wstring    shaderPath  = L"parallelsort_reduce_pass.hlsl";
            ShaderBuildDesc shaderDesc  = ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS", ShaderModel::SM6_2, &defineList);
            shaderDesc.AdditionalParams = L"-Wno-for-redefinition -Wno-ambig-lit-shift";
            psoDesc.AddShaderDesc(shaderDesc);

            m_pCountReducePipelineObj[i] = PipelineObject::CreatePipelineObject((L"ParallelSort_Reduce_PipelineObj_" + std::to_wstring(i)).c_str(), psoDesc);

            m_pCountReduceParameters[i] = ParameterSet::CreateParameterSet(m_pCountReduceRootSignature[i]);
            m_pCountReduceParameters[i]->SetRootConstantBufferResource(
                m_bIndirect ? m_pIndirectConstantBuffer->GetResource() : GetDynamicBufferPool()->GetResource(), sizeof(ParallelSortConstants), 0);
        }

        // Sort - Scan
        {
            // root signature
            RootSignatureDesc signatureDesc;
            signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
            signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);

            signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);

            m_pScanRootSignature[i] = RootSignature::CreateRootSignature((L"ParallelSort_RootSignature_Scan_" + std::to_wstring(i)).c_str(), signatureDesc);

            // Setup the pipeline object
            PipelineDesc psoDesc;
            psoDesc.SetRootSignature(m_pScanRootSignature[i]);

            DefineList defineList;
            if (m_bHasPayload)
                defineList.insert(std::make_pair(L"FFX_PARALLELSORT_OPTION_HAS_PAYLOAD", L"1"));

            // Setup the shaders to build on the pipeline object
            std::wstring    shaderPath  = L"parallelsort_scan_pass.hlsl";
            ShaderBuildDesc shaderDesc  = ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS", ShaderModel::SM6_2, &defineList);
            shaderDesc.AdditionalParams = L"-Wno-for-redefinition -Wno-ambig-lit-shift";
            psoDesc.AddShaderDesc(shaderDesc);

            m_pScanPipelineObj[i] = PipelineObject::CreatePipelineObject((L"ParallelSort_Scan_PipelineObj_" + std::to_wstring(i)).c_str(), psoDesc);

            m_pScanParameters[i] = ParameterSet::CreateParameterSet(m_pScanRootSignature[i]);
            m_pScanParameters[i]->SetRootConstantBufferResource(
                m_bIndirect ? m_pIndirectConstantBuffer->GetResource() : GetDynamicBufferPool()->GetResource(), sizeof(ParallelSortConstants), 0);
        }

        // Sort - Scan Add
        {
            // root signature
            RootSignatureDesc signatureDesc;
            signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
            signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);
            signatureDesc.AddBufferUAVSet(2, ShaderBindStage::Compute, 1);

            signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);

            m_pScanAddRootSignature[i] =
                RootSignature::CreateRootSignature((L"ParallelSort_RootSignature_ScanAdd_" + std::to_wstring(i)).c_str(), signatureDesc);

            // Setup the pipeline object
            PipelineDesc psoDesc;
            psoDesc.SetRootSignature(m_pScanAddRootSignature[i]);

            DefineList defineList;
            if (m_bHasPayload)
                defineList.insert(std::make_pair(L"FFX_PARALLELSORT_OPTION_HAS_PAYLOAD", L"1"));

            // Setup the shaders to build on the pipeline object
            std::wstring    shaderPath  = L"parallelsort_scan_add_pass.hlsl";
            ShaderBuildDesc shaderDesc  = ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS", ShaderModel::SM6_2, &defineList);
            shaderDesc.AdditionalParams = L"-Wno-for-redefinition -Wno-ambig-lit-shift";
            psoDesc.AddShaderDesc(shaderDesc);

            m_pScanAddPipelineObj[i] = PipelineObject::CreatePipelineObject((L"ParallelSort_ScanAdd_PipelineObj_" + std::to_wstring(i)).c_str(), psoDesc);

            m_pScanAddParameters[i] = ParameterSet::CreateParameterSet(m_pScanAddRootSignature[i]);
            m_pScanAddParameters[i]->SetRootConstantBufferResource(
                m_bIndirect ? m_pIndirectConstantBuffer->GetResource() : GetDynamicBufferPool()->GetResource(), sizeof(ParallelSortConstants), 0);
        }

        // Sort - Scatter
        {
            // root signature
            RootSignatureDesc signatureDesc;
            signatureDesc.AddBufferUAVSet(0, ShaderBindStage::Compute, 1);
            signatureDesc.AddBufferUAVSet(1, ShaderBindStage::Compute, 1);
            signatureDesc.AddBufferUAVSet(2, ShaderBindStage::Compute, 1);
            if (m_bHasPayload)
            {
                signatureDesc.AddBufferUAVSet(3, ShaderBindStage::Compute, 1);
                signatureDesc.AddBufferUAVSet(4, ShaderBindStage::Compute, 1);
            }

            signatureDesc.AddConstantBufferView(0, ShaderBindStage::Compute, 1);

            m_pScatterRootSignature[i] =
                RootSignature::CreateRootSignature((L"ParallelSort_RootSignature_Scatter_" + std::to_wstring(i)).c_str(), signatureDesc);

            // Setup the pipeline object
            PipelineDesc psoDesc;
            psoDesc.SetRootSignature(m_pScatterRootSignature[i]);

            DefineList defineList;
            if (m_bHasPayload)
                defineList.insert(std::make_pair(L"FFX_PARALLELSORT_OPTION_HAS_PAYLOAD", L"1"));

            // Setup the shaders to build on the pipeline object
            std::wstring    shaderPath  = L"parallelsort_scatter_pass.hlsl";
            ShaderBuildDesc shaderDesc  = ShaderBuildDesc::Compute(shaderPath.c_str(), L"CS", ShaderModel::SM6_2, &defineList);
            shaderDesc.AdditionalParams = L"-Wno-for-redefinition -Wno-ambig-lit-shift";
            psoDesc.AddShaderDesc(shaderDesc);

            m_pScatterPipelineObj[i] = PipelineObject::CreatePipelineObject((L"ParallelSort_Scatter_PipelineObj_" + std::to_wstring(i)).c_str(), psoDesc);

            m_pScatterParameters[i] = ParameterSet::CreateParameterSet(m_pScatterRootSignature[i]);
            m_pScatterParameters[i]->SetRootConstantBufferResource(
                m_bIndirect ? m_pIndirectConstantBuffer->GetResource() : GetDynamicBufferPool()->GetResource(), sizeof(ParallelSortConstants), 0);
        }
    }

    if (m_bIndirect)
        m_pIndirectWorkload = IndirectWorkload::CreateIndirectWorkload(IndirectCommandType::Dispatch);
}

ParallelSort::~ParallelSort()
{
    if (m_pSetupIndirectArgsRootSignature)
        delete m_pSetupIndirectArgsRootSignature;

    for (auto& pRootSignature : m_pCountRootSignature)
    {
        if (pRootSignature)
            delete pRootSignature;
    }

    for (auto& pRootSignature : m_pCountReduceRootSignature)
    {
        if (pRootSignature)
            delete pRootSignature;
    }

    for (auto& pRootSignature : m_pScanRootSignature)
    {
        if (pRootSignature)
            delete pRootSignature;
    }

    for (auto& pRootSignature : m_pScanAddRootSignature)
    {
        if (pRootSignature)
            delete pRootSignature;
    }

    for (auto& pRootSignature : m_pScatterRootSignature)
    {
        if (pRootSignature)
            delete pRootSignature;
    }

    if (m_pSetupIndirectArgsPipelineObj)
        delete m_pSetupIndirectArgsPipelineObj;

    for (auto& pPipelineObj : m_pCountPipelineObj)
    {
        if (pPipelineObj)
            delete pPipelineObj;
    }

    for (auto& pPipelineObj : m_pCountReducePipelineObj)
    {
        if (pPipelineObj)
            delete pPipelineObj;
    }

    for (auto& pPipelineObj : m_pScanPipelineObj)
    {
        if (pPipelineObj)
            delete pPipelineObj;
    }

    for (auto& pPipelineObj : m_pScanAddPipelineObj)
    {
        if (pPipelineObj)
            delete pPipelineObj;
    }

    for (auto& pPipelineObj : m_pScatterPipelineObj)
    {
        if (pPipelineObj)
            delete pPipelineObj;
    }

    if (m_pSetupIndirectArgsParameters)
        delete m_pSetupIndirectArgsParameters;

    for (auto& pParameters : m_pCountParameters)
    {
        if (pParameters)
            delete pParameters;
    }

    for (auto& pParameters : m_pCountReduceParameters)
    {
        if (pParameters)
            delete pParameters;
    }

    for (auto& pParameters : m_pScanParameters)
    {
        if (pParameters)
            delete pParameters;
    }

    for (auto& pParameters : m_pScanAddParameters)
    {
        if (pParameters)
            delete pParameters;
    }

    for (auto& pParameters : m_pScatterParameters)
    {
        if (pParameters)
            delete pParameters;
    }

    if (m_pIndirectWorkload)
        delete m_pIndirectWorkload;
}

void ParallelSort::Execute(cauldron::CommandList* pCmdList, NumKeys numKeysToSort, const cauldron::Buffer* pKeyBuffer, const cauldron::Buffer* pPayloadBuffer)
{
    CauldronAssert(ASSERT_CRITICAL, (pPayloadBuffer != nullptr) == m_bHasPayload, L"The payload setup is incorrect.");

    const cauldron::Buffer* pSrcKeyBuffer     = pKeyBuffer;
    const cauldron::Buffer* pSrcPayloadBuffer = pPayloadBuffer;
    const cauldron::Buffer* pDstKeyBuffer     = m_pSortScratchBuffer;
    const cauldron::Buffer* pDstPayloadBuffer = m_pPayloadScratchBuffer;

    // Initialize constants for the sort job
    ParallelSortConstants constants;
    memset(&constants, 0, sizeof(ParallelSortConstants));

    uint32_t numThreadGroupsToRun;
    uint32_t numReducedThreadGroupsToRun;

    if (!m_bIndirect)
    {
        ffxParallelSortSetConstantAndDispatchData(
            numKeysToSort.numKeys, FFX_PARALLELSORT_MAX_THREADGROUPS_TO_RUN, constants, numThreadGroupsToRun, numReducedThreadGroupsToRun);
    }

    // Sort - SetupIndirectArgs Pass
    // Those can only be set once per frame
    m_pSetupIndirectArgsParameters->SetBufferSRV(numKeysToSort.pNumKeysBuffer, 0);
    m_pSetupIndirectArgsParameters->SetBufferUAV(m_pIndirectConstantBuffer, 0);
    m_pSetupIndirectArgsParameters->SetBufferUAV(m_pIndirectCountScatterArgsBuffer, 1);
    m_pSetupIndirectArgsParameters->SetBufferUAV(m_pIndirectReduceScanArgsBuffer, 2);

    // Execute the sort algorithm in 4-bit increments
    constants.shift = 0;
    for (uint32_t i = 0; constants.shift < 32; constants.shift += FFX_PARALLELSORT_SORT_BITS_PER_PASS, ++i)
    {
        BufferAddressInfo parallelSortBufferInfo;

        // Update the constant buffer
        if (m_bIndirect)
        {
            SetupIndirectCB setupIndirectCB;
            memset(&setupIndirectCB, 0, sizeof(SetupIndirectCB));
            setupIndirectCB.maxThreadGroups = FFX_PARALLELSORT_MAX_THREADGROUPS_TO_RUN;
            setupIndirectCB.shift           = constants.shift;

            BufferAddressInfo setupIndirectBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(SetupIndirectCB), &setupIndirectCB);

            // Sort - SetupIndirectArgs Pass
            m_pSetupIndirectArgsParameters->UpdateRootConstantBuffer(&setupIndirectBufferInfo, 0);
            m_pSetupIndirectArgsParameters->Bind(pCmdList, m_pSetupIndirectArgsPipelineObj);
            SetPipelineState(pCmdList, m_pSetupIndirectArgsPipelineObj);
            Dispatch(pCmdList, 1, 1, 1);

            {
                Barrier barriers[3] = {
                    Barrier::Transition(m_pIndirectCountScatterArgsBuffer->GetResource(), ResourceState::UnorderedAccess, ResourceState::IndirectArgument),
                    Barrier::Transition(m_pIndirectReduceScanArgsBuffer->GetResource(), ResourceState::UnorderedAccess, ResourceState::IndirectArgument),
                    Barrier::Transition(m_pIndirectConstantBuffer->GetResource(), ResourceState::UnorderedAccess, ResourceState::ConstantBufferResource)};
                ResourceBarrier(pCmdList, 3, barriers);
            }

            parallelSortBufferInfo = m_pIndirectConstantBuffer->GetAddressInfo();
        }
        else
        {
            parallelSortBufferInfo = GetDynamicBufferPool()->AllocConstantBuffer(sizeof(ParallelSortConstants), &constants);
        }

        {
            Barrier barriers[2] = {Barrier::UAV(pSrcKeyBuffer->GetResource()), Barrier::UAV(m_pScratchBuffer->GetResource())};
            ResourceBarrier(pCmdList, 2, barriers);
        }

        // Sort - Sum Pass
        m_pCountParameters[i]->SetBufferUAV(pSrcKeyBuffer, 0);
        m_pCountParameters[i]->SetBufferUAV(m_pScratchBuffer, 1);
        m_pCountParameters[i]->UpdateRootConstantBuffer(&parallelSortBufferInfo, 0);
        m_pCountParameters[i]->Bind(pCmdList, m_pCountPipelineObj[i]);
        SetPipelineState(pCmdList, m_pCountPipelineObj[i]);
        if (m_bIndirect)
            ExecuteIndirect(pCmdList, m_pIndirectWorkload, m_pIndirectCountScatterArgsBuffer, 1, 0);
        else
            Dispatch(pCmdList, numThreadGroupsToRun, 1, 1);

        {
            Barrier barriers[2] = {Barrier::UAV(m_pScratchBuffer->GetResource()), Barrier::UAV(m_pReducedScratchBuffer->GetResource())};
            ResourceBarrier(pCmdList, 2, barriers);
        }

        // Sort - Reduce Pass
        m_pCountReduceParameters[i]->SetBufferUAV(m_pScratchBuffer, 0);
        m_pCountReduceParameters[i]->SetBufferUAV(m_pReducedScratchBuffer, 1);
        m_pCountReduceParameters[i]->UpdateRootConstantBuffer(&parallelSortBufferInfo, 0);
        m_pCountReduceParameters[i]->Bind(pCmdList, m_pCountReducePipelineObj[i]);
        SetPipelineState(pCmdList, m_pCountReducePipelineObj[i]);
        if (m_bIndirect)
            ExecuteIndirect(pCmdList, m_pIndirectWorkload, m_pIndirectReduceScanArgsBuffer, 1, 0);
        else
            Dispatch(pCmdList, numReducedThreadGroupsToRun, 1, 1);

        {
            Barrier barriers[1] = {Barrier::UAV(m_pReducedScratchBuffer->GetResource())};
            ResourceBarrier(pCmdList, 1, barriers);
        }

        // Sort - Scan
        m_pScanParameters[i]->SetBufferUAV(m_pReducedScratchBuffer, 0);
        m_pScanParameters[i]->SetBufferUAV(m_pReducedScratchBuffer, 1);
        m_pScanParameters[i]->UpdateRootConstantBuffer(&parallelSortBufferInfo, 0);
        m_pScanParameters[i]->Bind(pCmdList, m_pScanPipelineObj[i]);
        SetPipelineState(pCmdList, m_pScanPipelineObj[i]);
        Dispatch(pCmdList, 1, 1, 1);

        {
            Barrier barriers[2] = {Barrier::UAV(m_pScratchBuffer->GetResource()), Barrier::UAV(m_pReducedScratchBuffer->GetResource())};
            ResourceBarrier(pCmdList, 2, barriers);
        }

        // Sort - Scan Add
        m_pScanAddParameters[i]->SetBufferUAV(m_pScratchBuffer, 0);
        m_pScanAddParameters[i]->SetBufferUAV(m_pScratchBuffer, 1);
        m_pScanAddParameters[i]->SetBufferUAV(m_pReducedScratchBuffer, 2);
        m_pScanAddParameters[i]->UpdateRootConstantBuffer(&parallelSortBufferInfo, 0);
        m_pScanAddParameters[i]->Bind(pCmdList, m_pScanAddPipelineObj[i]);
        SetPipelineState(pCmdList, m_pScanAddPipelineObj[i]);
        if (m_bIndirect)
            ExecuteIndirect(pCmdList, m_pIndirectWorkload, m_pIndirectReduceScanArgsBuffer, 1, 0);
        else
            Dispatch(pCmdList, numReducedThreadGroupsToRun, 1, 1);

        {
            int     numBarriers = 3;
            Barrier barriers[5] = {
                Barrier::UAV(pSrcKeyBuffer->GetResource()), Barrier::UAV(pDstKeyBuffer->GetResource()), Barrier::UAV(m_pScratchBuffer->GetResource())};
            if (m_bHasPayload)
            {
                barriers[numBarriers++] = Barrier::UAV(pSrcPayloadBuffer->GetResource());
                barriers[numBarriers++] = Barrier::UAV(pDstPayloadBuffer->GetResource());
            }
            ResourceBarrier(pCmdList, numBarriers, barriers);
        }

        // Sort - Scatter
        m_pScatterParameters[i]->SetBufferUAV(pSrcKeyBuffer, 0);
        m_pScatterParameters[i]->SetBufferUAV(pDstKeyBuffer, 1);
        m_pScatterParameters[i]->SetBufferUAV(m_pScratchBuffer, 2);
        if (m_bHasPayload)
        {
            m_pScatterParameters[i]->SetBufferUAV(pSrcPayloadBuffer, 3);
            m_pScatterParameters[i]->SetBufferUAV(pDstPayloadBuffer, 4);
        }
        m_pScatterParameters[i]->UpdateRootConstantBuffer(&parallelSortBufferInfo, 0);
        m_pScatterParameters[i]->Bind(pCmdList, m_pScatterPipelineObj[i]);
        SetPipelineState(pCmdList, m_pScatterPipelineObj[i]);
        if (m_bIndirect)
            ExecuteIndirect(pCmdList, m_pIndirectWorkload, m_pIndirectCountScatterArgsBuffer, 1, 0);
        else
            Dispatch(pCmdList, numThreadGroupsToRun, 1, 1);

        // Swap
        const cauldron::Buffer* temp = pDstKeyBuffer;
        pDstKeyBuffer                = pSrcKeyBuffer;
        pSrcKeyBuffer                = temp;

        if (m_bHasPayload)
        {
            temp              = pDstPayloadBuffer;
            pDstPayloadBuffer = pSrcPayloadBuffer;
            pSrcPayloadBuffer = temp;
        }

        if (m_bIndirect)
        {
            Barrier barriers[3] = {
                Barrier::Transition(m_pIndirectCountScatterArgsBuffer->GetResource(), ResourceState::IndirectArgument, ResourceState::UnorderedAccess),
                Barrier::Transition(m_pIndirectReduceScanArgsBuffer->GetResource(), ResourceState::IndirectArgument, ResourceState::UnorderedAccess),
                Barrier::Transition(m_pIndirectConstantBuffer->GetResource(), ResourceState::ConstantBufferResource, ResourceState::UnorderedAccess)};
            ResourceBarrier(pCmdList, 3, barriers);
        }
    }
}
