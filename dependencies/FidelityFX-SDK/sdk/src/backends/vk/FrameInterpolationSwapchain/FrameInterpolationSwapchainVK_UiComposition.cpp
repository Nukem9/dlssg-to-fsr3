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

#include <vulkan/vulkan.h>
#include "FrameInterpolationSwapchainVK_Helpers.h"
#include "FrameInterpolationSwapchainVK_UiComposition.h"

#include "FrameInterpolationSwapchainUiCompositionVS.h"
#include "FrameInterpolationSwapchainUiCompositionPS.h"

#include "FrameInterpolationSwapchainUiCompositionPremulVS.h"
#include "FrameInterpolationSwapchainUiCompositionPremulPS.h"

constexpr uint32_t c_uiCompositionRingBufferSize = 4;
constexpr uint32_t c_uiCompositionViewCount      = 3;  // 2 uniforms + 1 render target
constexpr uint32_t c_uiCompositionTotalViewCount = c_uiCompositionViewCount * c_uiCompositionRingBufferSize;

VkDevice              s_uiCompositionDevice              = VK_NULL_HANDLE;
VkDescriptorPool      s_uiCompositionDescriptorPool      = VK_NULL_HANDLE;
VkDescriptorSetLayout s_uiCompositionDescriptorSetLayout = VK_NULL_HANDLE;
VkPipelineLayout      s_uiCompositionPipelineLayout      = VK_NULL_HANDLE;
VkRenderPass          s_uiCompositionRenderPass          = VK_NULL_HANDLE;
VkPipeline            s_uiCompositionPipeline            = VK_NULL_HANDLE;
VkPipeline            s_uiCompositionPremulPipeline      = VK_NULL_HANDLE;
VkFormat              s_uiCompositionAttachmentFormat    = VK_FORMAT_UNDEFINED;
uint32_t              s_uiCompositionDescriptorSetIndex  = 0;
VkDescriptorSet       s_uiCompositionDescriptorSets[c_uiCompositionRingBufferSize];
VkImageView           s_uiCompositionImageViews[c_uiCompositionTotalViewCount];
VkFramebuffer         s_uiCompositionFramebuffers[c_uiCompositionRingBufferSize];

VkResult CreateShaderModule(VkDevice device, size_t codeSize, const uint32_t* pCode, VkShaderModule* pModule, const VkAllocationCallbacks* pAllocator)
{
    VkShaderModuleCreateInfo info = {};
    info.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.pNext                    = nullptr;
    info.flags                    = 0;
    info.codeSize                 = codeSize;
    info.pCode                    = pCode;

    return vkCreateShaderModule(device, &info, pAllocator, pModule);
}

#define FFX_BACKEND_API_ERROR_ON_VK_ERROR(res) if (res != VK_SUCCESS) return FFX_ERROR_BACKEND_API_ERROR;


void releaseUiBlitGpuResources(const VkAllocationCallbacks* pAllocator)
{
    for (uint32_t i = 0; i < _countof(s_uiCompositionDescriptorSets); ++i)
    {
        vkFreeDescriptorSets(s_uiCompositionDevice, s_uiCompositionDescriptorPool, 1, &s_uiCompositionDescriptorSets[i]);
        s_uiCompositionDescriptorSets[i] = VK_NULL_HANDLE;
    }

    vkDestroyDescriptorPool(s_uiCompositionDevice, s_uiCompositionDescriptorPool, pAllocator);
    s_uiCompositionDescriptorPool = VK_NULL_HANDLE;

    vkDestroyPipeline(s_uiCompositionDevice, s_uiCompositionPipeline, pAllocator);
    s_uiCompositionPipeline = VK_NULL_HANDLE;
    vkDestroyPipeline(s_uiCompositionDevice, s_uiCompositionPremulPipeline, pAllocator);
    s_uiCompositionPremulPipeline = VK_NULL_HANDLE;

    vkDestroyPipelineLayout(s_uiCompositionDevice, s_uiCompositionPipelineLayout, pAllocator);
    s_uiCompositionPipelineLayout = VK_NULL_HANDLE;

    vkDestroyDescriptorSetLayout(s_uiCompositionDevice, s_uiCompositionDescriptorSetLayout, pAllocator);
    s_uiCompositionDescriptorSetLayout = VK_NULL_HANDLE;

    vkDestroyRenderPass(s_uiCompositionDevice, s_uiCompositionRenderPass, pAllocator);
    s_uiCompositionRenderPass = nullptr;

    for (uint32_t i = 0; i < _countof(s_uiCompositionImageViews); ++i)
    {
        vkDestroyImageView(s_uiCompositionDevice, s_uiCompositionImageViews[i], pAllocator);
        s_uiCompositionImageViews[i] = VK_NULL_HANDLE;
    }
    for (uint32_t i = 0; i < _countof(s_uiCompositionFramebuffers); ++i)
    {
        vkDestroyFramebuffer(s_uiCompositionDevice, s_uiCompositionFramebuffers[i], pAllocator);
        s_uiCompositionFramebuffers[i] = VK_NULL_HANDLE;
    }
}

void DestroyUiCompositionPipeline(VkDevice device, const VkAllocationCallbacks* pAllocator)
{
    vkDestroyRenderPass(device, s_uiCompositionRenderPass, pAllocator);
    s_uiCompositionRenderPass = VK_NULL_HANDLE;
    vkDestroyPipelineLayout(device, s_uiCompositionPipelineLayout, pAllocator);
    s_uiCompositionPipelineLayout = VK_NULL_HANDLE;
    vkDestroyPipeline(device, s_uiCompositionPipeline, pAllocator);
    s_uiCompositionPipeline = VK_NULL_HANDLE;
    vkDestroyPipeline(device, s_uiCompositionPremulPipeline, pAllocator);
    s_uiCompositionPremulPipeline = VK_NULL_HANDLE;
}

// create the pipeline state to use for UI composition
// pretty similar to FfxCreatePipelineFunc
VkResult CreateUiCompositionPipeline(VkDevice device, VkFormat fmt, const VkAllocationCallbacks* pAllocator)
{
    VkResult res = VK_SUCCESS;

    // create render pass
    if (res == VK_SUCCESS)
    {
        VkAttachmentDescription attachmentDesc = {};
        attachmentDesc.flags                   = 0;
        attachmentDesc.format                  = fmt;
        attachmentDesc.samples                 = VK_SAMPLE_COUNT_1_BIT;
        attachmentDesc.loadOp                  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDesc.storeOp                 = VK_ATTACHMENT_STORE_OP_STORE;
        attachmentDesc.stencilLoadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachmentDesc.stencilStoreOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachmentDesc.initialLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachmentDesc.finalLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference attachmentRef = {};
        attachmentRef.attachment            = 0;
        attachmentRef.layout                = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subPassDesc    = {};
        subPassDesc.flags                   = 0;
        subPassDesc.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subPassDesc.inputAttachmentCount    = 0;
        subPassDesc.pInputAttachments       = nullptr;
        subPassDesc.colorAttachmentCount    = 1;
        subPassDesc.pColorAttachments       = &attachmentRef;
        subPassDesc.pResolveAttachments     = nullptr;
        subPassDesc.pDepthStencilAttachment = nullptr;
        subPassDesc.preserveAttachmentCount = 0;
        subPassDesc.pPreserveAttachments    = nullptr;

        VkRenderPassCreateInfo renderPassCreateInfo = {};
        renderPassCreateInfo.sType                  = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.pNext                  = nullptr;
        renderPassCreateInfo.flags                  = 0;
        renderPassCreateInfo.attachmentCount        = 1;
        renderPassCreateInfo.pAttachments           = &attachmentDesc;
        renderPassCreateInfo.subpassCount           = 1;
        renderPassCreateInfo.pSubpasses             = &subPassDesc;
        renderPassCreateInfo.dependencyCount        = 0;
        renderPassCreateInfo.pDependencies          = nullptr;

        res = vkCreateRenderPass(device, &renderPassCreateInfo, pAllocator, &s_uiCompositionRenderPass);
    }

    // create pipeline layout
    if (res == VK_SUCCESS)
    {
        VkPipelineLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.pNext                      = nullptr;
        layoutInfo.flags                      = 0;
        layoutInfo.setLayoutCount             = 1;
        layoutInfo.pSetLayouts                = &s_uiCompositionDescriptorSetLayout;
        layoutInfo.pushConstantRangeCount     = 0;
        layoutInfo.pPushConstantRanges        = nullptr;

        res = vkCreatePipelineLayout(device, &layoutInfo, pAllocator, &s_uiCompositionPipelineLayout);
    }

    // create VS module
    VkShaderModule moduleVS = VK_NULL_HANDLE;
    if (res == VK_SUCCESS)
    {
        res = CreateShaderModule(device, sizeof(g_mainVS), g_mainVS, &moduleVS, pAllocator);
    }

    // create PS module
    VkShaderModule modulePS = VK_NULL_HANDLE;
    if (res == VK_SUCCESS)
    {
        res = CreateShaderModule(device, sizeof(g_mainPS), g_mainPS, &modulePS, pAllocator);
    }

    // create Premul VS module
    VkShaderModule modulePremulVS = VK_NULL_HANDLE;
    if (res == VK_SUCCESS)
    {
        res = CreateShaderModule(device, sizeof(g_mainPremulVS), g_mainPremulVS, &modulePremulVS, pAllocator);
    }

    // create Premul PS module
    VkShaderModule modulePremulPS = VK_NULL_HANDLE;
    if (res == VK_SUCCESS)
    {
        res = CreateShaderModule(device, sizeof(g_mainPremulPS), g_mainPremulPS, &modulePremulPS, pAllocator);
    }

    // create pipeline
    if (res == VK_SUCCESS)
    {
        VkPipelineShaderStageCreateInfo shaderStageCreateInfos[2];
        shaderStageCreateInfos[0].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfos[0].pNext               = nullptr;
        shaderStageCreateInfos[0].flags               = 0;
        shaderStageCreateInfos[0].stage               = VK_SHADER_STAGE_VERTEX_BIT;
        // shaderStageCreateInfos[0].module will be set later
        shaderStageCreateInfos[0].pName               = "main";
        shaderStageCreateInfos[0].pSpecializationInfo = nullptr;

        shaderStageCreateInfos[1].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageCreateInfos[1].pNext               = nullptr;
        shaderStageCreateInfos[1].flags               = 0;
        shaderStageCreateInfos[1].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
        // shaderStageCreateInfos[1].module will be set later
        shaderStageCreateInfos[1].pName               = "main";
        shaderStageCreateInfos[1].pSpecializationInfo = nullptr;

        VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
        vertexInputStateCreateInfo.sType                                = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputStateCreateInfo.pNext                                = nullptr;
        vertexInputStateCreateInfo.flags                                = 0;
        vertexInputStateCreateInfo.vertexBindingDescriptionCount        = 0;
        vertexInputStateCreateInfo.pVertexBindingDescriptions           = nullptr;
        vertexInputStateCreateInfo.vertexAttributeDescriptionCount      = 0;
        vertexInputStateCreateInfo.pVertexAttributeDescriptions         = nullptr;

        VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
        inputAssemblyStateCreateInfo.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyStateCreateInfo.pNext                                  = nullptr;
        inputAssemblyStateCreateInfo.flags                                  = 0;
        inputAssemblyStateCreateInfo.topology                               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyStateCreateInfo.primitiveRestartEnable                 = VK_FALSE;

        VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
        rasterizationStateCreateInfo.sType                                  = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizationStateCreateInfo.pNext                                  = nullptr;
        rasterizationStateCreateInfo.flags                                  = 0;
        rasterizationStateCreateInfo.depthClampEnable                       = VK_FALSE;
        rasterizationStateCreateInfo.rasterizerDiscardEnable                = VK_FALSE;
        rasterizationStateCreateInfo.polygonMode                            = VK_POLYGON_MODE_FILL;
        rasterizationStateCreateInfo.cullMode                               = VK_CULL_MODE_NONE;
        rasterizationStateCreateInfo.frontFace                              = VK_FRONT_FACE_CLOCKWISE;
        rasterizationStateCreateInfo.depthBiasEnable                        = VK_FALSE;
        rasterizationStateCreateInfo.depthBiasConstantFactor                = 0.0f;
        rasterizationStateCreateInfo.depthBiasClamp                         = 0.0f;
        rasterizationStateCreateInfo.depthBiasSlopeFactor                   = 0.0f;
        rasterizationStateCreateInfo.lineWidth                              = 0.0f;

        VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
        depthStencilStateCreateInfo.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilStateCreateInfo.pNext                                 = nullptr;
        depthStencilStateCreateInfo.flags                                 = 0;
        depthStencilStateCreateInfo.depthTestEnable                       = VK_FALSE;
        depthStencilStateCreateInfo.depthWriteEnable                      = VK_FALSE;
        depthStencilStateCreateInfo.depthCompareOp                        = VK_COMPARE_OP_NEVER;
        depthStencilStateCreateInfo.depthBoundsTestEnable                 = VK_FALSE;
        depthStencilStateCreateInfo.stencilTestEnable                     = VK_FALSE;
        depthStencilStateCreateInfo.front.failOp                          = VK_STENCIL_OP_KEEP;
        depthStencilStateCreateInfo.front.passOp                          = VK_STENCIL_OP_KEEP;
        depthStencilStateCreateInfo.front.depthFailOp                     = VK_STENCIL_OP_KEEP;
        depthStencilStateCreateInfo.front.compareOp                       = VK_COMPARE_OP_NEVER;
        depthStencilStateCreateInfo.front.compareMask                     = 0;
        depthStencilStateCreateInfo.front.writeMask                       = 0;
        depthStencilStateCreateInfo.front.reference                       = 0;
        depthStencilStateCreateInfo.back                                  = depthStencilStateCreateInfo.front;
        depthStencilStateCreateInfo.minDepthBounds                        = 0.0f;
        depthStencilStateCreateInfo.maxDepthBounds                        = 1.0f;

        VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
        colorBlendAttachmentState.blendEnable                         = VK_FALSE;
        colorBlendAttachmentState.srcColorBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachmentState.dstColorBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachmentState.colorBlendOp                        = VK_BLEND_OP_ADD;
        colorBlendAttachmentState.srcAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachmentState.dstAlphaBlendFactor                 = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachmentState.alphaBlendOp                        = VK_BLEND_OP_ADD;
        colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
        colorBlendStateCreateInfo.sType                               = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateCreateInfo.pNext                               = nullptr;
        colorBlendStateCreateInfo.flags                               = 0;
        colorBlendStateCreateInfo.logicOpEnable                       = VK_FALSE;
        colorBlendStateCreateInfo.logicOp                             = VK_LOGIC_OP_CLEAR;
        colorBlendStateCreateInfo.attachmentCount                     = 1;
        colorBlendStateCreateInfo.pAttachments                        = &colorBlendAttachmentState;
        colorBlendStateCreateInfo.blendConstants[0]                   = 0.0f;
        colorBlendStateCreateInfo.blendConstants[1]                   = 0.0f;
        colorBlendStateCreateInfo.blendConstants[2]                   = 0.0f;
        colorBlendStateCreateInfo.blendConstants[3]                   = 0.0f;

        VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
        dynamicStateCreateInfo.sType                            = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicStateCreateInfo.pNext                            = nullptr;
        dynamicStateCreateInfo.flags                            = 0;
        dynamicStateCreateInfo.dynamicStateCount                = _countof(dynamicStates);
        dynamicStateCreateInfo.pDynamicStates                   = dynamicStates;

        // dynamic so put dummy values
        VkViewport viewport;
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = 1920;
        viewport.height   = 1080;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor;
        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = 0;
        scissor.extent.height = 0;

        VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
        viewportStateCreateInfo.sType                             = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCreateInfo.viewportCount                     = 1;
        viewportStateCreateInfo.pViewports                        = &viewport;
        viewportStateCreateInfo.scissorCount                      = 1;
        viewportStateCreateInfo.pScissors                         = &scissor;

        VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
        multisampleStateCreateInfo.sType                                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampleStateCreateInfo.pNext                                = nullptr;
        multisampleStateCreateInfo.flags                                = 0;
        multisampleStateCreateInfo.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
        multisampleStateCreateInfo.sampleShadingEnable                  = VK_FALSE;
        multisampleStateCreateInfo.minSampleShading                     = 0.0f;
        multisampleStateCreateInfo.pSampleMask                          = nullptr;
        multisampleStateCreateInfo.alphaToCoverageEnable                = VK_FALSE;
        multisampleStateCreateInfo.alphaToOneEnable                     = VK_FALSE;

        VkGraphicsPipelineCreateInfo info = {};
        info.sType                        = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.pNext                        = nullptr;
        info.flags                        = 0;
        info.stageCount                   = _countof(shaderStageCreateInfos);
        info.pStages                      = shaderStageCreateInfos;
        info.pVertexInputState            = &vertexInputStateCreateInfo;
        info.pInputAssemblyState          = &inputAssemblyStateCreateInfo;
        info.pTessellationState           = nullptr;  // TODO: can this be nullptr?
        info.pViewportState               = &viewportStateCreateInfo;
        info.pRasterizationState          = &rasterizationStateCreateInfo;
        info.pMultisampleState            = &multisampleStateCreateInfo;
        info.pDepthStencilState           = &depthStencilStateCreateInfo;
        info.pColorBlendState             = &colorBlendStateCreateInfo;
        info.pDynamicState                = &dynamicStateCreateInfo;
        info.layout                       = s_uiCompositionPipelineLayout;  // shared by both pipelines
        info.renderPass                   = s_uiCompositionRenderPass;      // shared by both pipelines
        info.subpass                      = 0;
        info.basePipelineHandle           = VK_NULL_HANDLE;
        info.basePipelineIndex            = -1;

        if (res == VK_SUCCESS)
        {
            shaderStageCreateInfos[0].module = moduleVS;
            shaderStageCreateInfos[1].module = modulePS;

            res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, pAllocator, &s_uiCompositionPipeline);
        }

        if (res == VK_SUCCESS)
        {
            shaderStageCreateInfos[0].module = modulePremulVS;
            shaderStageCreateInfos[1].module = modulePremulPS;

            res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &info, pAllocator, &s_uiCompositionPremulPipeline);
        }
    }

    // cleanup
    vkDestroyShaderModule(device, moduleVS, pAllocator);
    vkDestroyShaderModule(device, modulePS, pAllocator);
    vkDestroyShaderModule(device, modulePremulVS, pAllocator);
    vkDestroyShaderModule(device, modulePremulPS, pAllocator);

    if (res == VK_SUCCESS)
    {
        s_uiCompositionAttachmentFormat = fmt;
    }
    else
    {
        DestroyUiCompositionPipeline(device, pAllocator);
    }

    return res;
}

FfxErrorCodes verifyUiBlitGpuResources(VkDevice device, VkFormat fmt, const VkAllocationCallbacks* pAllocator)
{
    FFX_ASSERT(VK_NULL_HANDLE != device);

    if (s_uiCompositionDevice != device)
    {
        if (s_uiCompositionDevice != VK_NULL_HANDLE)
        {
            // new device
            releaseUiBlitGpuResources(pAllocator);
        }
        else
        {
            s_uiCompositionDevice = device;
        }
    }

    VkResult res = VK_SUCCESS;

    // create pool
    if (res == VK_SUCCESS && s_uiCompositionDescriptorPool == VK_NULL_HANDLE)
    {
        VkDescriptorPoolSize poolSize;
        poolSize.type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSize.descriptorCount = 8; // 4 frames * 2 textures should be enough

        VkDescriptorPoolCreateInfo info = {};
        info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        info.pNext                      = nullptr;
        info.flags                      = 0;
        info.maxSets                    = 4;
        info.poolSizeCount              = 1;
        info.pPoolSizes                 = &poolSize;

        res = vkCreateDescriptorPool(device, &info, pAllocator, &s_uiCompositionDescriptorPool);
    }

    // create set layout
    if (res == VK_SUCCESS && s_uiCompositionDescriptorSetLayout == VK_NULL_HANDLE)
    {
        VkDescriptorSetLayoutBinding bindings[2];
        bindings[0].binding            = 0;
        bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount    = 1;
        bindings[0].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[0].pImmutableSamplers = nullptr;

        bindings[1].binding            = 1;
        bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[1].descriptorCount    = 1;
        bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pNext                           = nullptr;
        info.flags                           = 0;
        info.bindingCount                    = 2;
        info.pBindings                       = bindings;

        res = vkCreateDescriptorSetLayout(device, &info, pAllocator, &s_uiCompositionDescriptorSetLayout);
    }

    // create sets
    {
        VkDescriptorSetAllocateInfo info;
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.pNext              = nullptr;
        info.descriptorPool     = s_uiCompositionDescriptorPool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &s_uiCompositionDescriptorSetLayout;
        for (uint32_t i = 0; i < _countof(s_uiCompositionDescriptorSets); ++i)
        {
            if (res == VK_SUCCESS && s_uiCompositionDescriptorSets[i] == VK_NULL_HANDLE)
            {
                res = vkAllocateDescriptorSets(device, &info, &s_uiCompositionDescriptorSets[i]);
            }
        }
    }

    if (res == VK_SUCCESS)
    {
        if (s_uiCompositionAttachmentFormat != fmt)
        {
            DestroyUiCompositionPipeline(device, pAllocator);
        }
        if (s_uiCompositionPipeline == VK_NULL_HANDLE)
        {
            res = CreateUiCompositionPipeline(device, fmt, pAllocator);
        }
    }

    if (res == VK_SUCCESS)
        return FFX_OK;
    else
        return FFX_ERROR_BACKEND_API_ERROR;
}

FFX_API FfxErrorCode ffxFrameInterpolationUiComposition(const FfxPresentCallbackDescription* params, void* unusedUserCtx)
{
    const VkAllocationCallbacks* pAllocator  = nullptr;

    VkDevice device            = reinterpret_cast<VkDevice>(params->device);
    VkImage  renderTargetImage = reinterpret_cast<VkImage>(params->outputSwapChainBuffer.resource);

    FFX_ASSERT(device != VK_NULL_HANDLE);
    FFX_ASSERT(renderTargetImage != VK_NULL_HANDLE);

    // blit backbuffer and composit UI using a VS/PS pass
    FfxErrorCode res = verifyUiBlitGpuResources(device, ffxGetVkFormatFromSurfaceFormat(params->outputSwapChainBuffer.description.format), pAllocator);
    if (res != FFX_OK)
        return res;

    VkCommandBuffer commandBuffer   = reinterpret_cast<VkCommandBuffer>(params->commandList);
    VkImage         backbufferImage = reinterpret_cast<VkImage>(params->currentBackBuffer.resource);
    VkImage         uiImage         = reinterpret_cast<VkImage>(params->currentUI.resource);
    
    FFX_ASSERT(commandBuffer != VK_NULL_HANDLE);
    FFX_ASSERT(s_uiCompositionPipeline != VK_NULL_HANDLE);
    FFX_ASSERT(s_uiCompositionPremulPipeline != VK_NULL_HANDLE);
    FFX_ASSERT(backbufferImage != VK_NULL_HANDLE);

    const uint32_t backBufferWidth    = params->currentBackBuffer.description.width;
    const uint32_t backBufferHeight   = params->currentBackBuffer.description.height;
    const uint32_t renderTargetWidth  = params->outputSwapChainBuffer.description.width;
    const uint32_t renderTargetHeight = params->outputSwapChainBuffer.description.height;

    // TODO: should we keep that?
    if ((backBufferWidth != renderTargetWidth) || (backBufferHeight != renderTargetHeight))
        return FFX_ERROR_INVALID_SIZE;

    // some helper functions
    auto setBarrier = [](VkImageMemoryBarrier* barriers,
                         uint32_t&             barrierCount,
                         VkImage               image,
                         FfxResourceStates     srcState,
                         VkAccessFlags         dstAccessMask,
                         VkImageLayout         newLayout,
                         VkPipelineStageFlags& srcStageMask) {
        VkImageMemoryBarrier& barrier           = barriers[barrierCount];
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.pNext                           = nullptr;
        barrier.srcAccessMask                   = getVKAccessFlagsFromResourceState(srcState);
        barrier.dstAccessMask                   = dstAccessMask;
        barrier.oldLayout                       = getVKImageLayoutFromResourceState(srcState);
        barrier.newLayout                       = newLayout;
        barrier.srcQueueFamilyIndex             = 0;
        barrier.dstQueueFamilyIndex             = 0;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        if (barrier.oldLayout != barrier.newLayout)
        {
            srcStageMask = srcStageMask | getVKPipelineStageFlagsFromResourceState(srcState);
            ++barrierCount;
        }
    };

    auto flipBarrier = [](VkImageMemoryBarrier& barrier) {
        VkAccessFlags tmpAccess = barrier.srcAccessMask;
        VkImageLayout tmpLayout = barrier.oldLayout;
        barrier.srcAccessMask   = barrier.dstAccessMask;
        barrier.dstAccessMask   = tmpAccess;
        barrier.oldLayout       = barrier.newLayout;
        barrier.newLayout       = tmpLayout;
    };

    if (uiImage == VK_NULL_HANDLE)
    {
        // just do a resource copy to the real swapchain
        VkImageMemoryBarrier barriers[2];
        uint32_t             barrierCount = 0;
        VkPipelineStageFlags srcStageMask = 0;
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        setBarrier(barriers, barrierCount, backbufferImage,   params->currentBackBuffer.state,     VK_ACCESS_TRANSFER_READ_BIT,  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, srcStageMask);
        setBarrier(barriers, barrierCount, renderTargetImage, params->outputSwapChainBuffer.state, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, srcStageMask);

        if (barrierCount > 0)
            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, barrierCount, barriers);
        
        VkImageCopy imageCopy;
        imageCopy.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopy.srcSubresource.mipLevel       = 0;
        imageCopy.srcSubresource.baseArrayLayer = 0;
        imageCopy.srcSubresource.layerCount     = 1;
        imageCopy.srcOffset.x                   = 0;
        imageCopy.srcOffset.y                   = 0;
        imageCopy.srcOffset.z                   = 0;
        imageCopy.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageCopy.dstSubresource.mipLevel       = 0;
        imageCopy.dstSubresource.baseArrayLayer = 0;
        imageCopy.dstSubresource.layerCount     = 1;
        imageCopy.dstOffset.x                   = 0;
        imageCopy.dstOffset.y                   = 0;
        imageCopy.dstOffset.z                   = 0;
        imageCopy.extent.width                  = backBufferWidth;
        imageCopy.extent.height                 = backBufferHeight;
        imageCopy.extent.depth                  = params->currentBackBuffer.description.depth;
        vkCmdCopyImage(commandBuffer, backbufferImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, renderTargetImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy);

        if (barrierCount > 0)
        {
            for (uint32_t i = 0; i < barrierCount; ++i)
                flipBarrier(barriers[i]);

            vkCmdPipelineBarrier(commandBuffer, dstStageMask, srcStageMask, 0, 0, nullptr, 0, nullptr, barrierCount, barriers);
        }
    }
    else
    {
        uint32_t viewOffset = s_uiCompositionDescriptorSetIndex * c_uiCompositionViewCount;
        // create views
        auto createView = [device, pAllocator](VkImage image, FfxSurfaceFormat format, uint32_t viewIndex) {
            vkDestroyImageView(device, s_uiCompositionImageViews[viewIndex], pAllocator);
            s_uiCompositionImageViews[viewIndex] = VK_NULL_HANDLE;

            VkImageViewCreateInfo viewCreateInfo           = {};
            viewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCreateInfo.pNext                           = nullptr;
            viewCreateInfo.flags                           = 0;
            viewCreateInfo.image                           = image;
            viewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format                          = ffxGetVkFormatFromSurfaceFormat(format);
            viewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT; // TODO: is it always the case?
            viewCreateInfo.subresourceRange.baseMipLevel   = 0;
            viewCreateInfo.subresourceRange.levelCount     = 1;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.layerCount     = 1;

            return vkCreateImageView(device, &viewCreateInfo, pAllocator, &s_uiCompositionImageViews[viewIndex]);
        };

        res = createView(backbufferImage, params->currentBackBuffer.description.format, viewOffset);
        FFX_BACKEND_API_ERROR_ON_VK_ERROR(res);
        res = createView(uiImage, params->currentUI.description.format, viewOffset + 1);
        FFX_BACKEND_API_ERROR_ON_VK_ERROR(res);
        res = createView(renderTargetImage, params->outputSwapChainBuffer.description.format, viewOffset + 2);
        FFX_BACKEND_API_ERROR_ON_VK_ERROR(res);

        // update the descriptor set
        VkDescriptorImageInfo imageInfos[2];
        VkWriteDescriptorSet  writes[2];
        for (uint32_t i = 0; i < 2; ++i)
        {
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[i].imageView   = s_uiCompositionImageViews[viewOffset + i];
            imageInfos[i].sampler     = VK_NULL_HANDLE;

            writes[i].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].pNext            = nullptr;
            writes[i].dstSet           = s_uiCompositionDescriptorSets[s_uiCompositionDescriptorSetIndex];
            writes[i].dstBinding       = i;
            writes[i].dstArrayElement  = 0;
            writes[i].descriptorCount  = 1;
            writes[i].descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            writes[i].pImageInfo       = &imageInfos[i];
            writes[i].pBufferInfo      = nullptr;
            writes[i].pTexelBufferView = nullptr;
        }
        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

        // create framebuffer
        if (s_uiCompositionFramebuffers[s_uiCompositionDescriptorSetIndex] != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, s_uiCompositionFramebuffers[s_uiCompositionDescriptorSetIndex], pAllocator);
            s_uiCompositionFramebuffers[s_uiCompositionDescriptorSetIndex] = VK_NULL_HANDLE;
        }

        VkFramebufferCreateInfo framebufferCreateInfo = {};
        framebufferCreateInfo.sType                   = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext                   = nullptr;
        framebufferCreateInfo.flags                   = 0;
        framebufferCreateInfo.renderPass              = s_uiCompositionRenderPass;
        framebufferCreateInfo.attachmentCount         = 1;
        framebufferCreateInfo.pAttachments            = &s_uiCompositionImageViews[viewOffset + 2];
        framebufferCreateInfo.width                   = renderTargetWidth;
        framebufferCreateInfo.height                  = renderTargetHeight;
        framebufferCreateInfo.layers                  = 1;
        res = vkCreateFramebuffer(device, &framebufferCreateInfo, pAllocator, &s_uiCompositionFramebuffers[s_uiCompositionDescriptorSetIndex]);
        FFX_BACKEND_API_ERROR_ON_VK_ERROR(res);

        VkImageMemoryBarrier barriers[3];
        uint32_t             barrierCount = 0;
        VkPipelineStageFlags srcStageMask = 0;
        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        setBarrier(barriers, barrierCount, backbufferImage,   params->currentBackBuffer.state,     VK_ACCESS_SHADER_READ_BIT,            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, srcStageMask);
        setBarrier(barriers, barrierCount, uiImage,           params->currentUI.state,             VK_ACCESS_SHADER_READ_BIT,            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, srcStageMask);
        setBarrier(barriers, barrierCount, renderTargetImage, params->outputSwapChainBuffer.state, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, srcStageMask);

        if (barrierCount > 0)
            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, barrierCount, barriers);
        
        VkRenderPassBeginInfo beginInfo    = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.pNext                    = nullptr;
        beginInfo.renderPass               = s_uiCompositionRenderPass;
        beginInfo.framebuffer              = s_uiCompositionFramebuffers[s_uiCompositionDescriptorSetIndex];
        beginInfo.renderArea.offset.x      = 0;
        beginInfo.renderArea.offset.y      = 0;
        beginInfo.renderArea.extent.width  = renderTargetWidth;
        beginInfo.renderArea.extent.height = renderTargetHeight;
        beginInfo.clearValueCount          = 0;
        beginInfo.pClearValues             = nullptr;
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

        if (params->usePremulAlpha)
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_uiCompositionPremulPipeline);
        else
            vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, s_uiCompositionPipeline);

        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                s_uiCompositionPipelineLayout,
                                0,
                                1,
                                &s_uiCompositionDescriptorSets[s_uiCompositionDescriptorSetIndex],
                                0,
                                nullptr);

        VkViewport viewport;
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(renderTargetWidth);
        viewport.height   = static_cast<float>(renderTargetHeight);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor;
        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = renderTargetWidth;
        scissor.extent.height = renderTargetHeight;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        if (barrierCount > 0)
        {
            for (uint32_t i = 0; i < barrierCount; ++i)
                flipBarrier(barriers[i]);

            vkCmdPipelineBarrier(commandBuffer, dstStageMask, srcStageMask, 0, 0, nullptr, 0, nullptr, barrierCount, barriers);
        }
        s_uiCompositionDescriptorSetIndex = (s_uiCompositionDescriptorSetIndex + 1) % c_uiCompositionRingBufferSize;
    }

    return FFX_OK;
}
