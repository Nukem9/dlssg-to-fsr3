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

#include "compiler.h"


/// The GLSL (GSLang) specialization of <c><i>IShaderBinary</i></c> interface.
/// Handles everything necessary to export DXC compiled binary shader data.
///
/// @ingroup ShaderCompiler
struct GLSLShaderBinary : public IShaderBinary
{
    std::vector<uint8_t> spirv;             ///< spirv representation of the shader binary buffer

    /// GLSL Shader binary buffer accessor. 
    ///
    /// @returns
    /// Pointer to the internal GLSL buffer representation.
    ///
    /// @ingroup ShaderCompiler
    uint8_t* BufferPointer() override;

    /// Queries the GLSL shader binary size.
    ///
    /// @returns
    /// Size of the GLSL shader binary
    ///
    /// @ingroup ShaderCompiler
    size_t   BufferSize() override;
};


/// The GLSLCompiler specialization of <c><i>ICompiler</i></c> interface.
/// Handles everything necessary to compile and extract shader reflection data
/// for GSLS and then exports the binary and reflection data for consumption
/// by GLSL-specific backends.
///
/// @ingroup ShaderCompiler
class GLSLCompiler : public ICompiler
{
public:

    /// GLSL Compiler construction function
    /// 
    /// @param [in]  glslangExe         Path to the glslang exe to use to compile
    /// @param [in]  shaderPath         Path to the shader to compile
    /// @param [in]  shaderName         Shader entry point
    /// @param [in]  shaderFileName     Filename of the shader file to compile
    /// @param [in]  outputPath         Output path for shader export
    /// @param [in]  disableLogs        Enables/Disables logging of errors and warnings
    /// @param [in]  debugCompile       Compile shaders in debug and generate pdb information
    ///
    /// @returns
    /// none
    ///
    /// @ingroup ShaderCompiler
    GLSLCompiler(const std::string& glslangExe,
                 const std::string& shaderPath,
                 const std::string& shaderName,
                 const std::string& shaderFileName,
                 const std::string& outputPath,
                 bool               disableLogs,
                 bool               debugCompile);

    /// GLSL Compiler destruction function
    ///
    /// @returns
    /// none
    ///
    /// @ingroup ShaderCompiler
    ~GLSLCompiler();

    /// Compiles a GLSL shader permutation
    ///
    /// @param [in]  permutation            The permutation representation to compile
    /// @param [in]  arguments              List of arguments to pass to the compiler
    /// @param [in]  wrietMutex             Mutex to use for thread safety of compile process
    /// 
    /// @returns
    /// true if successful, false otherwise
    ///
    /// @ingroup ShaderCompiler
    bool Compile(Permutation& permutation, const std::vector<std::string>& arguments, std::mutex& writeMutex) override;

    /// Extracts GLSL shader reflection data
    ///
    /// @param [in]  permutation            The permutation representation to extract reflection for
    /// 
    /// @returns
    /// true if successful, false otherwise
    ///
    /// @ingroup ShaderCompiler
    bool ExtractReflectionData(Permutation& permutation) override;

    /// Writes GLSL reflection header data for shader permutations. 
    ///
    /// @param [in]  fp                     The file to write header information into
    /// @param [in]  permutation            The permutation representation to write to head
    /// @param [in]  wrietMutex             Mutex to use for thread safety of reflection data export
    /// 
    /// @returns
    /// none
    ///
    /// @ingroup ShaderCompiler
    void WriteBinaryHeaderReflectionData(FILE* fp, const Permutation& permutation, std::mutex& writeMutex) override;

    /// Writes GLSL permutation reflection header data structures for shader permutations. 
    ///
    /// @param [in]  fp                     The file to write header data structures into
    /// 
    /// @returns
    /// none
    ///
    /// @ingroup ShaderCompiler
    void WritePermutationHeaderReflectionStructMembers(FILE* fp) override;

    /// Writes GLSL permutation reflection header data for shader permutations. 
    ///
    /// @param [in]  fp                     The file to write header information into
    /// @param [in]  permutation            The permutation representation to write to head
    /// 
    /// @returns
    /// none
    ///
    /// @ingroup ShaderCompiler
    void WritePermutationHeaderReflectionData(FILE* fp, const Permutation& permutation) override;

private:
    std::string m_GlslangExe;
    std::unordered_set<std::string> m_ShaderDependencies;
    bool m_ShaderDependenciesCollected = false;
};
