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
#include "shaders/skydomecommon.h"
#include "core/components/lightcomponent.h"

#include <memory>
#include <vector>

namespace cauldron
{
    // forward declaration
    class ParameterSet;
    class PipelineObject;
    class RasterView;
    class RenderTarget;
    class ResourceView;
    class RootSignature;
    class Texture;
    class RasterView;
}  // namespace cauldron

/**
* @class SkyDomeRenderModule
*
* The sky dome render module is responsible for rendering the set ibl map to background or to generate a procedural sky.
*
* @ingroup CauldronRender
*/
class SkyDomeRenderModule : public cauldron::RenderModule
{
public:

    /**
    * @brief   Construction.
    */
    SkyDomeRenderModule() : RenderModule(L"SkyDomeRenderModule") {}

    /**
    * @brief   Destruction.
    */
    virtual ~SkyDomeRenderModule();

    /**
    * @brief   Initialization function. Sets up resource pointers, pipeline objects, root signatures, and parameter sets.
    */
    void Init(const json& initData) override;

    /**
    * @brief   Calls common functions. Then calls ExecuteSkydomeGeneration if time of day changed. Finally, calls ExecuteSkydomeRender
    */
    void Execute(double deltaTime, cauldron::CommandList* pCmdList) override;

    /**
    * @brief   Procedurally generates the skydome
    */
    void ExecuteSkydomeGeneration(cauldron::CommandList* pCmdList);

    /**
    * @brief   Render the skydome tp color remder target
    */
    void ExecuteSkydomeRender(cauldron::CommandList* pCmdList);

private:
    // No copy, No move
    NO_COPY(SkyDomeRenderModule)
    NO_MOVE(SkyDomeRenderModule)

private:
    void InitSkyDome();
    void InitProcedural();
    void InitSampleDirections();
    void InitSunlight();

    void UpdateSunDirection();

    // Callback for texture loading so we can mark ourselves "ready"
    void TextureLoadComplete(const std::vector<const cauldron::Texture*>& textureList, void*);

    // common
    bool                        m_IsProcedural                = false;
    bool                        m_pShouldRunSkydomeGeneration = true;
    std::atomic_bool            m_CubemapGenerateReady        = true;
    std::atomic_bool            m_CubemapCopyReady            = false;
    SkydomeCBData  m_SkydomeConstantData;
    const cauldron::Texture*    m_pSkyTexture    = nullptr;
    const cauldron::Texture* m_pSkyTextureGenerated = nullptr;
    uint32_t m_pWidth;
    uint32_t m_pHeight;

    // CS procedural skydome
    cauldron::RootSignature* m_pRootSignatureSkyDomeGeneration = nullptr;
    cauldron::PipelineObject* m_pPipelineObjEnvironmentCube = nullptr;
    cauldron::ParameterSet* m_pParametersEnvironmentCube = nullptr;
    ProceduralCBData    m_pProceduralConstantData;
    cauldron::LightComponentData m_pSunlightCompData = {};
    cauldron::LightComponent*    m_pSunlightComponent = nullptr;
    cauldron::Entity*            m_pSunlight          = nullptr;
    UpscalerInformation m_pUpscalerInfo;

    //used by PS applying skydome to color render target
    cauldron::RootSignature* m_pRootSignatureApplySkydome = nullptr;
    cauldron::PipelineObject* m_pPipelineObjApplySkydome = nullptr;
    cauldron::ParameterSet* m_pParametersApplySkydome = nullptr;
    const cauldron::Texture*    m_pRenderTarget  = nullptr;
    const cauldron::Texture*    m_pDepthTarget   = nullptr;
    std::vector<const cauldron::RasterView*> m_pRasterViews   = {};

    //used by CS skydome and IBL shader generation
    cauldron::CommandList* m_pComputeCmdList = nullptr;
    uint64_t               m_SignalValue     = 0;

    const cauldron::Texture*  m_pIrradianceCube        = nullptr;
    const cauldron::Texture*  m_pIrradianceCubeGenerated    = nullptr;
    cauldron::PipelineObject* m_pPipelineObjIrradianceCube = nullptr;
    cauldron::ParameterSet*   m_pParametersIrradianceCube  = nullptr;

    const cauldron::Texture*               m_pPrefilteredCube      = nullptr;
    const cauldron::Texture*               m_pPrefilteredCubeGenerated  = nullptr;
    std::vector<const cauldron::Buffer*>   m_pSampleDirections     = {};
    std::vector<cauldron::PipelineObject*> m_pPipelineObjPrefilteredCube = {};
    std::vector<cauldron::ParameterSet*>   m_pParametersPrefilteredCube  = {};
};
