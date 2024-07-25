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

#include "render/resourceresizedlistener.h"
#include "render/resourceview.h"

#include <vector>

namespace cauldron
{
    struct BufferAddressInfo;
    class CommandList;
    class Buffer;
    class PipelineObject;
    class RootSignature;
    class Sampler;
    class Texture;
    class TLAS;

    /// Maximum number of supported push-type entries
    ///
    /// @ingroup CauldronRender
    #define MAX_PUSH_CONSTANTS_ENTRIES 512

    /**
     * @class ParameterSet
     *
     * The parameter sets are how resource binding is handled in <c><i>FidelityFX Cauldron Framework</i></c>. They setup ahead of time at initialization
     * and then bound each frame prior to pipeline execution to bind all necessary resources to the active pipeline.
     *
     * @ingroup CauldronRender
     */
    class ParameterSet : public ResourceResizedListener
    {
    public:
        /**
         * @brief   ParameterSet instance creation function. Implemented per api/platform to return the correct
         *          internal resource type.
         */
        static ParameterSet* CreateParameterSet(RootSignature* pRootSignature, ResourceView* pImmediateViews = nullptr);

        /**
         * @brief   Destruction.
         */
        virtual ~ParameterSet();

        /**
         * @brief   Assigns a root constant buffer resource to a specific slot index.
         */
        virtual void SetRootConstantBufferResource(const GPUResource* pResource, const size_t size, uint32_t slotIndex) = 0;
        
        /**
         * @brief   Assigns a texture SRV resource view to a specific slot index.
         */
        virtual void SetTextureSRV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex = 0, int32_t mip = -1, int32_t arraySize = -1, int32_t firstSlice = -1) = 0;
        
        /**
         * @brief   Assigns a texture UAV resource view to a specific slot index.
         */
        virtual void SetTextureUAV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex = 0, int32_t mip = -1, int32_t arraySize = -1, int32_t firstSlice = -1) = 0;
        
        /**
         * @brief   Assigns a buffer SRV resource view to a specific slot index.
         */
        virtual void SetBufferSRV(const Buffer* pBuffer, uint32_t slotIndex = 0, uint32_t firstElement = -1, uint32_t numElements = -1) = 0;
        
        /**
         * @brief   Assigns a buffer UAV resource view to a specific slot index.
         */
        virtual void SetBufferUAV(const Buffer* pBuffer, uint32_t slotIndex = 0, uint32_t firstElement = -1, uint32_t numElements = -1) = 0;
        
        /**
         * @brief   Assigns a sampler resource to a specific slot index.
         */
        virtual void SetSampler(const Sampler* pSampler, uint32_t slotIndex = 0) = 0;
        
        /**
         * @brief   Assigns an acceleration structure resource to a specific slot index.
         */
        virtual void SetAccelerationStructure(const TLAS* pTLAS, uint32_t slotIndex = 0) = 0;

        /**
         * @brief   Updates a specified constant root buffer with current buffer address information.
         */
        virtual void UpdateRootConstantBuffer(const BufferAddressInfo* pRootConstantBuffer, uint32_t rootBufferIndex) = 0;
        
        /**
         * @brief   Updates one or more root 32-bit constants starting at a specified entry.
         */
        virtual void UpdateRoot32BitConstant(const uint32_t numEntries, const uint32_t* pConstData, uint32_t rootBufferIndex) = 0;
        
        /**
         * @brief   Binds all resources to the pipeline for GPU workload execution.
         */
        virtual void Bind(CommandList* pCmdList, const PipelineObject* pPipeline) = 0;

        /**
         * @brief   Assigns an offset for binding type. This is used for cauldron backend immediate mode binding.
         */
        void SetBindTypeOffset(BindingType bindType, uint32_t bindingOffset) { m_ImmediateTypeOffsets[static_cast<uint32_t>(bindType)] = bindingOffset; }

        /**
         * @brief   Takes care of automatically re-binding recreated resources to their proper parameter set when resources are resized.
         */
        virtual void OnResourceResized() override;

    private:
        // No copy, No move
        NO_COPY(ParameterSet)
        NO_MOVE(ParameterSet)

        // This parameter set lives on the stack and is used for direct setting. None of it's resources are allocated and must therefore not be released.
        bool m_Immediate = false;

    protected:
        ParameterSet(RootSignature* pRootSignature, ResourceView* pImmediateViews, uint32_t numBufferedSets);

        // Returns the right resource table index in the list of binding descriptions
        virtual int32_t GetResourceTableIndex(BindingType bindType, uint32_t slotIndex, const wchar_t* bindName) = 0;

        // returns the offset in the corresponding resource view
        ResourceViewInfo BindTextureSRV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t bufferedSetIndex);
        ResourceViewInfo BindTextureUAV(const Texture* pTexture, ViewDimension dimension, uint32_t slotIndex, int32_t mip, int32_t arraySize, int32_t firstSlice, uint32_t bufferedSetIndex);
        ResourceViewInfo BindBufferUAV(const Buffer* pBuffer, uint32_t slotIndex, uint32_t firstElement, uint32_t numElements, uint32_t bufferedSetIndex);
        ResourceViewInfo BindBufferSRV(const Buffer* pBuffer, uint32_t slotIndex, uint32_t firstElement, uint32_t numElements, uint32_t bufferedSetIndex);
        ResourceViewInfo BindSampler(const Sampler* pSampler, uint32_t slotIndex, uint32_t bufferedSetIndex);

        ResourceViewInfo GetTextureSRV(uint32_t rootParameterIndex, uint32_t slotIndex);
        ResourceViewInfo GetTextureUAV(uint32_t rootParameterIndex, uint32_t slotIndex);
        ResourceViewInfo GetBufferSRV(uint32_t rootParameterIndex, uint32_t slotIndex);
        ResourceViewInfo GetBufferUAV(uint32_t rootParameterIndex, uint32_t slotIndex);

        void CheckResizable();

        const uint32_t m_BufferedSetCount;
        uint32_t       m_CBVCount        = 0;
        uint32_t       m_TextureSRVCount = 0;
        uint32_t       m_BufferSRVCount  = 0;
        uint32_t       m_TextureUAVCount = 0;
        uint32_t       m_BufferUAVCount  = 0;
        uint32_t       m_SamplerCount    = 0;

        RootSignature*  m_pRootSignature            = nullptr;
        ResourceView*   m_pCBVResourceViews         = nullptr;
        ResourceView*   m_pTextureSRVResourceViews  = nullptr;
        ResourceView*   m_pBufferSRVResourceViews   = nullptr;
        ResourceView*   m_pTextureUAVResourceViews  = nullptr;
        ResourceView*   m_pBufferUAVResourceViews   = nullptr;
        ResourceView*   m_pSamplerResourceViews     = nullptr;

        // Immediate mode information
        ResourceView*   m_pImmediateResourceViews   = nullptr;
        uint32_t        m_ImmediateTypeOffsets[static_cast<uint32_t>(BindingType::Count)] = {0};

        struct BoundResource
        {
            union
            {
                const Texture*  pTexture = nullptr;
                const Buffer*   pBuffer;
                const Sampler*  pSampler;
            };

            uint32_t       RootParameterIndex = 0;              // Maps to ViewTableIndex index values
            uint32_t       ShaderRegister = 0;
            ViewDimension  Dimension = ViewDimension::Unknown;

            int32_t        Mip = -1;

            union
            {
                int32_t     FirstSlice = -1;
                int32_t     FirstElement;
            };

            union
            {
                int32_t     ArraySize = -1;
                int32_t     NumElements;
            };
        };

        std::vector<BoundResource> m_BoundCBVs;
        std::vector<BoundResource> m_BoundTextureSRVs;
        std::vector<BoundResource> m_BoundTextureUAVs;
        std::vector<BoundResource> m_BoundBufferSRVs;
        std::vector<BoundResource> m_BoundBufferUAVs;
        std::vector<BoundResource> m_BoundSamplers;
    };

} // namespace cauldron
