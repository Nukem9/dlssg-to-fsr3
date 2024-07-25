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

#include "render/renderdefines.h"

#include <xstring>
#include <map>
#include <vector>

namespace cauldron
{
    /// A structure representing shader build description information
    ///
    /// @ingroup CauldronRender
    struct ShaderBuildDesc
    {
        const wchar_t* ShaderCode = nullptr;            ///< Shader source (can be string source or a file path).
        const wchar_t* EntryPoint = nullptr;            ///< Shader entry point name.
        const wchar_t* AdditionalParams = nullptr;      ///< Shader additional params.
        ShaderStage    Stage = ShaderStage::Compute;    ///< Shader stage (defaults to ShaderStage::Compute).
        ShaderModel    Model = ShaderModel::SM6_0;      ///< Shader model to use (defaults to ShaderModel::SM6_0).
        DefineList     Defines;                         ///< Shader defines to use in shader compiling.

        /// Convenience function to build a vertex shader build description
        ///
        static inline ShaderBuildDesc Vertex(const wchar_t* shaderCode, const wchar_t* entryPoint, ShaderModel model = ShaderModel::SM6_0, DefineList* pDefines = nullptr)
        {
            ShaderBuildDesc desc;
            desc.ShaderCode = shaderCode;
            desc.EntryPoint = entryPoint;
            desc.Stage = ShaderStage::Vertex;
            desc.Model = model;
            
            if (pDefines)
                desc.Defines = *pDefines;

            return desc;
        }

        /// Convenience function to build a pixel shader build description
        ///
        static inline ShaderBuildDesc Pixel(const wchar_t* shaderCode, const wchar_t* entryPoint, ShaderModel model = ShaderModel::SM6_0, DefineList* pDefines = nullptr)
        {
            ShaderBuildDesc desc;
            desc.ShaderCode = shaderCode;
            desc.EntryPoint = entryPoint;
            desc.Stage = ShaderStage::Pixel;
            desc.Model = model;
            
            if (pDefines)
                desc.Defines = *pDefines;

            return desc;
        }

        /// Convenience function to build a compute shader build description
        ///
        static inline ShaderBuildDesc Compute(const wchar_t* shaderCode, const wchar_t* entryPoint, ShaderModel model = ShaderModel::SM6_0, DefineList* pDefines = nullptr)
        {
            ShaderBuildDesc desc;
            desc.ShaderCode = shaderCode;
            desc.EntryPoint = entryPoint;
            desc.Stage = ShaderStage::Compute;
            desc.Model = model;
            
            if (pDefines)
                desc.Defines = *pDefines;

            return desc;
        }
    };

    /// A structure representing shader blob description information
    ///
    /// @ingroup CauldronRender
    struct ShaderBlobDesc
    {
        const void* pData    = nullptr;                 ///< The shader binary to create the shader with.
        uint64_t    DataSize = 0;                       ///< The size of the shader binary.
        ShaderStage Stage    = ShaderStage::Compute;    ///< The stage of the shader to build (defaults to ShaderStage::Compute).

        /// Convenience function to build a vertex shader blob build description
        ///
        static inline ShaderBlobDesc Vertex(const void* blobData, const uint64_t blobSize)
        {
            ShaderBlobDesc desc = { blobData, blobSize, ShaderStage::Vertex };
            return desc;
        }

        /// Convenience function to build a pixel shader blob build description
        ///
        static inline ShaderBlobDesc Pixel(const void* blobData, const uint64_t blobSize)
        {
            ShaderBlobDesc desc = { blobData, blobSize, ShaderStage::Pixel };
            return desc;
        }

        /// Convenience function to build a compute shader blob build description
        ///
        static inline ShaderBlobDesc Compute(const void* blobData, const uint64_t blobSize)
        {
            ShaderBlobDesc desc = { blobData, blobSize, ShaderStage::Compute };
            return desc;
        }
    };
    
    /// Initializes the shader compilation system.
    ///
    /// @ingroup CauldronRender
    int InitShaderCompileSystem();

    /// Terminates the shader compilation system.
    ///
    /// @ingroup CauldronRender
    void TerminateShaderCompileSystem();

    /// Compiles the shader description to byte code.
    ///
    /// @ingroup CauldronRender
    void* CompileShaderToByteCode(const ShaderBuildDesc& shaderDesc, std::vector<const wchar_t*>* pAdditionalParameters = nullptr);

} // namespace cauldron
