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

#include "core/contentmanager.h"
#include "render/rendermodule.h"

namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class ResourceView;
    class RootSignature;
    struct AnimationComponentData;
    class MeshComponent;


    class RayTracingRenderModule : public RenderModule, public ContentListener
    {
    public:
        RayTracingRenderModule() : RenderModule(L"RayTracingRenderModule"), ContentListener() {}
        virtual ~RayTracingRenderModule();

        void Init(const json& initData) override;
        void Execute(double deltaTime, CommandList* pCmdList) override;

        // ContentListener overrides
        void OnNewContentLoaded(ContentBlock* pContentBlock) override;
        void OnContentUnloaded(cauldron::ContentBlock* pContentBlock) override;

    private:
        // No copy, No move
        NO_COPY(RayTracingRenderModule)
        NO_MOVE(RayTracingRenderModule)

    private:
        std::mutex          m_CriticalSection;

        struct RTAnimatedMeshes
        {
            const AnimationComponentData* pAnimationComponentData = nullptr;
            const MeshComponent*      pMeshComponent      = nullptr;
        };
        std::vector<RTAnimatedMeshes> m_RTAnimatedMeshes;

        void RebuildAnimatedBLAS(CommandList* pCmdList);
    };

    }  // namespace cauldron

