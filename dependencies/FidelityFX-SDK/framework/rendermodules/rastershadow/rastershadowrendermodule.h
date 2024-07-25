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

#include "shaders/surfacerendercommon.h"

#include "core/contentmanager.h"
#include "core/uimanager.h"
#include "render/rendermodule.h"
#include "render/shadowmapresourcepool.h"

#include <memory>
#include <mutex>
#include <vector>

namespace cauldron
{
    class LightComponent;
    class ParameterSet;
    class PipelineObject;
    class RasterView;
    class RenderTarget;
    class ResourceView;
    struct RootSignatureDesc;
    class RootSignature;
    class Texture;
}  // namespace cauldron

/**
* @class RasterShadowRenderModule
*
* The raster shadow render module is responsible for rendering all rasterized shadow geometry.
*
* @ingroup CauldronRender
*/
class RasterShadowRenderModule : public cauldron::RenderModule, public cauldron::ContentListener
{
public:

    /**
    * @brief   Construction.
    */
    RasterShadowRenderModule() : RenderModule(L"RasterShadowRenderModule"), ContentListener() {}

    /**
    * @brief   Destruction.
    */
    virtual ~RasterShadowRenderModule();

    /**
    * @brief   Initialization function. Sets up resource pointers, pipeline objects, root signatures, and parameter sets.
    */
    void Init(const json& initData) override;

    /**
    * @brief   Renders all active shadow geometry in the <c><i>Scene</i></c> from each shadow-casting light's point of view.
    */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
    * @brief   Callback invoked when new content is loaded so we can create additional pipelines and resources if needed.
    */
    void OnNewContentLoaded(cauldron::ContentBlock* pContentBlock) override;

    /**
    * @brief   Callback invoked when content is unloaded. Permits us to clean things up if needed.
    */
    void OnContentUnloaded(cauldron::ContentBlock* pContentBlock) override;

private:
    // No copy, No move
    NO_COPY(RasterShadowRenderModule)
    NO_MOVE(RasterShadowRenderModule)

    // Content creation helpers
    uint32_t GetPipelinePermutationID(const cauldron::Surface* pSurface);  //uint32_t vertexAttributeFlags, const Material* pMaterial);
    int32_t  AddTexture(const cauldron::Material* pMaterial, const cauldron::TextureClass textureClass, int32_t& textureSamplerIndex);
    void RemoveTexture(int32_t index);

    void CreateShadowMapInfo(cauldron::LightComponent* pLightComponent, cauldron::ShadowMapResolution resolution);
    void DestroyShadowMapInfo(cauldron::LightComponent* pLightComponent);

    void UpdateCascades();

    void UpdateUIState(bool hasDirectional);

private:

    static constexpr uint32_t s_MaxTextureCount = 200;
    static constexpr uint32_t s_MaxSamplerCount = 20;

    cauldron::RootSignature* m_pRootSignature = nullptr;
    cauldron::ParameterSet*  m_pParameterSet  = nullptr;

    struct BoundTexture
    {
        const cauldron::Texture* pTexture = nullptr;
        uint32_t       count    = 1;
    };
    std::vector<BoundTexture> m_Textures;
    std::vector<cauldron::Sampler*> m_Samplers;
    std::mutex m_CriticalSection;

    struct PipelineSurfaceRenderInfo
    {
        const cauldron::Entity* pOwner   = nullptr;
        const cauldron::Surface* pSurface = nullptr;
        TextureIndices           TextureIndices;
    };

    struct PipelineRenderGroup
    {
        cauldron::PipelineObject*              m_Pipeline       = nullptr;
        uint64_t                               m_PipelineHash   = 0;
        uint32_t                               m_UsedAttributes = 0;
        std::vector<PipelineSurfaceRenderInfo> m_RenderSurfaces = {};
    };

    struct ShadowMapInfo
    {
        int                                          ShadowMapIndex = -1;
        std::vector<const cauldron::LightComponent*> LightComponents;  // list of light components using this shadow map
        const cauldron::RasterView*                  pRasterView = nullptr;
    };

    std::vector<ShadowMapInfo>       m_ShadowMapInfos;
    std::vector<PipelineRenderGroup> m_PipelineRenderGroups;

    // For UI params
    cauldron::UISection*                    m_UISection = nullptr; // weak ptr.
    bool                                    m_CascadeSplitPointsEnabled[3] = {false};
    bool                                    m_DirUIShowing = false;

    int                 m_NumCascades        = 4;
    std::vector<float>  m_CascadeSplitPoints = {10.0, 20.0, 60.0, 100.0};
    bool                m_MoveLightTexelSize = true;
};
