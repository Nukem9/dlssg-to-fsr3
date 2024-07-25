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

#if defined(_VK)
#include "render/vk/device_vk.h"
#include "render/vk/pipelinedesc_vk.h"
#include "render/vk/pipelineobject_vk.h"
#include "render/vk/rootsignature_vk.h"

#include "render/shaderbuilder.h"
#include "core/framework.h"
#include "misc/helpers.h"
#include "helpers.h"

// Sharing dxc for compilation
#include "dxc/inc/dxcapi.h"

#include <algorithm>
#include <array>

namespace cauldron
{
    // entry point names are hardcoded: Vertex, Pixel, Hull, Domain, Geometry, Compute
    const char* cEntryPointName = "main";

    VkPipelineLayout CreateLayout(const RootSignature* pRootSignature)
    {
        VkDescriptorSetLayout descriptorSetLayouts = pRootSignature->GetImpl()->VKDescriptorSetLayout();

        const std::vector<VkPushConstantRange>& pushConstantRange = pRootSignature->GetImpl()->VKPushConstantRanges();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayouts;
        pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRange.size());
        pipelineLayoutInfo.pPushConstantRanges = pushConstantRange.data();

        DeviceInternal* pDevice = GetDevice()->GetImpl();
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkResult res = vkCreatePipelineLayout(pDevice->VKDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout);
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Failed to create pipeline layout!");

        return pipelineLayout;
    }

    VkShaderStageFlagBits Convert(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Pixel:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Hull:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::Domain:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        default:
            return (VkShaderStageFlagBits)0;
        }
    }

    class PipelineShadersBuilder
    {
    public:
        PipelineShadersBuilder(const Device* pDevice, PipelineDesc& pipelineDesc, std::vector<const wchar_t*>* pShadersAdditionalParameters = nullptr)
        {
            // create shaders
            const size_t numShaders = pipelineDesc.m_ShaderDescriptions.size();
            m_Shaders.reserve(numShaders);
            m_ShadersEntryPoints.reserve(numShaders);
            
            bool setWave64 = false;
            if (pDevice->FeatureSupported(DeviceFeature::WaveSize) && pipelineDesc.GetPipelineType() == PipelineType::Compute && pipelineDesc.m_IsWave64)
            {
                m_ShaderStageRequiredSubgroupSizeCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT;
                m_ShaderStageRequiredSubgroupSizeCreateInfo.requiredSubgroupSize = 64;
                setWave64 = true;
            }

            for (auto& desc : pipelineDesc.m_ShaderDescriptions)
            {
                std::string entryPoint = WStringToString(desc.EntryPoint);
                m_ShadersEntryPoints.push_back(std::move(entryPoint));

                VkPipelineShaderStageCreateInfo shaderStageInfo = {};
                shaderStageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                shaderStageInfo.pNext                           = (setWave64 ? &m_ShaderStageRequiredSubgroupSizeCreateInfo : nullptr);
                shaderStageInfo.stage                           = Convert(desc.Stage);
                shaderStageInfo.module                          = BuildShaderModule(desc, pShadersAdditionalParameters);
                shaderStageInfo.pName                           = m_ShadersEntryPoints[m_ShadersEntryPoints.size() - 1].c_str();

                m_Shaders.push_back(shaderStageInfo);
            }
            for (auto& desc : pipelineDesc.m_ShaderBlobDescriptions)
            {
                VkPipelineShaderStageCreateInfo shaderStageInfo = {};
                shaderStageInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                shaderStageInfo.pNext                           = (setWave64 ? &m_ShaderStageRequiredSubgroupSizeCreateInfo : nullptr);
                shaderStageInfo.stage                           = Convert(desc.Stage);
                shaderStageInfo.module                          = CreateShaderModule(desc.pData, static_cast<size_t>(desc.DataSize));
                shaderStageInfo.pName                           = cEntryPointName;

                m_Shaders.push_back(shaderStageInfo);
            }
        }

        ~PipelineShadersBuilder()
        {
            // destroy the shader modules
            VkDevice device = GetDevice()->GetImpl()->VKDevice();
            for (auto shader : m_Shaders)
            {
                if (shader.module != VK_NULL_HANDLE)
                    vkDestroyShaderModule(device, shader.module, nullptr);
            }
        }

    private:
        VkShaderModule CreateShaderModule(const void* pData, const size_t size)
        {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = size;
            createInfo.pCode = reinterpret_cast<const uint32_t*>(pData);

            DeviceInternal* pDevice = GetDevice()->GetImpl();
            VkShaderModule shaderModule = VK_NULL_HANDLE;
            VkResult res = vkCreateShaderModule(pDevice->VKDevice(), &createInfo, nullptr, &shaderModule);

            if (res != VK_SUCCESS)
                CauldronWarning(L"failed to create shader module!");

            return shaderModule;
        }

        VkShaderModule BuildShaderModule(ShaderBuildDesc& shaderDesc, std::vector<const wchar_t*>* pShadersAdditionalParameters)
        {
            // Add defines for the platform
            shaderDesc.Defines[L"_VK"] = L"";
            shaderDesc.Defines[L"_HLSL"] = L"";

            // Additional parameters for Vulkan
            std::vector<const wchar_t*> additionalParameters;
            additionalParameters.push_back(L"-spirv");
            additionalParameters.push_back(L"-fspv-target-env=vulkan1.2"); // access to wave operations
            // https://github.com/KhronosGroup/glslang/issues/795
            //additionalParameters.push_back(L"-fspv-flatten-resource-arrays"); // each resource in an array takes one binding - use it once DXC compiler is fixed
            // binding shift for CBV
            additionalParameters.push_back(L"-fvk-b-shift");
            additionalParameters.push_back(CONSTANT_BUFFER_BINDING_SHIFT_STR);
            additionalParameters.push_back(L"0");
            // binding shift for sampler
            additionalParameters.push_back(L"-fvk-s-shift");
            additionalParameters.push_back(SAMPLER_BINDING_SHIFT_STR);
            additionalParameters.push_back(L"0");
            // binding shift for texture
            additionalParameters.push_back(L"-fvk-t-shift");
            additionalParameters.push_back(TEXTURE_BINDING_SHIFT_STR);
            additionalParameters.push_back(L"0");
            // binding shift for UAV
            additionalParameters.push_back(L"-fvk-u-shift");
            additionalParameters.push_back(UNORDERED_ACCESS_VIEW_BINDING_SHIFT_STR);
            additionalParameters.push_back(L"0");

            if (pShadersAdditionalParameters)
            {
                for (auto& parameter : *pShadersAdditionalParameters)
                {
                    additionalParameters.push_back(parameter);
                }
            }

            void* pBlob = CompileShaderToByteCode(shaderDesc, &additionalParameters);
            if (pBlob != nullptr)
            {
                IDxcBlob* pDXBlob = reinterpret_cast<IDxcBlob*>(pBlob);

                VkShaderModule module = CreateShaderModule(pDXBlob->GetBufferPointer(), pDXBlob->GetBufferSize());

                pDXBlob->Release();

                return module;
            }
            else
            {
                CauldronWarning(L"Unable to build the shader");
                return VK_NULL_HANDLE;
            }
        }

    public:
        VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT m_ShaderStageRequiredSubgroupSizeCreateInfo;
        std::vector<VkPipelineShaderStageCreateInfo> m_Shaders;
        std::vector<std::string> m_ShadersEntryPoints; // we need this vector to keep the pointers to the entry point name
    };

    PipelineObject* PipelineObject::CreatePipelineObject(const wchar_t*               pipelineObjectName,
                                                         const PipelineDesc&          Desc,
                                                         std::vector<const wchar_t*>* pAdditionalParameters /*=nullptr*/)
    {
        PipelineObjectInternal* pNewPipeline = new PipelineObjectInternal(pipelineObjectName);

        // Build in one step before returning
        pNewPipeline->Build(Desc, pAdditionalParameters);

        return pNewPipeline;
    }

    PipelineObjectInternal::PipelineObjectInternal(const wchar_t* PipelineObjectName) :
        PipelineObject(PipelineObjectName)
    {
    }

    PipelineObjectInternal::~PipelineObjectInternal()
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();
        vkDestroyPipelineLayout(pDevice->VKDevice(), m_PipelineLayout, nullptr);
        vkDestroyPipeline(pDevice->VKDevice(), m_Pipeline, nullptr);
    }

    void PipelineObjectInternal::BuildGraphicsPipeline(PipelineDesc& pipelineDesc)
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();

        m_PipelineLayout = CreateLayout(pipelineDesc.GetImpl()->m_pRootSignature);
        pDevice->SetResourceName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)m_Pipeline, m_Name.c_str());

        uint32_t width = 1920;
        uint32_t height = 1080;
        uint32_t offsetX = 0;
        uint32_t offsetY = 0;

        // dynamic so put dummy values
        VkViewport viewport;
        viewport.x = static_cast<float>(offsetX);
        viewport.y = static_cast<float>(height) - static_cast<float>(offsetY);
        viewport.width = static_cast<float>(width);
        viewport.height = -static_cast<float>(height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor;
        scissor.offset.x = offsetX;
        scissor.offset.y = offsetY;
        scissor.extent.width = width;
        scissor.extent.height = height;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = pipelineDesc.GetImpl()->m_NumAttachments;
        colorBlending.pAttachments = pipelineDesc.GetImpl()->m_BlendStates;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // viewport and scissors are dynamic stage to avoid recreating the pipeline if the size of the render target changes
        // Topology is also dynamic
        std::array<VkDynamicState, 4> dynamicStates;
        dynamicStates[0] = VK_DYNAMIC_STATE_VIEWPORT;
        dynamicStates[1] = VK_DYNAMIC_STATE_SCISSOR;
        dynamicStates[2] = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT;
        dynamicStates[3] = VK_DYNAMIC_STATE_FRAGMENT_SHADING_RATE_KHR;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateInfo.pNext = 0;
        dynamicStateInfo.flags = 0;
        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicStateInfo.pDynamicStates = dynamicStates.data();

        // build shaders
        PipelineShadersBuilder pipelineShaders(GetDevice(), pipelineDesc);

        VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo = {};
        pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        pipelineRenderingInfo.pNext = nullptr;
        pipelineRenderingInfo.viewMask = 0;
        pipelineRenderingInfo.colorAttachmentCount = pipelineDesc.GetImpl()->m_NumAttachments,
        pipelineRenderingInfo.pColorAttachmentFormats = pipelineRenderingInfo.colorAttachmentCount > 0 ? pipelineDesc.GetImpl()->m_ColorAttachmentFormats : nullptr;
        pipelineRenderingInfo.depthAttachmentFormat = pipelineDesc.GetImpl()->m_DepthFormat;
        pipelineRenderingInfo.stencilAttachmentFormat = HasStencilComponent(pipelineRenderingInfo.depthAttachmentFormat) ? pipelineRenderingInfo.depthAttachmentFormat : VK_FORMAT_UNDEFINED;

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = &pipelineRenderingInfo;
        pipelineInfo.stageCount = static_cast<uint32_t>(pipelineShaders.m_Shaders.size());
        pipelineInfo.pStages = pipelineShaders.m_Shaders.data();
        pipelineInfo.pVertexInputState = &pipelineDesc.GetImpl()->m_VertexInputInfo;
        pipelineInfo.pInputAssemblyState = &pipelineDesc.GetImpl()->m_InputAssemblyState;
        pipelineInfo.pTessellationState = nullptr;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &pipelineDesc.GetImpl()->m_RasterizationState;
        pipelineInfo.pMultisampleState = &pipelineDesc.GetImpl()->m_MultisampleState;
        pipelineInfo.pDepthStencilState = &pipelineDesc.GetImpl()->m_DepthStencilState;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicStateInfo; // Optional
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.renderPass = VK_NULL_HANDLE;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional
        pipelineInfo.flags = VK_PIPELINE_CREATE_RENDERING_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;

        VkResult res = vkCreateGraphicsPipelines(pDevice->VKDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
        CauldronAssert(ASSERT_ERROR, res == VK_SUCCESS, L"Failed to create graphics pipeline!");

        pDevice->SetResourceName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_Pipeline, m_Name.c_str());
    }

    void PipelineObjectInternal::BuildComputePipeline(PipelineDesc& pipelineDesc, std::vector<const wchar_t*>* pAdditionalParameters)
    {
        DeviceInternal* pDevice = GetDevice()->GetImpl();

        m_PipelineLayout = CreateLayout(pipelineDesc.GetImpl()->m_pRootSignature);
        pDevice->SetResourceName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)m_Pipeline, m_Name.c_str());

        // build shaders
        PipelineShadersBuilder pipelineShaders(GetDevice(), pipelineDesc, pAdditionalParameters);
        CauldronAssert(ASSERT_ERROR, pipelineShaders.m_Shaders.size() == 1, L"The compute pipeline description doesn't have exactly one shader.");

        VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.pNext = nullptr;
        pipelineInfo.flags = 0;
        pipelineInfo.stage = pipelineShaders.m_Shaders[0];
        pipelineInfo.layout = m_PipelineLayout;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;  // Optional
        pipelineInfo.basePipelineIndex = 0;  // Optional

        VkResult res = vkCreateComputePipelines(pDevice->VKDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
        CauldronAssert(ASSERT_CRITICAL, res == VK_SUCCESS, L"Failed to create compute pipeline!");

        pDevice->SetResourceName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)m_Pipeline, m_Name.c_str());
    }

    void PipelineObjectInternal::Build(const PipelineDesc& pipelineDesc, std::vector<const wchar_t*>* pAdditionalParameters)
    {
        m_Desc = std::move(pipelineDesc);
        m_Type = pipelineDesc.GetPipelineType();

        switch (m_Type)
        {
        case PipelineType::Graphics:
            BuildGraphicsPipeline(m_Desc);
            break;
        case PipelineType::Compute:
            BuildComputePipeline(m_Desc, pAdditionalParameters);
            break;
        default:
            CauldronCritical(L"Unable to build pipeline of unknown type");
            break;
        }
    }
} // namespace cauldron

#endif // #if defined(_VK)
