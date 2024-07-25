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
#include "core/uimanager.h"

#include <vector>

namespace cauldron
{
    class RootSignature;
    class RasterView;
    class ParameterSet;
    class PipelineObject;
    class RenderTarget;
    class Texture;
    class ResourceView;
}  // namespace cauldron

class AnimatedTexturesRenderModule : public cauldron::RenderModule
{
public:
    AnimatedTexturesRenderModule() : RenderModule(L"AnimatedTexturesRenderModule") {}
    virtual ~AnimatedTexturesRenderModule();

    virtual void Init(const json& initData) override;
    virtual void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

private:
    // Callback for texture loading so we can mark ourselves "ready"
    void TextureLoadComplete(const std::vector<const cauldron::Texture*>& textureList, void*);

    cauldron::RootSignature*                   m_pRootSignature   = nullptr;
    std::array<const cauldron::RasterView*, 5> m_pRasterViews     = {};
    cauldron::PipelineObject*                  m_pPipelineObj     = nullptr;
    const cauldron::Texture*                   m_pRenderTarget    = nullptr;
    const cauldron::Texture*                   m_pMotionVectors   = nullptr;
    const cauldron::Texture*                   m_pReactiveMask    = nullptr;
    const cauldron::Texture*                   m_pCompositionMask = nullptr;
    const cauldron::Texture*                   m_pDepthTarget     = nullptr;
    std::vector<const cauldron::Texture*>      m_Textures         = {};
    cauldron::ParameterSet*                    m_pParameters      = nullptr;

    float m_scrollFactor   = 0.0f;
    float m_rotationFactor = 0.0f;
    float m_flipTimer      = 0.0f;
    float m_Speed          = 1.0f;

    // UI
    cauldron::UISection* m_pUISection = nullptr;  // weak ptr.
};
