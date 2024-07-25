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

#if defined(_WIN)
#include "render/shaderbuilder.h"

#include "core/framework.h"
#include "misc/assert.h"
#include "misc/fileio.h"

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING    // To avoid receiving deprecation error since we are using C++11 only
#include <experimental/filesystem>
using namespace std::experimental;
#include <sstream>

#include <wrl.h>
#include "dxc/inc/dxcapi.h"
using namespace Microsoft::WRL;

namespace
{

} // unnamed namespace

namespace cauldron
{
    //////////////////////////////////////////////////////////////////////////
    // Helpers

    DxcCreateInstanceProc g_DXCCreateFunc;

    interface IncludeHandler : public IDxcIncludeHandler
    {
        IDxcUtils* m_pUtils = nullptr;
    public:
        IncludeHandler(IDxcUtils* pUtils) : m_pUtils(pUtils) {}
        HRESULT QueryInterface(const IID&, void**) { return S_OK; }
        ULONG AddRef() { return 0; }
        ULONG Release() { return 0; }
        HRESULT LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource)
        {
            filesystem::path file = filesystem::current_path();
            file.append(L"shaders");
            file.append(pFilename);
            bool fileExists = filesystem::exists(file);

            CauldronAssert(ASSERT_ERROR, fileExists, L"Could not find include file for reading %ls", pFilename);
            if (!fileExists)
                return E_FAIL;

            // Dump the include file code to string
            std::wstring includeCodeString;
            {
                int64_t fileSize = GetFileSize(file.c_str());
                CauldronAssert(ASSERT_ERROR, fileSize > 0, L"Error getting file size for include file %ls", file.c_str());
                if (fileSize > 0)
                {
                    // Read the file directly to the string
                    std::string includeString;
                    includeString.resize(fileSize);

                    int64_t sizeRead = ReadFileAll(file.c_str(), const_cast<void*>(reinterpret_cast<const void*>(includeString.c_str())), fileSize);
                    CauldronAssert(ASSERT_ERROR, sizeRead == fileSize, L"Error reading include file %ls", file.c_str());
                    includeCodeString = StringToWString(includeString);
                }
            }

            IDxcBlobEncoding* includeCode;
            CauldronThrowOnFail(m_pUtils->CreateBlob(includeCodeString.c_str(), static_cast<UINT32>(includeCodeString.length() * sizeof(wchar_t)), DXC_CP_UTF16, &includeCode));

            *ppIncludeSource = includeCode;
            return S_OK;
        }
    };

    void ParseStringToShaderCode(const wchar_t* pShaderString, const DefineList* pDefines, std::wstring& shaderCodeOutput)
    {
        // Make sure something was passed in
        size_t length = wcslen(pShaderString);
        CauldronAssert(ASSERT_ERROR, length > 0, L"Can't parse an empty string to shader code");
        if (!length)
            return;

        // Start by putting in all the of the defines
        std::wostringstream shaderStream;
        if (pDefines)
        {
            for (auto defineIt = pDefines->begin(); defineIt != pDefines->end(); ++defineIt)
                shaderStream << L"#define " << defineIt->first.c_str() << L" " << defineIt->second.c_str() << L"\n";
        }

        // Now append the actual shader
        shaderStream << pShaderString;

        // Write out everything to the ShaderCode output
        shaderCodeOutput = shaderStream.str();
    }

    void ParseFileToShaderCode(const wchar_t* pFilePath, const DefineList* pDefines, std::wstring& shaderCodeOutput)
    {
        // Dump the shader code to string and pass everything off to the string function to put everything together
        std::wstring shaderString;
        {
            int64_t fileSize = GetFileSize(pFilePath);
            CauldronAssert(ASSERT_ERROR, fileSize > 0, L"Error getting file size for include file %ls", pFilePath);
            if (fileSize > 0)
            {
                // Read the file directly to the string
                std::string shaderCodeString;
                shaderCodeString.resize(fileSize);

                int64_t sizeRead = ReadFileAll(pFilePath, const_cast<void*>(reinterpret_cast<const void*>(shaderCodeString.c_str())), fileSize);
                CauldronAssert(ASSERT_ERROR, sizeRead == fileSize, L"Error reading include file %ls", pFilePath);
                shaderString = StringToWString(shaderCodeString);
            }
        }

        ParseStringToShaderCode(shaderString.c_str(), pDefines, shaderCodeOutput);
    }

    //////////////////////////////////////////////////////////////////////////
    // Shaderbuilder

    void* CompileShaderToByteCode(const ShaderBuildDesc& shaderDesc, std::vector<const wchar_t*>* pAdditionalParameters/*=nullptr*/)
    {
        CauldronAssert(ASSERT_ERROR, g_DXCCreateFunc != nullptr, L"DXC Shader Compiler has not been initialized");
        if (!g_DXCCreateFunc)
            return nullptr;

        std::wstring shaderCode;

        // Append the shader path to the file name
        std::wstring filePath = L"Shaders\\";
        filePath += shaderDesc.ShaderCode;

        // Is this a file or just a string?
        bool shaderFile = false;
        const uint64_t stringLength = wcslen(shaderDesc.ShaderCode);
        if (shaderDesc.ShaderCode[stringLength - 5] == L'.' &&
            (shaderDesc.ShaderCode[stringLength - 4] == L'h' || shaderDesc.ShaderCode[stringLength - 4] == L'H') &&
            (shaderDesc.ShaderCode[stringLength - 3] == L'l' || shaderDesc.ShaderCode[stringLength - 3] == L'L') &&
            (shaderDesc.ShaderCode[stringLength - 2] == L's' || shaderDesc.ShaderCode[stringLength - 2] == L'S') &&
            (shaderDesc.ShaderCode[stringLength - 1] == L'l' || shaderDesc.ShaderCode[stringLength - 1] == L'L'))
        {
            ParseFileToShaderCode(filePath.c_str(), &shaderDesc.Defines, shaderCode);
            shaderFile = true;
        }
        else
        {
            ParseStringToShaderCode(shaderDesc.ShaderCode, &shaderDesc.Defines, shaderCode);
        }

        // Build a shader hash based on shader code + entry point
        std::wstring hashString = shaderDesc.EntryPoint;
        hashString += shaderCode;

        std::hash<std::wstring> hasher;
        size_t hashID = hasher(hashString);
        hashString = std::to_wstring(hashID);
        hashString += L".lld";

        // Get our exe path
        filesystem::path pdbPath = filesystem::current_path();
        pdbPath.append(L"DX12PDBs");
        pdbPath.append(hashString.c_str());

        // Init the utils (new API) to build the shader
        ComPtr<IDxcUtils> pUtils;
        CauldronThrowOnFail(g_DXCCreateFunc(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));

        // Init the compiler (use Compiler3 API)
        ComPtr<IDxcCompiler3> pCompiler;
        CauldronThrowOnFail(g_DXCCreateFunc(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));

        // Temp because suspect utf16 path doesn't work
        std::string shaderCodeString = WStringToString(shaderCode);

        // Create the source code blob
        ComPtr<IDxcBlobEncoding>    pSourceCode;
        CauldronThrowOnFail(pUtils->CreateBlob(shaderCode.c_str(), static_cast<UINT32>(wcslen(shaderCode.c_str()) * sizeof(wchar_t)), DXC_CP_UTF16, &pSourceCode));

        // Put together the arguments (Note that defines are already rolled into the source)
        std::vector<LPCWSTR> arguments;

        // Push the entry point
        arguments.push_back(L"-E");
        arguments.push_back(shaderDesc.EntryPoint);

        // Disable HLSL2021 for now (comes enabled on new DXC by default)
        arguments.push_back(L"-HV 2018");

        // Push the profile
        arguments.push_back(L"-T");
        std::wstring profile;
        switch (shaderDesc.Stage)
        {
        case ShaderStage::Vertex:
            profile = L"vs_";
            break;
        case ShaderStage::Pixel:
            profile = L"ps_";
            break;
        case ShaderStage::Domain:
            profile = L"ds_";
            break;
        case ShaderStage::Hull:
            profile = L"hs_";
            break;
        case ShaderStage::Geometry:
            profile = L"gs_";
            break;
        case ShaderStage::Compute:
        default:
            profile = L"cs_";
            break;
        }

        switch (shaderDesc.Model)
        {
        case ShaderModel::SM5_1:
            profile += L"5_1";
            break;
        case ShaderModel::SM6_0:
        default:
            profile += L"6_0";
            break;
        case ShaderModel::SM6_1:
            profile += L"6_1";
            break;
        case ShaderModel::SM6_2:
            profile += L"6_2";
            break;
        case ShaderModel::SM6_3:
            profile += L"6_3";
            break;
        case ShaderModel::SM6_4:
            profile += L"6_4";
            break;
        case ShaderModel::SM6_5:
            profile += L"6_5";
            break;
        case ShaderModel::SM6_6:
            profile += L"6_6";
            break;
        case ShaderModel::SM6_7:
            profile += L"6_7";
            break;
        }
        arguments.push_back(profile.c_str());

        // Make warnings errors
        arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);    // -WX

        // Strip reflection from shader
        //arguments.push_back(L"-Qstrip_reflect");

        // Debug compile if requested
        static bool s_DebugShaders = GetConfig()->DebugShaders;
        if (s_DebugShaders)
        {
            arguments.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE); // -Zss

            arguments.push_back(DXC_ARG_DEBUG); // -Zi
            arguments.push_back(L"-Od");

#if _DX12 // Apparently no longer supported when compiling to spir-v
            // Push pdb to pdb path
            arguments.push_back(L"-Fd");
            arguments.push_back(pdbPath.c_str());
#endif // _DX12
        }

        std::vector<std::wstring> params;
        if (shaderDesc.AdditionalParams != nullptr)
        {
            std::wistringstream istringstream(shaderDesc.AdditionalParams);
            std::wstring        param;
            while (istringstream >> param)
            {
                params.push_back(param);
            }
            for (auto& iter : params)
            {
                arguments.push_back(iter.c_str());
            }
        }

        if (pAdditionalParameters != nullptr)
            arguments.insert(arguments.end(), pAdditionalParameters->begin(), pAdditionalParameters->end());

        DxcBuffer shaderCodeBuffer;
        shaderCodeBuffer.Ptr = pSourceCode->GetBufferPointer();
        shaderCodeBuffer.Size = pSourceCode->GetBufferSize();
        shaderCodeBuffer.Encoding = DXC_CP_UTF16; //0;

        IncludeHandler includeFileHandler(pUtils.Get());

        // Compile the shader
        ComPtr<IDxcResult> pCompiledResult;
        pCompiler->Compile(&shaderCodeBuffer, arguments.data(), static_cast<UINT32>(arguments.size()), &includeFileHandler, IID_PPV_ARGS(&pCompiledResult));

        // Handle any errors if they occurred
        ComPtr<IDxcBlobUtf8> pErrors;    // wide version currently doesn't appear to be supported
        pCompiledResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
        if (pErrors && pErrors->GetStringLength() > 0)
        {
            std::string errorString = pErrors->GetStringPointer();
            std::wstring errorWString = StringToWString(errorString.c_str());
            CauldronCritical(L"%ls : %ls", (shaderFile)? filePath.c_str() : L"ShaderCodeString", errorWString.c_str());
            return nullptr;
        }

        // Write out the pdb if there is one
        ComPtr<IDxcBlob> pPDBBlob;
        pCompiledResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDBBlob), nullptr);
        if (pPDBBlob && pPDBBlob->GetBufferSize() > 0 && pPDBBlob->GetBufferPointer() != nullptr)
        {
            // create folder if necessary
            filesystem::create_directories(pdbPath.parent_path());
            std::ofstream f(pdbPath.c_str(), std::ofstream::binary);
            f.write(reinterpret_cast<const char*>(pPDBBlob->GetBufferPointer()), pPDBBlob->GetBufferSize());
            f.close();
        }

        // Get the shader hash (might do something with this since it's likely better than the one we calculated above?
        ComPtr<IDxcBlob> pShaderHashBlob;
        pCompiledResult->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pShaderHashBlob), nullptr);
        if (pShaderHashBlob)
        {
            // Do something with the hash
        }

        // Get the binary code so we can return it
        IDxcBlob* pShaderBinary;    // We are not using a ComPtr here as we need this to go to void*. Will store in a ComPtr inside the ShaderObject.
        pCompiledResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&pShaderBinary), nullptr);
        if (pShaderBinary)
            return pShaderBinary;

        // Something went wrong
        return nullptr;
    }

    int InitShaderCompileSystem()
    {
        std::wstring fullCompilerPath = L"dxcompiler.dll";
        std::wstring fullDXILPath = L"dxil.dll";

        HMODULE hDXILModule = ::LoadLibraryW(fullDXILPath.c_str());
        HMODULE hDXCModule = ::LoadLibraryW(fullCompilerPath.c_str());

        g_DXCCreateFunc = (DxcCreateInstanceProc)::GetProcAddress(hDXCModule, "DxcCreateInstance");

        if (g_DXCCreateFunc)
            return 0;

        // Something went wrong
        return -1;
    }

    void TerminateShaderCompileSystem()
    {

    }

} // namespace cauldron

#endif // #if defined(_WIN)
