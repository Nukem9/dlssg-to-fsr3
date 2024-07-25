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
#include "shaders/uicommon.h"

#include "imgui/imgui.h"

namespace cauldron
{
    class RootSignature;
    class RenderTarget;
    class RasterView;
    class PipelineObject;
    class ParameterSet;
    class Texture;
    class UICheckBox;
    class DynamicBufferPool;
    struct ResourceViewInfo;

    class UIRenderModule : public RenderModule
    {
    public:
        UIRenderModule() : RenderModule(L"UIRenderModule") {}
        virtual ~UIRenderModule();

        void Init(const json& initData) override;
        void UpdateUI(double deltaTime) override;
        void Execute(double deltaTime, CommandList* pCmdList) override;

        void SetFontResourceTexture(const Texture* pFontTexture);

        void SetAsyncRender(bool async);
        void SetRenderToTexture(bool renderToTexture);
        void SetCopyHudLessTexture(bool copyHudLessTexture);
        void SetUiSurfaceIndex(uint32_t uiTextureIndex);

        // Called on separate thread to render asynchronously.
        void ExecuteAsync(CommandList* pCmdList, const ResourceViewInfo* pRTViewInfo);
        void BlitBackbufferToHudLess(CommandList* pCmdList);
    private:
        // No copy, No move
        NO_COPY(UIRenderModule)
        NO_MOVE(UIRenderModule)

        struct RenderCommand
        {
            Rect     scissor;
            uint32_t indexCount, startIndex, baseVertex;
        };
        struct RenderParams
        {
            std::vector<ImDrawVert>    vtxBuffer{};
            std::vector<ImDrawIdx>     idxBuffer{};
            Mat4                       matrix;
            HDRCBData                  hdr;
            std::vector<RenderCommand> commands;
        };

        void UpdateMagnifierParams();
        void Render(CommandList* pCmdList, const ResourceViewInfo* pRTViewInfo, RenderParams* params);

    private:

        struct UIVertexBufferConstants
        {
            Mat4 ProjectionMatrix;
        };
	    UICheckBox*                 m_MagnifierEnabledPtr = nullptr; // weak ptr, shall be released by UISection itself.
        UICheckBox*                 m_LockMagnifierPositionPtr = nullptr;  // weak ptr, shall be released by UISection itself.

        HDRCBData                   m_HDRCBData                 = {};
        MagnifierCBData             m_MagnifierCBData           = {};
        bool                        m_MagnifierEnabled          = false;
        bool                        m_LockMagnifierPosition     = false;
        int32_t                     m_LockedMagnifierPositionX  = 0;
        int32_t                     m_LockedMagnifierPositionY  = 0;

        RootSignature*              m_pUIRootSignature          = nullptr;
        RootSignature*              m_pMagnifierRootSignature   = nullptr;

        const Texture*              m_pRenderTarget             = nullptr;
        const Texture*              m_pRenderTargetTemp         = nullptr;
        const RasterView*           m_pUIRasterView             = nullptr;

        uint32_t                    m_curUiTextureIndex         = 0;
        const cauldron::Texture*    m_pHudLessRenderTarget[2]   = {};
        const cauldron::Texture*    m_pUiOnlyRenderTarget[2]    = {};
        const RasterView*           m_pUiOnlyRasterView[2]      = {};
        RootSignature*              m_pHudLessRootSignature     = nullptr;
        const RasterView*           m_pHudLessRasterView[2]     = {};
        ParameterSet*               m_pHudLessParameters        = nullptr;
        PipelineObject*             m_pHudLessPipelineObj       = nullptr;


        PipelineObject*             m_pUIPipelineObj            = nullptr;
        PipelineObject*             m_pAsyncPipelineObj         = nullptr;
        PipelineObject*             m_pMagnifierPipelineObj     = nullptr;

        ParameterSet*               m_pUIParameters             = nullptr;
        ParameterSet*               m_pMagnifierParameters      = nullptr;

        bool                        m_AsyncRender               = false;
        std::atomic<RenderParams*>  m_AsyncChannel              = nullptr;
        RenderParams*               m_BufferedRenderParams      = nullptr;

        bool                        m_bCopyHudLessTexture       = false;
        bool                        m_bRenderToTexture          = false;
    };

} // namespace cauldron
