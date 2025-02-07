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

#include "glsl_compiler.h"
#include "utils.h"

#include <md5.h>
#include <spirv_reflect.h>

std::string MD5HashString(unsigned char* sig)
{
    char out[33];
    out[32] = '\0';

    char* out_ptr = out;
    std::stringstream ss;

    for (int i = 0; i < MD5_SIZE; i++)
    {
        std::snprintf(out_ptr, 32, "%02x", sig[i]);
        out_ptr += 2;
    }

    return std::string(out);
}

std::string GetMD5HashDigest(void* buffer, size_t size)
{
    unsigned char sig[MD5_SIZE];

    md5::md5_t md5;

    md5.process(buffer, size);

    md5.finish(sig);

    return MD5HashString(sig);
}

uint8_t* GLSLShaderBinary::BufferPointer()
{
    return spirv.data();
}

size_t GLSLShaderBinary::BufferSize()
{
    return spirv.size();
}

GLSLCompiler::GLSLCompiler(const std::string& glslangExe,
                           const std::string& shaderPath,
                           const std::string& shaderName,
                           const std::string& shaderFileName,
                           const std::string& outputPath,
                           bool               disableLogs,
                           bool               debugCompile)
    : ICompiler(shaderPath, shaderName, shaderFileName, outputPath, disableLogs, debugCompile)
    , m_GlslangExe(glslangExe.empty() ? "glslangValidator.exe" : glslangExe)
{
    fs::create_directory(m_OutputPath + "/" + m_ShaderName + "_temp");
}

GLSLCompiler::~GLSLCompiler()
{
    fs::remove_all(m_OutputPath + "/" + m_ShaderName + "_temp");
}

static bool FindIncludeFilePath(const std::string& includeFile, const std::vector<fs::path>& includeSearchPaths, fs::path& includeFilePath)
{
    fs::path localPath = includeFile;
    if (fs::exists(localPath))
    {
        includeFilePath = fs::absolute(localPath);
        return true;
    }

    for (const auto& searchPath : includeSearchPaths)
    {
        includeFilePath = fs::absolute(searchPath / localPath);
        if (fs::exists(includeFilePath))
            return true;
    }

    return false;
}

static void CollectDependencies(const std::string& shaderPath, const std::vector<fs::path>& includeSearchPaths, std::unordered_set<std::string>& dependencies)
{
    std::ifstream ifileStream(shaderPath);

    if (!ifileStream.is_open())
        return;

    auto findNonWhiteSpace = [](const std::string& line, size_t startIndex) -> size_t {
        for (size_t i = startIndex; i < line.size(); ++i)
        {
            if (!std::isspace(line[i]))
                return i;
        }

        return std::string::npos;
    };

    std::string line;
    while (std::getline(ifileStream, line))
    {
        size_t nwsIndex = findNonWhiteSpace(line, 0u);
        if (nwsIndex != std::string::npos)
        {
            // If the first non-whitespace text is "#include" then find the absolute path to the specified file.
            if (nwsIndex != line.find("#include"))
                continue;

            nwsIndex = findNonWhiteSpace(line, nwsIndex + 8u);
            if (nwsIndex == std::string::npos)
                continue;

            // If the first non-whitespace after the #include is a quote character then find the close quote char.
            if (line[nwsIndex] == '"' || line[nwsIndex] == '<')
            {
                char closeQuoteChar = '"';
                if (line[nwsIndex] == '<')
                    closeQuoteChar = '>';
                size_t closeQuoteIndex = line.find(closeQuoteChar, nwsIndex + 1u);
                if (closeQuoteIndex != std::string::npos)
                {
                    std::string includeFile(&line[nwsIndex + 1u], closeQuoteIndex - (nwsIndex + 1u));
                    fs::path includeFilePath;
                    if (FindIncludeFilePath(includeFile, includeSearchPaths, includeFilePath))
                    {
                        dependencies.insert(includeFilePath.generic_string());

                        CollectDependencies(includeFilePath.generic_string(), includeSearchPaths, dependencies);
                    }
                }
            }
        }
    }
}

bool GLSLCompiler::GLSLCompiler::Compile(Permutation& permutation, const std::vector<std::string>& arguments, std::mutex& writeMutex)
{
    GLSLShaderBinary* glslShaderBinary = new GLSLShaderBinary();

    permutation.shaderBinary = std::shared_ptr<GLSLShaderBinary>(glslShaderBinary);

    bool compileSuccessful = false;

    struct ErrorData
    {
        std::string error;
        int lineNumber = -1;
    };
    std::vector<ErrorData> errors;

    // ------------------------------------------------------------------------------------------------
    // Assemble command line arguments
    // ------------------------------------------------------------------------------------------------

    std::string cmdLine = m_GlslangExe + " ";

    if (m_DebugCompile)
    {
        cmdLine += "-g -gVS -Od ";
    }

    std::vector<fs::path> includeSearchPaths;
    for (int i = 0; i < arguments.size(); i++)
    {
        if (arguments[i][0] == '-' && arguments[i][1] == 'I')
        {
            cmdLine += "\"" + arguments[i] + "\"";
            includeSearchPaths.push_back(&(arguments[i][2]));
        }
        else
        {
            cmdLine += arguments[i];
        }

        if (!(arguments[i][0] == '-' && arguments[i][1] == 'D'))
            cmdLine += " ";
    }

    // Our code for collecting shader dependencies is not smart enough to deal with the possibility that each permutation
    // might have different #include files, so we only need to collect them once and then reuse them for each permutation.
    writeMutex.lock();
    if (!m_ShaderDependenciesCollected)
    {
        m_ShaderDependenciesCollected = true;
        CollectDependencies(m_ShaderPath, includeSearchPaths, m_ShaderDependencies);
    }
    writeMutex.unlock();

    // ------------------------------------------------------------------------------------------------
    // Create temporary SPIRV name
    // ------------------------------------------------------------------------------------------------

    std::string tempFilePath = m_OutputPath + "/" + m_ShaderName + "_temp/" + std::to_string(permutation.key) + ".spv";

    cmdLine += "-o \"" + tempFilePath + "\" \"" + m_ShaderPath + "\"";

    // ------------------------------------------------------------------------------------------------
    // Launch process and compile SPIRV using glslangValidator
    // ------------------------------------------------------------------------------------------------

    const auto func = [&](const char* bytes, size_t n) {

        std::stringstream ss(std::string(bytes, n));

        std::string token;

        while (std::getline(ss, token, '\n'))
        {
            if (token != "\r") // avoid carriage return
            {
                // parse file / line info
                int lineNumber = -1;
                if (token.rfind("ERROR: ", 0) == 0)
                {
                    token.erase(0, 7);
                }
                if (token.rfind(m_ShaderPath, 0) == 0)
                {
                    size_t begin = m_ShaderPath.size() + 1;
                    if (token[begin - 1u] == ':')
                    {
                        size_t end = token.find(':', begin);

                        std::string lineNumberString(token.begin() + begin, token.begin() + end);
                        lineNumber = std::stoi(lineNumberString);

                        token.erase(0, end + 2);
                    }
                }
                token.pop_back();
                errors.push_back(ErrorData{token, lineNumber});
            }
        }
    };

    tpl::Process process(cmdLine, "", func, func);

    bool succeeded = process.get_exit_status() == 0;

    if (!m_DisableLogs && errors.size() > 1)
    {
        writeMutex.lock();

        fprintf(stderr, "%s[%lu]\n", m_ShaderFileName.c_str(), permutation.key);

        for (size_t i = 1; i < errors.size(); i++)
        {
            if (errors[i].lineNumber > -1)
            {
                fprintf(stderr, "%s(%d) : glslangValidator error : %s\n", m_ShaderPath.c_str(), errors[i].lineNumber, errors[i].error.c_str());
            }
            else
            {
                fprintf(stderr, "%s\n", errors[i].error.c_str());
            }
        }

        writeMutex.unlock();
    }

    if (succeeded)
    {
        // ------------------------------------------------------------------------------------------------
        // Read temporary SPIRV blob from disk
        // ------------------------------------------------------------------------------------------------

        std::ifstream file(tempFilePath, std::ios::ate | std::ios::binary);

        if (!file.is_open())
            throw std::runtime_error("Failed to open SPIRV file!");

        size_t fileSize = (size_t)file.tellg();
        glslShaderBinary->spirv.resize(fileSize);

        file.seekg(0);
        file.read((char*)glslShaderBinary->spirv.data(), fileSize);

        file.close();

        // ------------------------------------------------------------------------------------------------
        // Generate hash for SPIRV
        // ------------------------------------------------------------------------------------------------

        permutation.hashDigest     = GetMD5HashDigest(glslShaderBinary->BufferPointer(), glslShaderBinary->BufferSize());
        permutation.name           = m_ShaderName + "_" + permutation.hashDigest;
        permutation.headerFileName = permutation.name + ".h";
    }

    permutation.dependencies = m_ShaderDependencies;

    return succeeded;
}

bool GLSLCompiler::ExtractReflectionData(Permutation& permutation)
{
    GLSLShaderBinary*   glslShaderBinary   = dynamic_cast<GLSLShaderBinary*>(permutation.shaderBinary.get());
    IReflectionData* glslReflectionData = new IReflectionData();

    permutation.reflectionData = std::shared_ptr<IReflectionData>(glslReflectionData);

    SpvReflectShaderModule reflectShaderModule;

    SpvReflectResult result = spvReflectCreateShaderModule(glslShaderBinary->BufferSize(), glslShaderBinary->BufferPointer(), &reflectShaderModule);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    uint32_t count = 0;
    result = spvReflectEnumerateDescriptorSets(&reflectShaderModule, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectDescriptorSet*> sets(count);
    result = spvReflectEnumerateDescriptorSets(&reflectShaderModule, &count, sets.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    for (int setIdx = 0; setIdx < sets.size(); setIdx++)
    {
        SpvReflectDescriptorSet* ds = sets[setIdx];

        for (int bindingIdx = 0; bindingIdx < ds->binding_count; bindingIdx++)
        {
            SpvReflectDescriptorBinding* binding = ds->bindings[bindingIdx];
            ShaderResourceInfo resourceInfo = {binding->name ? binding->name : "", binding->binding, binding->count, ds->set};

            switch (binding->descriptor_type)
            {
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                glslReflectionData->constantBuffers.push_back(resourceInfo);
                break;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                glslReflectionData->srvTextures.push_back(resourceInfo);
                break;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                glslReflectionData->samplers.push_back(resourceInfo);
                break;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                glslReflectionData->uavTextures.push_back(resourceInfo);
                break;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                if (binding->resource_type == SPV_REFLECT_RESOURCE_FLAG_SRV)
                    glslReflectionData->srvBuffers.push_back(resourceInfo);
                else
                    glslReflectionData->uavBuffers.push_back(resourceInfo);
                break;
            case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                glslReflectionData->rtAccelerationStructures.push_back(resourceInfo);
                break;

            case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            default:
                throw std::runtime_error("Shader uses an unsupported resource type!");
                break;
            }
        }
    }

    spvReflectDestroyShaderModule(&reflectShaderModule);

    return true;
}

void GLSLCompiler::WriteBinaryHeaderReflectionData(FILE* fp, const Permutation& permutation, std::mutex& writeMutex)
{
    IReflectionData* glslReflectionData = dynamic_cast<IReflectionData*>(permutation.reflectionData.get());

    const auto WriteResourceInfo =
        [](FILE* fp, const std::string& permutationName, const std::vector<ShaderResourceInfo>& resourceInfo, const std::string& resourceTypeString) {
            if (resourceInfo.size() > 0)
            {
                fprintf(fp, "static const char* g_%s_%sResourceNames[] = { ", permutationName.c_str(), resourceTypeString.c_str());

                for (int j = 0; j < resourceInfo.size(); j++)
                {
                    const ShaderResourceInfo& info = resourceInfo[j];

                    fprintf(fp, " \"%s\",", info.name.c_str());
                }

                fprintf(fp, " };\n");

                fprintf(fp, "static const uint32_t g_%s_%sResourceBindings[] = { ", permutationName.c_str(), resourceTypeString.c_str());

                for (int j = 0; j < resourceInfo.size(); j++)
                {
                    const ShaderResourceInfo& info = resourceInfo[j];

                    fprintf(fp, " %i,", info.binding);
                }

                fprintf(fp, " };\n");

                fprintf(fp, "static const uint32_t g_%s_%sResourceCounts[] = { ", permutationName.c_str(), resourceTypeString.c_str());

                for (int j = 0; j < resourceInfo.size(); j++)
                {
                    const ShaderResourceInfo& info = resourceInfo[j];

                    fprintf(fp, " %i,", info.count);
                }

                fprintf(fp, " };\n");

                fprintf(fp, "static const uint32_t g_%s_%sResourceSets[] = { ", permutationName.c_str(), resourceTypeString.c_str());

                for (int j = 0; j < resourceInfo.size(); j++)
                {
                    const ShaderResourceInfo& info = resourceInfo[j];

                    fprintf(fp, " %i,", info.space);
                }

                fprintf(fp, " };\n\n");
            }
        };

    // CBVs
    WriteResourceInfo(fp, permutation.name, glslReflectionData->constantBuffers, "CBV");

    // SRV Textures
    WriteResourceInfo(fp, permutation.name, glslReflectionData->srvTextures, "TextureSRV");

    // UAV Textures
    WriteResourceInfo(fp, permutation.name, glslReflectionData->uavTextures, "TextureUAV");

    // SRV Buffers
    WriteResourceInfo(fp, permutation.name, glslReflectionData->srvBuffers, "BufferSRV");

    // UAV Buffers
    WriteResourceInfo(fp, permutation.name, glslReflectionData->uavBuffers, "BufferUAV");

    // Sampler
    WriteResourceInfo(fp, permutation.name, glslReflectionData->samplers, "Sampler");

    // RT Acceleration Structure
    WriteResourceInfo(fp, permutation.name, glslReflectionData->rtAccelerationStructures, "RTAccelerationStructure");
}

void GLSLCompiler::WritePermutationHeaderReflectionStructMembers(FILE* fp)
{
    fprintf(fp, "\n");
    fprintf(fp, "    const uint32_t  numConstantBuffers;\n");
    fprintf(fp, "    const char**    constantBufferNames;\n");
    fprintf(fp, "    const uint32_t* constantBufferBindings;\n");
    fprintf(fp, "    const uint32_t* constantBufferCounts;\n");
    fprintf(fp, "    const uint32_t* constantBufferSpaces;\n");
    fprintf(fp, "\n");
    fprintf(fp, "    const uint32_t  numSRVTextures;\n");
    fprintf(fp, "    const char**    srvTextureNames;\n");
    fprintf(fp, "    const uint32_t* srvTextureBindings;\n");
    fprintf(fp, "    const uint32_t* srvTextureCounts;\n");
    fprintf(fp, "    const uint32_t* srvTextureSpaces;\n");
    fprintf(fp, "\n");
    fprintf(fp, "    const uint32_t  numUAVTextures;\n");
    fprintf(fp, "    const char**    uavTextureNames;\n");
    fprintf(fp, "    const uint32_t* uavTextureBindings;\n");
    fprintf(fp, "    const uint32_t* uavTextureCounts;\n");
    fprintf(fp, "    const uint32_t* uavTextureSpaces;\n");
    fprintf(fp, "\n");
    fprintf(fp, "    const uint32_t  numSRVBuffers;\n");
    fprintf(fp, "    const char**    srvBufferNames;\n");
    fprintf(fp, "    const uint32_t* srvBufferBindings;\n");
    fprintf(fp, "    const uint32_t* srvBufferCounts;\n");
    fprintf(fp, "    const uint32_t* srvBufferSpaces;\n");
    fprintf(fp, "\n");
    fprintf(fp, "    const uint32_t  numUAVBuffers;\n");
    fprintf(fp, "    const char**    uavBufferNames;\n");
    fprintf(fp, "    const uint32_t* uavBufferBindings;\n");
    fprintf(fp, "    const uint32_t* uavBufferCounts;\n");
    fprintf(fp, "    const uint32_t* uavBufferSpaces;\n");
    fprintf(fp, "\n");
    fprintf(fp, "    const uint32_t  numSamplers;\n");
    fprintf(fp, "    const char**    samplerNames;\n");
    fprintf(fp, "    const uint32_t* samplerBindings;\n");
    fprintf(fp, "    const uint32_t* samplerCounts;\n");
    fprintf(fp, "    const uint32_t* samplerSpaces;\n");
    fprintf(fp, "\n");
    fprintf(fp, "    const uint32_t  numRTAccelerationStructures;\n");
    fprintf(fp, "    const char**    rtAccelerationStructureNames;\n");
    fprintf(fp, "    const uint32_t* rtAccelerationStructureBindings;\n");
    fprintf(fp, "    const uint32_t* rtAccelerationStructureCounts;\n");
    fprintf(fp, "    const uint32_t* rtAccelerationStructureSpaces;\n");
}

void GLSLCompiler::WritePermutationHeaderReflectionData(FILE* fp, const Permutation& permutation)
{
    IReflectionData* glslReflectionData = dynamic_cast<IReflectionData*>(permutation.reflectionData.get());

    const auto WriteResourceInfo = [](FILE* fp, const int& numResources, const std::string& permutationName, const std::string& resourceTypeString) {
        if (numResources == 0)
        {
            fprintf(fp, "0, 0, 0, 0, 0, ");
        }
        else
        {
            fprintf(fp,
                    "%i, g_%s_%sResourceNames, g_%s_%sResourceBindings, g_%s_%sResourceCounts, g_%s_%sResourceSets, ",
                    numResources,
                    permutationName.c_str(),
                    resourceTypeString.c_str(),
                    permutationName.c_str(),
                    resourceTypeString.c_str(),
                    permutationName.c_str(),
                    resourceTypeString.c_str(),
                    permutationName.c_str(),
                    resourceTypeString.c_str());
        }
    };

    WriteResourceInfo(fp, glslReflectionData->constantBuffers.size(), permutation.name, "CBV");
    WriteResourceInfo(fp, glslReflectionData->srvTextures.size(), permutation.name, "TextureSRV");
    WriteResourceInfo(fp, glslReflectionData->uavTextures.size(), permutation.name, "TextureUAV");
    WriteResourceInfo(fp, glslReflectionData->srvBuffers.size(), permutation.name, "BufferSRV");
    WriteResourceInfo(fp, glslReflectionData->uavBuffers.size(), permutation.name, "BufferUAV");
    WriteResourceInfo(fp, glslReflectionData->samplers.size(), permutation.name, "Sampler");
    WriteResourceInfo(fp, glslReflectionData->rtAccelerationStructures.size(), permutation.name, "RTAccelerationStructure");
}
