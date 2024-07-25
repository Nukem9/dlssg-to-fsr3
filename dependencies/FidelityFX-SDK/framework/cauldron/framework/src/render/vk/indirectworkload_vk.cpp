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

#include "render/vk/indirectworkload_vk.h"
#include <vulkan/vulkan.h>

namespace cauldron
{
    IndirectWorkload* IndirectWorkload::CreateIndirectWorkload(const IndirectCommandType& type)
    {
        return new IndirectWorkloadInternal(type);
    }

    IndirectWorkloadInternal::IndirectWorkloadInternal(const IndirectCommandType& type)
        : m_type(type)
    {
        switch (m_type)
        {
        case IndirectCommandType::Draw:
            m_stride = sizeof(VkDrawIndirectCommand);
            break;
        case IndirectCommandType::DrawIndexed:
            m_stride = sizeof(VkDrawIndexedIndirectCommand);
            break;
        case IndirectCommandType::Dispatch:
            m_stride = sizeof(VkDispatchIndirectCommand);
            break;
        default:
            CauldronWarning(L"Unsupported command type for indirect workload.");
            return;
        }
    }

}  // namespace cauldron
