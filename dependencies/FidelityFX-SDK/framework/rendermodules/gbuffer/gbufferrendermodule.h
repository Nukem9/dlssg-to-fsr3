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
#include "render/rendermodule.h"

#include <memory>
#include <mutex>
#include <vector>

namespace cauldron
{
    class ParameterSet;
    class PipelineObject;
    class RenderTarget;
    class ResourceView;
    struct RootSignatureDesc;
    class RootSignature;
    class Texture;
    class Sampler;
    class RasterView;
}  // namespace cauldron

/**
* @class GBufferRenderModule
*
* The GBuffer render module is responsible for rendering the gbuffer for all loaded scene entities.
* If initialized to do so, will also output motion vectors for the frame.
*
* @ingroup CauldronRender
*/
class GBufferRenderModule : public cauldron::RenderModule, public cauldron::ContentListener
{
public:

    /**
    * @brief   Construction.
    */
    GBufferRenderModule() : RenderModule(L"GBufferRenderModule"), ContentListener() {}

    /**
    * @brief   Destruction.
    */
    virtual ~GBufferRenderModule();

    /**
    * @brief   Initialization function. Sets up target pointers and other global data.
    */
    void Init(const json& initData) override;

    /**
    * @brief   Renders all active geometric entities in the <c><i>Scene</i></c>.
    */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
    * @brief   Callback invoked when new content is loaded so we can create additional pipelines if needed.
    */
    void OnNewContentLoaded(cauldron::ContentBlock* pContentBlock) override;

    /**
    * @brief   Callback invoked when content is unloaded. Permits us to clean things up if needed.
    */
    void OnContentUnloaded(cauldron::ContentBlock* pContentBlock) override;

private:
    // No copy, No move
    NO_COPY(GBufferRenderModule)
    NO_MOVE(GBufferRenderModule)

    // Content creation helpers - not thread safe
    uint32_t GetPipelinePermutationID(const cauldron::Surface* pSurface);  //uint32_t vertexAttributeFlags, const Material* pMaterial);
    int32_t  AddTexture(const cauldron::Material* pMaterial, const cauldron::TextureClass textureClass, int32_t& textureSamplerIndex);
    void RemoveTexture(int32_t index);

private:

    bool                            m_VariableShading               = false;
    bool                            m_GenerateMotionVectors         = false;
    cauldron::RootSignature*        m_pRootSignature                = nullptr;
    cauldron::ParameterSet*         m_pParameterSet                 = nullptr;
    const cauldron::Texture*        m_pAlbedoRenderTarget           = nullptr;
    const cauldron::Texture*        m_pNormalRenderTarget           = nullptr;
    const cauldron::Texture*        m_pAoRoughnessMetallicTarget    = nullptr;
    const cauldron::Texture*        m_pDepthTarget                  = nullptr;
    const cauldron::Texture*        m_pMotionVector                 = nullptr;
    std::vector<const cauldron::RasterView*> m_RasterViews          = {};

    struct BoundTexture
    {
        const cauldron::Texture* pTexture = nullptr;
        uint32_t       count    = 1;
    };
    std::vector<BoundTexture>       m_Textures;
    std::vector<cauldron::Sampler*> m_Samplers;
    std::mutex m_CriticalSection;

    struct PipelineSurfaceRenderInfo
    {
        const cauldron::Entity*  pOwner   = nullptr;
        const cauldron::Surface* pSurface = nullptr;
        TextureIndices           TextureIndices;
    };

    struct PipelineRenderGroup
    {
        cauldron::PipelineObject*               m_Pipeline       = nullptr;
        uint64_t                                m_PipelineHash   = 0;
        uint32_t                                m_UsedAttributes = 0;
        std::vector<PipelineSurfaceRenderInfo>  m_RenderSurfaces = {};
    };

    std::vector<PipelineRenderGroup>            m_PipelineRenderGroups;
};
