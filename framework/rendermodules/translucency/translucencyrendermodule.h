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
#include "shaders/lightingcommon.h"
#include "shaders/particlerendercommon.h"

#include "core/contentmanager.h"
#include "render/rendermodule.h"
#include "render/shaderbuilder.h"
#include "render/pipelinedesc.h"

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
    class IndirectWorkload;
    class ParticleSystem;
}  // namespace cauldron

/// A structure representing optional shader instructions and targets to apply to translucency pipelines
///
/// @ingroup CauldronRender
struct OptionalTransparencyOptions
{
    std::vector<std::pair<const cauldron::Texture*, cauldron::BlendDesc>>     OptionalTargets = {};         ///< Vector of render targets and blend descriptions to additionally write to.
    std::wstring                                                        OptionalAdditionalOutputs = L"";    ///< String representing code to define into the translucency shader as addition output.
    std::wstring                                                        OptionalAdditionalExports = L"";    ///< String representing code to define into the translucency shader as addition exports.
};

/**
* @class TranslucencyRenderModule
*
* The Translucency render module is responsible for rendering all translucent geometry 
* and particles in a sorted (back to front) manner.
*
* @ingroup CauldronRender
*/
class TranslucencyRenderModule : public cauldron::RenderModule, public cauldron::ContentListener
{
public:
    
    /**
    * @brief   Construction.
    */
    TranslucencyRenderModule() : RenderModule(L"TranslucencyRenderModule"), ContentListener() {}

    /**
    * @brief   Destruction.
    */
    virtual ~TranslucencyRenderModule();

    /**
    * @brief   Initialization function. Sets up target pointers and other global data.
    */
    void Init(const json& initData) override;

    /**
    * @brief   Renders all active translucent geometry and particles in the <c><i>Scene</i></c>.
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

    /**
    * @brief   Sets optional transparency options to append to created pipelines.
    */
    void AddOptionalTransparencyOptions(const OptionalTransparencyOptions& options);

private:
    // No copy, No move
    NO_COPY(TranslucencyRenderModule)
    NO_MOVE(TranslucencyRenderModule)

    // Content creation helpers - not thread safe
    uint32_t CreatePipelineObject(const cauldron::Surface* pSurface);
    int32_t  AddTexture(const cauldron::Material* pMaterial, const cauldron::TextureClass textureClass, int32_t& textureSamplerIndex);
    void RemoveTexture(int32_t index);

private:
    bool m_VariableShading = false;
    uint32_t        m_ShadowMapCount   = 0;  // temporary variable to track the number of shadow maps

    // Constant data for Lighting
    LightingCBData m_LightingConstantData;

    cauldron::RootSignature*        m_pRootSignature       = nullptr;
    cauldron::ParameterSet*         m_pParameterSet        = nullptr;

    const cauldron::Texture*        m_pColorRenderTarget   = nullptr;
    const cauldron::Texture*        m_pDepthTarget         = nullptr;

    std::vector<const cauldron::RasterView*> m_RasterViews = {};

    OptionalTransparencyOptions     m_OptionalTransparencyOptions = {};

    struct BoundTexture
    {
        const cauldron::Texture* pTexture = nullptr;
        uint32_t count                    = 1;
    };
    std::vector<BoundTexture>       m_Textures;
    std::vector<cauldron::Sampler*> m_Samplers;
    std::mutex m_CriticalSection;

    struct PipelineSurfaceRenderInfo
    {
        const cauldron::Entity* pOwner   = nullptr;
        const cauldron::Surface* pSurface = nullptr;
        TextureIndices  TextureIndicies;
    };

    struct PipelineHashObject
    {
        cauldron::PipelineObject* m_Pipeline     = nullptr;
        uint64_t                  m_PipelineHash   = 0;
        uint32_t                  m_UsedAttributes = 0;
    };
    std::vector<PipelineHashObject> m_PipelineHashObjects;

    // Translucent pass
    struct TranslucentRenderData
    {
        float                     m_depth          = {};
        PipelineSurfaceRenderInfo m_RenderSurface  = {};
        cauldron::PipelineObject* m_Pipeline       = nullptr;
        uint32_t                  m_UsedAttributes = 0;
        operator float() { return -m_depth; }
    };
    std::vector<TranslucentRenderData> m_TranslucentRenderSurfaces;

    cauldron::RootSignature*        m_pParticlesRenderRootSignature = nullptr;
    cauldron::ParameterSet*         m_pParticlesRenderParameters    = nullptr;
    std::vector<PipelineHashObject> m_pParticlesRenderPipelineHashObjects;

    cauldron::IndirectWorkload* m_pIndirectWorkload = nullptr;

    struct PipelineParticlesRenderInfo
    {
        const cauldron::Entity*  pOwner                 = nullptr;
        const cauldron::ParticleSystem* pParticleSystem = nullptr;
    };

    struct ParticlesRenderData
    {
        float                       m_depth           = {};
        PipelineParticlesRenderInfo m_RenderParticles = {};
        cauldron::PipelineObject*   m_Pipeline        = nullptr;
        bool                        m_ReadyForFrame   = false;
        operator float() { return -m_depth; }
    };
    std::vector<ParticlesRenderData> m_RenderParticleSpawners;
};
