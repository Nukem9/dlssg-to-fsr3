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

#include "render/gpuresource.h"
#include "render/resourceview.h"
#include "core/framework.h"

namespace cauldron
{
    bool IsSRGB(ResourceFormat format)
    {
        switch (format)
        {
        case ResourceFormat::RGBA8_SRGB:
        case ResourceFormat::BC1_SRGB:
        case ResourceFormat::BC2_SRGB:
        case ResourceFormat::BC3_SRGB:
        case ResourceFormat::BC7_SRGB:
            return true;
        }

        return false;
    }

    bool IsDepth(ResourceFormat format)
    {
        return (format == ResourceFormat::D16_UNORM
            || format == ResourceFormat::D32_FLOAT);
    }

    ResourceFormat ToGamma(ResourceFormat format)
    {
        switch (format)
        {
        case ResourceFormat::RGBA8_UNORM: return ResourceFormat::RGBA8_SRGB;
        case ResourceFormat::BC1_UNORM:   return ResourceFormat::BC1_SRGB;
        case ResourceFormat::BC2_UNORM:   return ResourceFormat::BC2_SRGB;
        case ResourceFormat::BC3_UNORM:   return ResourceFormat::BC3_SRGB;
        case ResourceFormat::BC7_UNORM:   return ResourceFormat::BC7_SRGB;
        }

        return format;
    }

    ResourceFormat FromGamma(ResourceFormat format)
    {
        switch (format)
        {
        case ResourceFormat::RGBA8_SRGB: return ResourceFormat::RGBA8_UNORM;
        case ResourceFormat::BC1_SRGB:   return ResourceFormat::BC1_UNORM;
        case ResourceFormat::BC2_SRGB:   return ResourceFormat::BC2_UNORM;
        case ResourceFormat::BC3_SRGB:   return ResourceFormat::BC3_UNORM;
        case ResourceFormat::BC7_SRGB:   return ResourceFormat::BC7_UNORM;
        }

        return format;
    }

    uint32_t GetResourceFormatStride(ResourceFormat format)
    {
        switch (format)
        {
        case ResourceFormat::Unknown:
            return 0;
        default:
            CauldronCritical(L"Unsupported Resource Format requested! Aborting");
            return 0;

            // 8-bit
        case ResourceFormat::R8_SINT:
        case ResourceFormat::R8_UINT:
            return 1;

            // 16-bit
        case ResourceFormat::R16_SINT:;
        case ResourceFormat::R16_UINT:
        case ResourceFormat::R16_FLOAT:
        case ResourceFormat::RG8_SINT:
        case ResourceFormat::RG8_UINT:
        case ResourceFormat::RG8_UNORM:
        case ResourceFormat::D16_UNORM:
            return 2;

            // 32-bit
        case ResourceFormat::R32_SINT:
        case ResourceFormat::R32_UINT:
        case ResourceFormat::RGBA8_SINT:
        case ResourceFormat::RGBA8_UINT:
        case ResourceFormat::RGBA8_UNORM:
        case ResourceFormat::RGBA8_SNORM:
        case ResourceFormat::RGBA8_SRGB:
        case ResourceFormat::RGB10A2_UNORM:
        case ResourceFormat::RG16_SINT:
        case ResourceFormat::RG16_UINT:
        case ResourceFormat::RG16_FLOAT:
        case ResourceFormat::R32_FLOAT:
        case ResourceFormat::D32_FLOAT:
            return 4;

            // 64-bit
        case ResourceFormat::RGBA16_SINT:
        case ResourceFormat::RGBA16_UINT:
        case ResourceFormat::RGBA16_UNORM:
        case ResourceFormat::RGBA16_SNORM:
        case ResourceFormat::RGBA16_FLOAT:
        case ResourceFormat::RG32_SINT:
        case ResourceFormat::RG32_UINT:
        case ResourceFormat::RG32_FLOAT:
            return 8;

            // 96-bit
        case ResourceFormat::RGB32_SINT:
        case ResourceFormat::RGB32_UINT:
        case ResourceFormat::RGB32_FLOAT:
            return 12;

            // 128-bit
        case ResourceFormat::RGBA32_SINT:
        case ResourceFormat::RGBA32_UINT:
        case ResourceFormat::RGBA32_FLOAT:
        case ResourceFormat::RGBA32_TYPELESS:
            return 16;


            // compressed formats return a stride for a 4x4 pixel block
            // see https://docs.microsoft.com/en-us/windows/win32/direct3d11/texture-block-compression-in-direct3d-11
        case cauldron::ResourceFormat::BC1_UNORM:
        case cauldron::ResourceFormat::BC1_SRGB:
        case cauldron::ResourceFormat::BC4_UNORM:
        case cauldron::ResourceFormat::BC4_SNORM:
            return 8;

        case cauldron::ResourceFormat::BC2_UNORM:
        case cauldron::ResourceFormat::BC2_SRGB:
        case cauldron::ResourceFormat::BC3_UNORM:
        case cauldron::ResourceFormat::BC3_SRGB:
        case cauldron::ResourceFormat::BC5_UNORM:
        case cauldron::ResourceFormat::BC5_SNORM:
        case cauldron::ResourceFormat::BC7_UNORM:
        case cauldron::ResourceFormat::BC7_SRGB:
            return 16;
        };
    }

    ResourceState GPUResource::GetCurrentResourceState(uint32_t subResource) const
    {
        if (subResource == 0xffffffff)
            return m_CurrentStates.front();
        CauldronAssert(ASSERT_CRITICAL, subResource < m_CurrentStates.size(), L"Trying to get state of sub-resource out of range!");
        return m_CurrentStates.at(subResource);
    }

    void GPUResource::SetCurrentResourceState(ResourceState newState, uint32_t subResource)
    {
        if (subResource == 0xffffffff)
            std::fill(m_CurrentStates.begin(), m_CurrentStates.end(), newState);
        else
        {
            CauldronAssert(ASSERT_CRITICAL, subResource < m_CurrentStates.size(), L"Trying to get state of sub-resource out of range!");
            m_CurrentStates.at(subResource) = newState;
        }
    }

    GPUResource::GPUResource(const wchar_t* resourceName, void* pOwner, ResourceState initialState, bool resizable)
        :
        m_Name(resourceName),
        m_Resizable(resizable),
        m_pOwner(pOwner)
    {
        // Insert only whole resource state, for sub-resources call InitSubResourceCount()
        m_CurrentStates.emplace_back(initialState);
    }

    void GPUResource::InitSubResourceCount(uint32_t subResourceCount)
    {
        CauldronAssert(ASSERT_CRITICAL, subResourceCount < 0xffffffff && subResourceCount > 0, L"Wrong number of sub-resources!");

        m_CurrentStates.resize(subResourceCount);
        if (subResourceCount > 1)
            std::fill(m_CurrentStates.begin() + 1, m_CurrentStates.end(), m_CurrentStates.front());
    }

} // namespace cauldron
