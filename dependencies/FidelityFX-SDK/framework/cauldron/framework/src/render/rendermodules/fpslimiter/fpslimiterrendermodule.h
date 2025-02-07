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
#include "render/renderdefines.h"
#include "core/uimanager.h"
#include "misc/math.h"
#include <chrono>

namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class RasterView;
    class ResourceView;
    class RootSignature;
    class RenderTarget;
    class Texture;
    class Buffer;

class FPSLimiterRenderModule : public RenderModule
{
public:
    FPSLimiterRenderModule();
    virtual ~FPSLimiterRenderModule();

    void Init(const json& initData) override;
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;
    bool IsFPSLimited() const { return m_LimitFPS; }

private:
    cauldron::RootSignature*  m_pRootSignature = nullptr;
    cauldron::ParameterSet*   m_pParameters    = nullptr;
    cauldron::PipelineObject* m_pPipelineObj   = nullptr;
    cauldron::Buffer*         m_pBuffer        = nullptr;

    double                   m_Overhead                   = 1.0;
    static const uint32_t    s_FRAME_TIME_HISTORY_SAMPLES = 4;
    uint64_t                 m_FrameTimeHistory[s_FRAME_TIME_HISTORY_SAMPLES];
    uint64_t                 m_FrameTimeHistorySum   = 0;
    uint64_t                 m_FrameTimeHistoryCount = 0;
    std::chrono::nanoseconds m_LastFrameEnd{0};

    // UI
    bool                m_LimitFPS      = false;
    bool                m_LimitGPU      = true;
    int32_t             m_TargetFPS     = 240;
};

}  // namespace cauldron
