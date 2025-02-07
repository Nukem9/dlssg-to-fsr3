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

#include "d3d12.h"
#include "FrameInterpolationSwapchainDX12.h"
#include "FrameInterpolationSwapchainDX12_Helpers.h"
#include "FrameInterpolationSwapchainDX12_UiComposition.h"

namespace UiCompositionShaders
{
#include "FrameInterpolationSwapchainUiCompositionVS.h"
#include "FrameInterpolationSwapchainUiCompositionPS.h"
}  // namespace UiCompositionShaders;

namespace UiCompositionPremulShaders
{
#include "FrameInterpolationSwapchainUiCompositionPremulVS.h"
#include "FrameInterpolationSwapchainUiCompositionPremulPS.h"
}  // namespace UiCompositionPremulShaders;

typedef HRESULT(__stdcall* D3D12SerializeVersionedRootSignatureType)(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature,
                                                                     ID3DBlob**                                 ppBlob,
                                                                     ID3DBlob**                                 ppErrorBlob);

const uint32_t          s_uiCompositionDescRingBufferSize = FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT * 2 * 2;   // FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT real frames (i.e. * 2), 2 SRV each should be enough
const uint32_t          s_uiCompositionDescHeapRtvSize = FFX_FRAME_INTERPOLATION_SWAP_CHAIN_MAX_BUFFER_COUNT * 2;
ID3D12RootSignature*    s_uiCompositionRootSignature;
ID3D12PipelineState*    s_uiCompositionPipeline;
ID3D12PipelineState*    s_uiCompositionPremulPipeline;
uint32_t                s_uiCompositionDescRingBufferBase;
ID3D12DescriptorHeap*   s_uiCompositionDescRingBuffer;
uint32_t                s_uiCompositionNextRtvDescriptor;
ID3D12DescriptorHeap*   s_uiCompositionDescHeapRtvCpu;

// create the pipeline state to use for UI composition
// pretty similar to FfxCreatePipelineFunc
FfxErrorCodes CreateUiCompositionPipeline(ID3D12Device* dx12Device, DXGI_FORMAT fmt)
{
    D3D12_DESCRIPTOR_RANGE1   range;
    range.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors                    = 2;
    range.BaseShaderRegister                = 0;
    range.RegisterSpace                     = 0;
    range.Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER1 rootParameters;
    rootParameters.ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters.ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters.DescriptorTable.NumDescriptorRanges = 1;
    rootParameters.DescriptorTable.pDescriptorRanges   = &range;

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Version                    = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSignatureDesc.Desc_1_1.NumParameters     = 1;
    rootSignatureDesc.Desc_1_1.pParameters       = &rootParameters;
    rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
    rootSignatureDesc.Desc_1_1.pStaticSamplers   = nullptr;
    rootSignatureDesc.Desc_1_1.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3D12RootSignature*                     dx12RootSignature;
    ID3DBlob*                                signature = nullptr;
    ID3DBlob*                                error = nullptr;
    HMODULE                                  d3d12ModuleHandle                        = GetModuleHandleW(L"D3D12.dll");
    if (NULL != d3d12ModuleHandle)
    {
        D3D12SerializeVersionedRootSignatureType d3d12SerializeVersionedRootSignatureFunc =
            (D3D12SerializeVersionedRootSignatureType)GetProcAddress(d3d12ModuleHandle, "D3D12SerializeVersionedRootSignature");

        if (nullptr != d3d12SerializeVersionedRootSignatureFunc)
        {
            HRESULT result = d3d12SerializeVersionedRootSignatureFunc(&rootSignatureDesc, &signature, &error);
            if (FAILED(result))
            {
                return FFX_ERROR_BACKEND_API_ERROR;
            }

            result = dx12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&dx12RootSignature));
            SafeRelease(signature);
            SafeRelease(error);

            if (FAILED(result))
            {
                return FFX_ERROR_BACKEND_API_ERROR;
            }
        }
        else
        {
            return FFX_ERROR_BACKEND_API_ERROR;
        }
    }
    else
    {
        return FFX_ERROR_BACKEND_API_ERROR;
    }

    // create the PSO
    D3D12_RASTERIZER_DESC rasterDesc;
    rasterDesc.FillMode                                                        = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode                                                        = D3D12_CULL_MODE_BACK;
    rasterDesc.FrontCounterClockwise                                           = FALSE;
    rasterDesc.DepthBias                                                       = D3D12_DEFAULT_DEPTH_BIAS;
    rasterDesc.DepthBiasClamp                                                  = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterDesc.SlopeScaledDepthBias                                            = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterDesc.DepthClipEnable                                                 = TRUE;
    rasterDesc.MultisampleEnable                                               = FALSE;
    rasterDesc.AntialiasedLineEnable                                           = FALSE;
    rasterDesc.ForcedSampleCount                                               = 0;
    rasterDesc.ConservativeRaster                                              = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blendDesc;
    blendDesc.AlphaToCoverageEnable                                    = FALSE;
    blendDesc.IndependentBlendEnable                                   = FALSE;
    const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc = {
        FALSE,
        FALSE,
        D3D12_BLEND_ONE,
        D3D12_BLEND_ZERO,
        D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE,
        D3D12_BLEND_ZERO,
        D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;

    D3D12_DEPTH_STENCIL_DESC depthStencilDesc;
    depthStencilDesc.DepthEnable                                       = TRUE;
    depthStencilDesc.DepthWriteMask                                    = D3D12_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc                                         = D3D12_COMPARISON_FUNC_LESS;
    depthStencilDesc.StencilEnable                                     = FALSE;
    depthStencilDesc.StencilReadMask                                   = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencilDesc.StencilWriteMask                                  = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
    depthStencilDesc.FrontFace                        = defaultStencilOp;
    depthStencilDesc.BackFace                         = defaultStencilOp;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC dx12PipelineStateDescription = {};
    dx12PipelineStateDescription.RasterizerState                    = rasterDesc;
    dx12PipelineStateDescription.BlendState                         = blendDesc;
    dx12PipelineStateDescription.DepthStencilState                  = depthStencilDesc;
    dx12PipelineStateDescription.DepthStencilState.DepthEnable      = FALSE;
    dx12PipelineStateDescription.SampleMask                         = UINT_MAX;
    dx12PipelineStateDescription.PrimitiveTopologyType              = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    dx12PipelineStateDescription.SampleDesc                         = {1, 0};
    dx12PipelineStateDescription.NumRenderTargets                   = 1;
    dx12PipelineStateDescription.RTVFormats[0]                      = fmt;

    dx12PipelineStateDescription.Flags              = D3D12_PIPELINE_STATE_FLAG_NONE;
    dx12PipelineStateDescription.pRootSignature     = dx12RootSignature;

    s_uiCompositionRootSignature = dx12RootSignature;

    {
        dx12PipelineStateDescription.VS.pShaderBytecode = UiCompositionShaders::g_mainVS;
        dx12PipelineStateDescription.VS.BytecodeLength  = sizeof(UiCompositionShaders::g_mainVS);
        dx12PipelineStateDescription.PS.pShaderBytecode = UiCompositionShaders::g_mainPS;
        dx12PipelineStateDescription.PS.BytecodeLength  = sizeof(UiCompositionShaders::g_mainPS);

        if (FAILED(dx12Device->CreateGraphicsPipelineState(&dx12PipelineStateDescription,
                                                           IID_PPV_ARGS(reinterpret_cast<ID3D12PipelineState**>(&s_uiCompositionPipeline)))))
            return FFX_ERROR_BACKEND_API_ERROR;
    }

    {
        dx12PipelineStateDescription.VS.pShaderBytecode = UiCompositionPremulShaders::g_mainVS;
        dx12PipelineStateDescription.VS.BytecodeLength  = sizeof(UiCompositionPremulShaders::g_mainVS);
        dx12PipelineStateDescription.PS.pShaderBytecode = UiCompositionPremulShaders::g_mainPS;
        dx12PipelineStateDescription.PS.BytecodeLength  = sizeof(UiCompositionPremulShaders::g_mainPS);

        if (FAILED(dx12Device->CreateGraphicsPipelineState(&dx12PipelineStateDescription,
                                                           IID_PPV_ARGS(reinterpret_cast<ID3D12PipelineState**>(&s_uiCompositionPremulPipeline)))))
            return FFX_ERROR_BACKEND_API_ERROR;
    }

    return FFX_OK;
}

FfxErrorCodes verifyUiBlitGpuResources(ID3D12Device* dx12Device, DXGI_FORMAT fmt)
{
    FFX_ASSERT(nullptr != dx12Device);

    if (nullptr == s_uiCompositionPipeline || nullptr == s_uiCompositionPremulPipeline)
    {
        FfxErrorCodes res = CreateUiCompositionPipeline(dx12Device, fmt);
        if (res != FFX_OK)
            return res;
    }

    if (nullptr == s_uiCompositionDescRingBuffer)
    {
        D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
        descHeap.NumDescriptors             = s_uiCompositionDescRingBufferSize;
        descHeap.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descHeap.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descHeap.NodeMask                   = 0;
        s_uiCompositionDescRingBufferBase   = 0;
        if(FAILED(dx12Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&s_uiCompositionDescRingBuffer))))
            return FFX_ERROR_BACKEND_API_ERROR;
    }

    if (nullptr == s_uiCompositionDescHeapRtvCpu)
    {
        D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
        descHeap.NumDescriptors             = s_uiCompositionDescHeapRtvSize;
        descHeap.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descHeap.NodeMask                   = 0;
        descHeap.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        if(FAILED(dx12Device->CreateDescriptorHeap(&descHeap, IID_PPV_ARGS(&s_uiCompositionDescHeapRtvCpu))))
            return FFX_ERROR_BACKEND_API_ERROR;
    }

    return FFX_OK;
}

void releaseUiBlitGpuResources()
{
    SafeRelease(s_uiCompositionRootSignature);
    SafeRelease(s_uiCompositionPipeline);
    SafeRelease(s_uiCompositionPremulPipeline);

    SafeRelease(s_uiCompositionDescRingBuffer);
    s_uiCompositionDescRingBufferBase = 0;
    
    SafeRelease(s_uiCompositionDescHeapRtvCpu);
    s_uiCompositionNextRtvDescriptor = 0;
}

FFX_API FfxErrorCode ffxFrameInterpolationUiComposition(const FfxPresentCallbackDescription* params, void* unusedUserCtx)
{
    (void)unusedUserCtx;
    ID3D12Device*   dx12Device  = reinterpret_cast<ID3D12Device*>(params->device);
    ID3D12Resource* pRtResource = (ID3D12Resource*)(params->outputSwapChainBuffer.resource);

    FFX_ASSERT(nullptr != dx12Device);
    FFX_ASSERT(nullptr != pRtResource);

    // blit backbuffer and composit UI using a VS/PS pass
    D3D12_RESOURCE_DESC desc = pRtResource->GetDesc();

    FfxErrorCode res = verifyUiBlitGpuResources(dx12Device, desc.Format);
    if (res != FFX_OK)
        return res;
    
    ID3D12CommandList*         pCommandList            = reinterpret_cast<ID3D12CommandList*>(params->commandList);
    ID3D12GraphicsCommandList* pCmdList                = (ID3D12GraphicsCommandList*)pCommandList;
    ID3D12PipelineState*       dx12PipelineStateObject = nullptr;
    if (params->usePremulAlpha)
    {
        dx12PipelineStateObject = s_uiCompositionPremulPipeline;
    }
    else
    {
        dx12PipelineStateObject = s_uiCompositionPipeline;
    }

    ID3D12Resource*            pResBackbuffer          = (ID3D12Resource*)(params->currentBackBuffer.resource);
    ID3D12Resource*            pResUI                  = (ID3D12Resource*)(params->currentUI.resource);

    FFX_ASSERT(nullptr != pCommandList);
    FFX_ASSERT(nullptr != pCmdList);
    FFX_ASSERT(nullptr != dx12PipelineStateObject);
    FFX_ASSERT(nullptr != pResBackbuffer);

    if (nullptr == pResUI)
    {
        // just do a resource copy to the real swapchain
        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource   = pResBackbuffer;
        barriers[0].Transition.StateBefore = ffxGetDX12StateFromResourceState(params->currentBackBuffer.state);
        barriers[0].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;

        barriers[1].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource   = pRtResource;
        barriers[1].Transition.StateBefore = ffxGetDX12StateFromResourceState(params->outputSwapChainBuffer.state);
        barriers[1].Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
        pCmdList->ResourceBarrier(_countof(barriers), barriers);

        pCmdList->CopyResource(pRtResource, pResBackbuffer);

        for (size_t i = 0; i < _countof(barriers); ++i)
        {
            D3D12_RESOURCE_STATES tmpStateBefore = barriers[i].Transition.StateBefore;
            barriers[i].Transition.StateBefore   = barriers[i].Transition.StateAfter;
            barriers[i].Transition.StateAfter    = tmpStateBefore;
        }

        pCmdList->ResourceBarrier(_countof(barriers), barriers);
    }
    else
    {
        uint32_t               barrierCount = 0;
        D3D12_RESOURCE_BARRIER barriers[3] = {};
        barriers[barrierCount].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[barrierCount].Transition.pResource   = pResBackbuffer;
        barriers[barrierCount].Transition.StateBefore = ffxGetDX12StateFromResourceState(params->currentBackBuffer.state);
        barriers[barrierCount].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (barriers[barrierCount].Transition.StateBefore != barriers[barrierCount].Transition.StateAfter)
        {
            ++barrierCount;
        }
        barriers[barrierCount].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[barrierCount].Transition.pResource   = pResUI;
        barriers[barrierCount].Transition.StateBefore = ffxGetDX12StateFromResourceState(params->currentUI.state);
        barriers[barrierCount].Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if (barriers[barrierCount].Transition.StateBefore != barriers[barrierCount].Transition.StateAfter)
        {
            ++barrierCount;
        }
        barriers[barrierCount].Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[barrierCount].Transition.pResource   = pRtResource;
        barriers[barrierCount].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barriers[barrierCount].Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        if (barriers[barrierCount].Transition.StateBefore != barriers[barrierCount].Transition.StateAfter)
        {
            ++barrierCount;
        }
        pCmdList->ResourceBarrier(barrierCount, barriers);

        // set root signature
        pCmdList->SetGraphicsRootSignature(s_uiCompositionRootSignature);

        // set descriptor heap
        ID3D12DescriptorHeap* dx12DescriptorHeap = reinterpret_cast<ID3D12DescriptorHeap*>(s_uiCompositionDescRingBuffer);
        pCmdList->SetDescriptorHeaps(1, &dx12DescriptorHeap);

        D3D12_RENDER_TARGET_VIEW_DESC colorDesc = {};
        colorDesc.Format                        = desc.Format;
        colorDesc.ViewDimension                 = D3D12_RTV_DIMENSION_TEXTURE2D;
        colorDesc.Texture2D.MipSlice            = 0;
        colorDesc.Texture2D.PlaneSlice          = 0;

        // set up the descriptor table
        D3D12_GPU_DESCRIPTOR_HANDLE gpuView = dx12DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        gpuView.ptr += s_uiCompositionDescRingBufferBase * dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        
        D3D12_CPU_DESCRIPTOR_HANDLE cpuView = dx12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        cpuView.ptr += s_uiCompositionDescRingBufferBase * dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        D3D12_SHADER_RESOURCE_VIEW_DESC dx12SrvDescription  = {};
        dx12SrvDescription.Shader4ComponentMapping          = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        dx12SrvDescription.Format                           = ffxGetDX12FormatFromSurfaceFormat(params->currentBackBuffer.description.format);
        dx12SrvDescription.ViewDimension                    = D3D12_SRV_DIMENSION_TEXTURE2D;
        dx12SrvDescription.Texture2D.MipLevels              = pResBackbuffer->GetDesc().MipLevels;
        dx12Device->CreateShaderResourceView(pResBackbuffer, &dx12SrvDescription, cpuView);

        cpuView.ptr += dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        
        const D3D12_RESOURCE_DESC uiDesc                    = pResUI->GetDesc();
        dx12SrvDescription.Format                           = convertFormatSrv(ffxGetDX12FormatFromSurfaceFormat(params->currentUI.description.format));
        dx12SrvDescription.Texture2D.MipLevels              = uiDesc.MipLevels;
        dx12Device->CreateShaderResourceView(pResUI, &dx12SrvDescription, cpuView);

        s_uiCompositionDescRingBufferBase = (s_uiCompositionDescRingBufferBase + 2) % s_uiCompositionDescRingBufferSize;
        pCmdList->SetGraphicsRootDescriptorTable(0, gpuView);

        D3D12_CPU_DESCRIPTOR_HANDLE backbufferRTV = s_uiCompositionDescHeapRtvCpu->GetCPUDescriptorHandleForHeapStart();
        backbufferRTV.ptr += s_uiCompositionNextRtvDescriptor * dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        s_uiCompositionNextRtvDescriptor = (s_uiCompositionNextRtvDescriptor + 1) % s_uiCompositionDescHeapRtvSize;
        dx12Device->CreateRenderTargetView(pRtResource, &colorDesc, backbufferRTV);

        D3D12_RESOURCE_DESC backBufferDesc = pRtResource->GetDesc();
        D3D12_VIEWPORT      vpd            = {0.0f, 0.0f, static_cast<float>(backBufferDesc.Width), static_cast<float>(backBufferDesc.Height), 0.0f, 1.0f};
        D3D12_RECT          srd            = {0, 0, (LONG)backBufferDesc.Width, (LONG)backBufferDesc.Height};

        pCmdList->OMSetRenderTargets(1, &backbufferRTV, true, NULL);
        pCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCmdList->SetPipelineState(dx12PipelineStateObject);
        pCmdList->RSSetViewports(1, &vpd);
        pCmdList->RSSetScissorRects(1, &srd);
        pCmdList->DrawInstanced(3, 1, 0, 0);

        for (int i = 0; i < (int)barrierCount; ++i)
        {
            D3D12_RESOURCE_STATES  tmpStateBefore   = barriers[i].Transition.StateBefore;
            barriers[i].Transition.StateBefore      = barriers[i].Transition.StateAfter;
            barriers[i].Transition.StateAfter       = tmpStateBefore;
        }

        pCmdList->ResourceBarrier(barrierCount, barriers);
    }

    return FFX_OK;
}
