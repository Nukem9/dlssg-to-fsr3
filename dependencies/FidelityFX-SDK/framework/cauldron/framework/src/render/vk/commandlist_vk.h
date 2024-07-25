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

#if defined(_VK)

#include "render/commandlist.h"
#include <vulkan/vulkan.h>

namespace cauldron
{
    class Device;

    struct CommandListInitParams
    {
        Device*         pDevice;
        VkCommandPool   pool;
    };

    class CommandListInternal final : public CommandList
    {
    public:
        virtual ~CommandListInternal();

        VkCommandPool VKCmdPool() { return m_CommandPool; }
        const VkCommandPool VKCmdPool() const { return m_CommandPool; }

        VkCommandBuffer VKCmdBuffer() { return m_CommandBuffer; }
        const VkCommandBuffer VKCmdBuffer() const { return m_CommandBuffer; }

        virtual const CommandListInternal* GetImpl() const override { return this; }
        virtual CommandListInternal* GetImpl() override { return this; }

    private:
        friend class CommandList;
        CommandListInternal(Device* pDevice, CommandQueue queueType, VkCommandPool pool, const wchar_t* name = nullptr);
        CommandListInternal(CommandQueue queueType, VkCommandBuffer cmdBuffer, const wchar_t* name = nullptr); // TODO: remove this hack only used by FSR3
        CommandListInternal() = delete;

        // Internal members
        VkDevice        m_Device        = VK_NULL_HANDLE;
        VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
        VkCommandPool   m_CommandPool   = VK_NULL_HANDLE;
    };

    class UploadContextInternal final : public UploadContext
    {
    public:
        virtual ~UploadContextInternal();

        void Execute() override;

        const UploadContextInternal* GetImpl() const override { return this; }
        UploadContextInternal* GetImpl() override { return this; }

        CommandList* GetCopyCmdList() { return m_pCopyCommandList; }
        CommandList* GetGraphicsCmdList() { return m_pGraphicsCommandList; }

        bool& HasGraphicsCmdList() { return m_HasGraphicsCommands; }

    private:
        friend class UploadContext;
        UploadContextInternal();

        // Internal members
        CommandList*    m_pCopyCommandList = nullptr;
        CommandList*    m_pGraphicsCommandList = nullptr;
        bool            m_HasGraphicsCommands = false;
    };

} // namespace cauldron

#endif // #if defined(_VK)
