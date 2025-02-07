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

#include "render/rendermodule.h"
#include "shaders/shadercommon.h"

namespace cauldron
{
    class RasterView;
    class SwapChainRenderTarget;
    class RootSignature;
    class PipelineObject;
    class ParameterSet;
    class Texture;

    class SwapChainRenderModule : public RenderModule
    {
    public:
        SwapChainRenderModule() 
            : RenderModule(L"SwapChainRenderModule"),
              m_pBackbufferClearColor{0.f,0.f,0.f,1.0f}
        {}
        virtual ~SwapChainRenderModule();

        virtual void Init(const json& initData) override;
        virtual void Execute(double deltaTime, CommandList* pCmdList) override;

    private:
        // No copy, No move
        NO_COPY(SwapChainRenderModule)
        NO_MOVE(SwapChainRenderModule)

    private:

        SwapchainCBData                 m_ConstantData;

        RootSignature*                  m_pRootSignature = nullptr;
        const RasterView*               m_pRasterView    = nullptr;
        PipelineObject*                 m_pPipelineObj   = nullptr;
        ParameterSet*                   m_pParameters    = nullptr;
        const SwapChainRenderTarget*    m_pRenderTarget  = nullptr;
        const Texture*                  m_pTexture       = nullptr;
        float                           m_pBackbufferClearColor[4];
    };

} // namespace cauldron
