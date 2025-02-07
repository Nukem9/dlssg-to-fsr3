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

#include "hlsl_compiler.h"
#include "utils.h"

// D3D12SDKVersion needs to line up with the version number on Microsoft's DirectX12 Agility SDK Download page
extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 614; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

struct DxcCustomIncludeHandler : public IDxcIncludeHandler
{
    HRESULT DxcCustomIncludeHandler::QueryInterface(REFIID iid, void** ppvObject) override
    {
        return S_OK;
    }

    ULONG DxcCustomIncludeHandler::AddRef() override
    {
        return 1;
    }

    ULONG DxcCustomIncludeHandler::Release() override
    {
        return 1;
    }

    HRESULT DxcCustomIncludeHandler::LoadSource(_In_z_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
    {
        fs::path filename = WCharToUTF8(std::wstring(pFilename));
        fs::path dependentFilename;

        // try opening the file in local folder
        fs::path localFolder = sourcePath;
        localFolder.remove_filename();
        dependentFilename         = fs::absolute(filename);
        filename                  = fs::relative(dependentFilename);
        fs::path relativeFilename = fs::relative(filename, localFolder);

        // try search file in include paths
        if (!fs::exists(filename))
        {
            dependentFilename.clear();

            bool     found               = false;
            fs::path newRelativeFilename = relativeFilename;

            // WORKAROUND: the pFilename could be incorrect and contain unnecessary relative path to the front when multiple level of files included
            // So when file can't be found, we try to remove the first level folder in path
            do
            {
                relativeFilename = newRelativeFilename;
                for (auto& searchPath : includeSearchPaths)
                {
                    filename = fs::absolute(searchPath / relativeFilename);
                    if (fs::exists(filename))
                    {
                        dependentFilename = filename;  // update dependent filename to the searched location
                        found             = true;
                        break;
                    }
                }

                // remove one path level in front
                if (!found)
                    newRelativeFilename = fs::relative(relativeFilename, *relativeFilename.begin());
            } while (!found && newRelativeFilename != relativeFilename);
        }

        if (!dependentFilename.empty())
            dependencies.insert(dependentFilename.generic_string());

        return dxcDefaultIncludeHandler->LoadSource(UTF8ToWChar(dependentFilename.string()).c_str(), ppIncludeSource);
    }

    fs::path sourcePath;
    std::vector<fs::path> includeSearchPaths;
    std::unordered_set<std::string> dependencies;
    CComPtr<IDxcIncludeHandler> dxcDefaultIncludeHandler;
};

struct FxcCustomIncludeHandler : public ID3DInclude
{
    COM_DECLSPEC_NOTHROW HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFilename, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
    {
        fs::path filename;
        fs::path dependentFilename;

        FILE* fp = NULL;

        // try opening the file in local folder
        fs::path localFolder = sourcePath;
        localFolder.remove_filename();
        filename          = fs::absolute(localFolder / pFilename);
        _wfopen_s(&fp, UTF8ToWChar(filename.string()).c_str(), L"rb");

        // try search file in include paths
        if (!fp)
        {
            for (auto& searchPath : includeSearchPaths)
            {
                filename = fs::absolute(searchPath / pFilename);
                _wfopen_s(&fp, UTF8ToWChar(filename.string()).c_str(), L"rb");
                if (fp)
                {
                    dependentFilename = filename;  // update dependent filename to the searched location
                    break;
                }
            }
        }

        if (!dependentFilename.empty())
            dependencies.insert(dependentFilename.generic_string());

        if (fp)
        {
            fseek(fp, 0, SEEK_END);
            *pBytes = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            buffer.resize(*pBytes);
            fread(buffer.data(), *pBytes, 1, fp);
            fclose(fp);

            *ppData = buffer.data();

            return S_OK;
        }

        return E_FAIL;
    }

    COM_DECLSPEC_NOTHROW HRESULT __stdcall Close(LPCVOID pData)
    {
        return S_OK;
    }

    fs::path sourcePath;
    std::vector<fs::path> includeSearchPaths;
    std::unordered_set<std::string> dependencies;
    std::vector<char*> buffer;
};

uint8_t* HLSLDxcShaderBinary::BufferPointer()
{
    return (uint8_t*)pShader->GetBufferPointer();
}

size_t HLSLDxcShaderBinary::BufferSize()
{
    return pShader->GetBufferSize();
}

uint8_t* HLSLFxcShaderBinary::BufferPointer()
{
    return (uint8_t*)pShader->GetBufferPointer();
}

size_t HLSLFxcShaderBinary::BufferSize()
{
    return pShader->GetBufferSize();
}

HLSLCompiler::HLSLCompiler(HLSLCompiler::Backend backend,
                           const std::string&    dll,
                           const std::string&    shaderPath,
                           const std::string&    shaderName,
                           const std::string&    shaderFileName,
                           const std::string&    outputPath,
                           bool                  disableLogs,
                           bool                  debugCompile)
    : ICompiler(shaderPath, shaderName, shaderFileName, outputPath, disableLogs, debugCompile)
    , m_backend(backend)
{
    // Read shader source
    std::ifstream stream(m_ShaderPath);
    m_Source = std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

    switch (m_backend)
    {
    case HLSLCompiler::DXC:
    {
        if (!dll.empty())
        {
            printf("Attempting to load binary:\n");
            printf("%s\n", dll.c_str());
        }
        m_DllHandle = LoadLibrary(dll.empty() ? "dxcompiler.dll" : dll.c_str());

        if (m_DllHandle != nullptr)
        {
            m_DxcCreateInstanceFunc = (DxcCreateInstanceProc)GetProcAddress(m_DllHandle, "DxcCreateInstance");

            if (!m_DxcCreateInstanceFunc)
                throw std::runtime_error("Failed to load DXC library!");
        }
        else
        {
            auto err = GetLastError();
            std::string errorMsg("Failed to load DXC library! Failed with error ");
            errorMsg += std::to_string(err);
            throw std::runtime_error(errorMsg);
        } 

        // Create compiler and utils.
        HRESULT hr = m_DxcCreateInstanceFunc(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils));
        assert(SUCCEEDED(hr));
        hr = m_DxcCreateInstanceFunc(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler));
        assert(SUCCEEDED(hr));

        // Create default include handler.
        hr = m_DxcUtils->CreateDefaultIncludeHandler(&m_DxcDefaultIncludeHandler);
        assert(SUCCEEDED(hr));

        break;
    }
    case HLSLCompiler::GDK_SCARLETT_X64:
    case HLSLCompiler::GDK_XBOXONE_X64:
    {
        // Setup the path to the compiler dll
        std::wstring dllPath;

        // Xbox One
        if (m_backend == HLSLCompiler::GDK_XBOXONE_X64)
        {
            const wchar_t* gdkPath = _wgetenv(L"GXDKLatest");
            if (!gdkPath)
            {
                throw std::runtime_error("GDK Xbox One compile requested, but could not find \"GXDKLatest\" environment variable. Please ensure the GDK is installed");
            }

            dllPath = gdkPath;
            dllPath += L"bin\\XboxOne\\dxcompiler_x.dll";

            // Compiler internally looks for local paths
            std::wstring dllPathSearch = gdkPath;
            dllPathSearch += L"bin\\XboxOne\\";
            SetDllDirectoryW(dllPathSearch.c_str());
        }
        
        // Scarlett
        else
        {
            const wchar_t* gdkPath = _wgetenv(L"GXDKLatest");
            if (!gdkPath)
            {
                throw std::runtime_error("GDK Scarlett compile requested, but could not find \"GXDKLatest\" environment variable. Please ensure the GDK is installed");
            }

            dllPath = gdkPath;
            dllPath += L"bin\\Scarlett\\dxcompiler_xs.dll";

            // Compiler internally looks for local paths
            std::wstring dllPathSearch = gdkPath;
            dllPathSearch += L"bin\\Scarlett\\";
            SetDllDirectoryW(dllPathSearch.c_str());
        }

        m_DllHandle = LoadLibraryW(dllPath.c_str());

        if (m_DllHandle != nullptr)
        {
            m_DxcCreateInstanceFunc = (DxcCreateInstanceProc)GetProcAddress(m_DllHandle, "DxcCreateInstance");

            if (!m_DxcCreateInstanceFunc)
                throw std::runtime_error("Failed to load GDC dxcompiler.dll!");
        }
        else
        {
            auto        err = GetLastError();
            std::string errorMsg("Failed to load GDC dxcompiler.dll! Failed with error ");
            errorMsg += std::to_string(err);
            throw std::runtime_error(errorMsg);
        }

        // Create compiler and utils.
        HRESULT hr = m_DxcCreateInstanceFunc(CLSID_DxcUtils, IID_PPV_ARGS(&m_DxcUtils));
        assert(SUCCEEDED(hr));
        hr = m_DxcCreateInstanceFunc(CLSID_DxcCompiler, IID_PPV_ARGS(&m_DxcCompiler));
        assert(SUCCEEDED(hr));

        // Create default include handler.
        hr = m_DxcUtils->CreateDefaultIncludeHandler(&m_DxcDefaultIncludeHandler);
        assert(SUCCEEDED(hr));

        break;
    }
    case HLSLCompiler::FXC:
    {
        m_DllHandle = LoadLibrary(dll.empty() ? "D3DCompiler_47.dll" : dll.c_str());

        if (m_DllHandle != nullptr)
        {
            m_FxcD3DCompile = (pD3DCompile)GetProcAddress(m_DllHandle, "D3DCompile");
            m_FxcD3DGetBlobPart = (pD3DGetBlobPart)GetProcAddress(m_DllHandle, "D3DGetBlobPart");
            m_FxcD3DReflect = (pD3DReflect)GetProcAddress(m_DllHandle, "D3DReflect");

            if (!(m_FxcD3DCompile && m_FxcD3DGetBlobPart && m_FxcD3DReflect))
                throw std::runtime_error("Failed to load D3DCompiler library!");
        }
        else
            throw std::runtime_error("Failed to load D3DCompiler library!");

        break;
    }
    default:
        assert(false);
    }
}

HLSLCompiler::~HLSLCompiler()
{
    m_DxcDefaultIncludeHandler.Release();
    m_DxcUtils.Release();
    m_DxcCompiler.Release();

    FreeLibrary(m_DllHandle);
}

bool HLSLCompiler::CompileDXC(Permutation& permutation, const std::vector<std::string>& arguments, std::mutex& writeMutex)
{
    HLSLDxcShaderBinary* hlslShaderBinary = new HLSLDxcShaderBinary();

    permutation.shaderBinary = std::shared_ptr<HLSLDxcShaderBinary>(hlslShaderBinary);

    // ------------------------------------------------------------------------------------------------
    // Setup compiler args.
    // ------------------------------------------------------------------------------------------------
    std::vector<std::wstring> strDefines = {};
    std::vector<std::wstring> strArgs    = {};

    bool shouldGeneratePDB = false;

    std::wstring entry;
    std::wstring profile;

    std::vector<fs::path> includePaths;

    for (size_t i = 0; i < arguments.size(); i++)
    {
        const std::string& arg = arguments[i];

        if (arg == "-Zi" || arg == "-Zs")
        {
            shouldGeneratePDB = true;
            // Skip as m_DebugCompile also sets this.
            if (m_DebugCompile)
                continue;
        }

        // Skip as m_DebugCompile also sets this.
        if (arg == "-Zss" && m_DebugCompile)
            continue;

        if (arg == "-E")
        {
            entry = UTF8ToWChar(arguments[i + 1]);

            i++;
            continue;
        }

        if (arg == "-T")
        {
            profile = UTF8ToWChar(arguments[i + 1]);

            i++;
            continue;
        }

        
        if (arguments[i] == "-I")
        {
            includePaths.push_back(arguments[i + 1].c_str());

            i++;
            continue;
        }

        if (arg == "-D")
        {
            const auto define = arguments[i + 1];

            std::wstringstream test(UTF8ToWChar(define));
            std::wstring       token;

            std::vector<std::wstring> seglist;

            while (std::getline(test, token, L'='))
            {
                token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
                seglist.push_back(token);
            }

            auto baseIndex = strDefines.size();
            strDefines.push_back(seglist[0]);
            strDefines.push_back(seglist[1]);

            i++;
            continue;
        }

        strArgs.push_back(UTF8ToWChar(arg));
    }

    if (m_backend == HLSLCompiler::GDK_SCARLETT_X64 || m_backend == HLSLCompiler::GDK_XBOXONE_X64)
    {
        strArgs.push_back(L"-Qstrip_debug");
    }

    std::wstring pdbPath;
    if (m_DebugCompile)
    {
        shouldGeneratePDB = true;
        strArgs.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE);  // -Zss
        strArgs.push_back(DXC_ARG_DEBUG);  // -Zi
        strArgs.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
    }

    std::vector<LPCWSTR> args = {};
    args.reserve(strArgs.size());
    for (auto& str : strArgs)
    {
        args.push_back(str.c_str());
    }

    std::vector<DxcDefine> defines = {};
    defines.reserve(strDefines.size() / 2);
    for (size_t i = 0; i < strDefines.size(); i += 2)
    {
        defines.push_back(DxcDefine{strDefines[i].c_str(), strDefines[i + 1].c_str()});
    }

    std::wstring sourceName = UTF8ToWChar(m_ShaderPath);

    CComPtr<IDxcCompilerArgs> pArgs;
    m_DxcUtils->BuildArguments(sourceName.c_str(), entry.c_str(), profile.c_str(), args.data(), args.size(), defines.data(), defines.size(), &pArgs);

    // ------------------------------------------------------------------------------------------------
    // Compile it with specified arguments.
    // ------------------------------------------------------------------------------------------------
    DxcBuffer buffer;

    buffer.Ptr      = m_Source.c_str();
    buffer.Size     = m_Source.size() * sizeof(char);
    buffer.Encoding = DXC_CP_UTF8;

    DxcCustomIncludeHandler customIncludeHandler;
    customIncludeHandler.dxcDefaultIncludeHandler = m_DxcDefaultIncludeHandler;
    customIncludeHandler.sourcePath               = permutation.sourcePath;
    customIncludeHandler.includeSearchPaths       = std::move(includePaths);

    HRESULT hr = m_DxcCompiler->Compile(&buffer,                                   // Source buffer.
                                        pArgs->GetArguments(),                     // Array of pointers to arguments.
                                        pArgs->GetCount(),                         // Number of arguments.
                                        &customIncludeHandler,                     // User-provided interface to handle #include directives (optional).
                                        IID_PPV_ARGS(&hlslShaderBinary->pResults)  // Compiler output status, buffer, and errors.
    );
    assert(SUCCEEDED(hr));

    permutation.dependencies = std::move(customIncludeHandler.dependencies);

    // Quit if the compilation failed.
    HRESULT hrStatus;
    hlslShaderBinary->pResults->GetStatus(&hrStatus);

    bool succeeded = !FAILED(hrStatus);

    CComPtr<IDxcBlobUtf8> errors = nullptr;
    hlslShaderBinary->pResults->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);

    if (!m_DisableLogs && errors != nullptr && errors->GetStringLength() != 0)
    {
        writeMutex.lock();
        fprintf(stderr, "%s[%lu]\n%s", m_ShaderFileName.c_str(), permutation.key, errors->GetStringPointer());
        writeMutex.unlock();
    }

    if (succeeded)
    {
        // ------------------------------------------------------------------------------------------------
        // Retrieve shader binary.
        // ------------------------------------------------------------------------------------------------
        CComPtr<IDxcBlobUtf16> pShaderName = nullptr;
        hlslShaderBinary->pResults->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&hlslShaderBinary->pShader), &pShaderName);

        // ------------------------------------------------------------------------------------------------
        // Retrieve shader hash
        // ------------------------------------------------------------------------------------------------
        CComPtr<IDxcBlob> pHash = nullptr;

        hlslShaderBinary->pResults->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&pHash), nullptr);
        if (pHash != nullptr)
        {
            DxcShaderHash* pHashBuf = (DxcShaderHash*)pHash->GetBufferPointer();

            std::stringstream s;
            s << std::setfill('0') << std::hex;

            for (uint8_t i = 0; i < 16; i++)
                s << std::setw(2) << (uint32_t)pHashBuf->HashDigest[i];

            permutation.hashDigest = s.str();
        }

        // ------------------------------------------------------------------------------------------------
        // Dump PDB if required
        // ------------------------------------------------------------------------------------------------
        if (shouldGeneratePDB)
        {
            CComPtr<IDxcBlob>      pPDB     = nullptr;
            CComPtr<IDxcBlobUtf16> pPDBName = nullptr;

            std::wstring pdbPath;
            // Use a unique name that takes the hash into consideration
            pdbPath = UTF8ToWChar(m_OutputPath + "\\") + UTF8ToWChar(permutation.hashDigest) + L".pdb";


            // Account for longer than MAX_PATH length file paths
            if (pdbPath.length() > MAX_PATH - 1)
            {
                // We can get around MAX_PATH with "\\\\?\\", but this requires only having backslashes in the path (CMake uses forward slashes)
                size_t pos;
                while ((pos = pdbPath.find(L"/")) != std::wstring::npos) {
                    pdbPath.replace(pos, 1, L"\\");
                }
                pdbPath = L"\\\\?\\" + pdbPath;
            }
            hlslShaderBinary->pResults->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName);

            FILE* fp = NULL;

            const std::string pathToPDB{WCharToUTF8(pdbPath)};

            fp = fopen(pathToPDB.c_str(), "wb");
            if (fp)
            {
                fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
                fclose(fp);
            }
        }

        permutation.name           = m_ShaderName + "_" + permutation.hashDigest;
        permutation.headerFileName = permutation.name + ".h";
    }

    return succeeded;
}

bool HLSLCompiler::CompileFXC(Permutation&                    permutation,
                              const std::vector<std::string>& arguments,
                              std::mutex&                     writeMutex)
{
    HLSLFxcShaderBinary* hlslShaderBinary = new HLSLFxcShaderBinary();

    permutation.shaderBinary = std::shared_ptr<HLSLFxcShaderBinary>(hlslShaderBinary);

    // ------------------------------------------------------------------------------------------------
    // Setup compiler args.
    // ------------------------------------------------------------------------------------------------
    std::vector<std::string> strMacros = {};
    std::vector<D3D_SHADER_MACRO> macros = {};
    strMacros.reserve(arguments.size());
    macros.reserve(arguments.size());

    const char* entryPoint = nullptr;
    const char* target = nullptr;
    bool shouldGeneratePDB = false;
    uint32_t flags = 0;

    std::vector<fs::path> includePaths;

    for (auto i = 0; i < arguments.size(); ++i)
    {
        if (arguments[i] == "-E")
        {
            entryPoint = arguments[++i].c_str();
        }
        if (arguments[i] == "-T")
        {
            target = arguments[++i].c_str();
        }
        if (arguments[i] == "-Zi" || arguments[i] == "-Zs")
        {
            shouldGeneratePDB = true;
            flags |= D3DCOMPILE_DEBUG;
        }
        if (arguments[i] == "-I")
        {
            includePaths.push_back(arguments[++i].c_str());
        }
        if (arguments[i] == "-Od")
        {
            flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
        }
        if (arguments[i] == "-O0")
        {
            flags |= D3DCOMPILE_OPTIMIZATION_LEVEL0;
        }
        if (arguments[i] == "-O1")
        {
            flags |= D3DCOMPILE_OPTIMIZATION_LEVEL1;
        }
        if (arguments[i] == "-O2")
        {
            flags |= D3DCOMPILE_OPTIMIZATION_LEVEL2;
        }
        if (arguments[i] == "-O3")
        {
            flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
        }
        if (arguments[i] == "-D")
        {
            const std::string& arg = arguments[++i];
            size_t idx = arg.find_first_of('=');
            strMacros.push_back(arg.substr(0, idx));
            if (idx != std::string::npos)
            {
                strMacros.push_back(arg.substr(idx + 1));
            }
            else
            {
                strMacros.push_back("");
            }
            macros.push_back({ strMacros[strMacros.size() - 2].c_str(), strMacros[strMacros.size() - 1].c_str() });
        }
    }
    macros.push_back({ nullptr, nullptr });

    if (m_DebugCompile)
    {
        shouldGeneratePDB = true;
        flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
    }

    CComPtr<ID3DBlob> pError = nullptr;

    FxcCustomIncludeHandler customIncludeHandler;
    customIncludeHandler.sourcePath = permutation.sourcePath;
    customIncludeHandler.includeSearchPaths = std::move(includePaths);

    HRESULT hr = m_FxcD3DCompile(m_Source.c_str(),
                                 m_Source.size(),
                                 permutation.sourcePath.generic_string().c_str(),
                                 macros.data(),
                                 &customIncludeHandler,
                                 entryPoint,
                                 target,
                                 flags,
                                 0,
                                 &hlslShaderBinary->pShader,
                                 &pError);

    bool succeeded = !FAILED(hr);

    if (!m_DisableLogs && pError != nullptr)
    {
        writeMutex.lock();
        printf("%s[%lu]\n%s",
            m_ShaderFileName.c_str(),
            permutation.key,
            (char*)pError->GetBufferPointer());
        writeMutex.unlock();
    }

    if(succeeded)
    {
        permutation.dependencies = std::move(customIncludeHandler.dependencies);

        // ------------------------------------------------------------------------------------------------
        // Retrieve shader hash
        // ------------------------------------------------------------------------------------------------
        DWORD hash[4];

        if (CalculateDXBCChecksum(hlslShaderBinary->BufferPointer(), hlslShaderBinary->BufferSize(), hash))
        {
            std::stringstream s;
            s << std::setfill('0') << std::hex;

            for (int i = 0; i < 4; ++i)
                s << std::setw(8) << (uint32_t)hash[i];

            permutation.hashDigest = s.str();
        }

        // ------------------------------------------------------------------------------------------------
        // Dump PDB if required
        // ------------------------------------------------------------------------------------------------
        if (shouldGeneratePDB)
        {
            CComPtr<ID3DBlob> pPDB     = nullptr;
            CComPtr<ID3DBlob> pPDBName = nullptr;

            m_FxcD3DGetBlobPart(hlslShaderBinary->BufferPointer(), hlslShaderBinary->BufferSize(), D3D_BLOB_PDB, 0, &pPDB);

            FILE* fp = NULL;

            std::string pdbName = permutation.hashDigest + ".pdb";
            std::string pathToPDB = m_OutputPath + "\\" + pdbName;

            // Account for longer than MAX_PATH length file paths
            if (pathToPDB.length() > MAX_PATH - 1)
            {
                // We can get around MAX_PATH with "\\\\?\\", but this requires only having backslashes in the path (CMake uses forward slashes)
                size_t pos;
                while ((pos = pathToPDB.find("/")) != std::string::npos) {
                    pathToPDB.replace(pos, 1, "\\");
                }
                pathToPDB = "\\\\?\\" + pathToPDB;
            }

            fp = fopen(pathToPDB.c_str(), "wb");
            fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
            fclose(fp);
        }

        permutation.name           = m_ShaderName + "_" + permutation.hashDigest;
        permutation.headerFileName = permutation.name + ".h";
    }

    return succeeded;
}

bool HLSLCompiler::Compile(Permutation&                    permutation,
                           const std::vector<std::string>& arguments,
                           std::mutex&                     writeMutex)
{
    switch (m_backend)
    {
    case HLSLCompiler::DXC:
    case HLSLCompiler::GDK_SCARLETT_X64:
    case HLSLCompiler::GDK_XBOXONE_X64:
        return CompileDXC(permutation, arguments, writeMutex);
    case HLSLCompiler::FXC:
        return CompileFXC(permutation, arguments, writeMutex);
    default:
        assert(false);
        return false;
    }
}

bool HLSLCompiler::ExtractDXCReflectionData(Permutation& permutation)
{
    IShaderBinary* hlslShaderBinary = permutation.shaderBinary.get();
    permutation.reflectionData = std::make_shared<IReflectionData>();

    IReflectionData* hlslReflectionData = reinterpret_cast<IReflectionData*>(permutation.reflectionData.get());

    CComPtr<IDxcBlob> pReflectionData;
    reinterpret_cast<HLSLDxcShaderBinary*>(hlslShaderBinary)->pResults->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionData), nullptr);

    if (pReflectionData != nullptr)
    {
        // Create reflection interface.
        DxcBuffer ReflectionData;
        ReflectionData.Encoding = DXC_CP_ACP;
        ReflectionData.Ptr      = hlslShaderBinary->BufferPointer();
        ReflectionData.Size     = hlslShaderBinary->BufferSize();

        CComPtr<ID3D12ShaderReflection> pReflection;

        m_DxcUtils->CreateReflection(&ReflectionData, IID_PPV_ARGS(&pReflection));

        D3D12_SHADER_DESC shaderDesc;

        pReflection->GetDesc(&shaderDesc);

        for (int i = 0; i < shaderDesc.BoundResources; i++)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindingDesc;
            pReflection->GetResourceBindingDesc(i, &bindingDesc);
            ShaderResourceInfo resourceInfo = { bindingDesc.Name, bindingDesc.BindPoint, bindingDesc.BindCount, bindingDesc.Space };

            switch (bindingDesc.Type)
            {
            case D3D_SIT_CBUFFER:
                hlslReflectionData->constantBuffers.push_back(resourceInfo);
                break;
            case D3D_SIT_TEXTURE:
                hlslReflectionData->srvTextures.push_back(resourceInfo);
                break;
            case D3D_SIT_SAMPLER:
                hlslReflectionData->samplers.push_back(resourceInfo);
                break;
            case D3D_SIT_UAV_RWTYPED:
                hlslReflectionData->uavTextures.push_back(resourceInfo);
                break;
            case D3D_SIT_STRUCTURED:
                hlslReflectionData->srvBuffers.push_back(resourceInfo);
                break;
            case D3D_SIT_UAV_RWSTRUCTURED:
                hlslReflectionData->uavBuffers.push_back(resourceInfo);
                break;
            case D3D_SIT_RTACCELERATIONSTRUCTURE:
                hlslReflectionData->rtAccelerationStructures.push_back(resourceInfo);
                break;
            case D3D_SIT_BYTEADDRESS:
                hlslReflectionData->srvBuffers.push_back(resourceInfo);
                break;
            case D3D_SIT_UAV_RWBYTEADDRESS:
                hlslReflectionData->uavBuffers.push_back(resourceInfo);
                break;

            case D3D_SIT_UAV_APPEND_STRUCTURED:
            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
            case D3D_SIT_UAV_FEEDBACKTEXTURE:
            case D3D_SIT_TBUFFER:
            default:
                throw std::runtime_error("Shader uses an unsupported resource type!");
                break;
            }
        }

        return true;
    }
    else
        return false;
}

bool HLSLCompiler::ExtractFXCReflectionData(Permutation& permutation)
{
    IShaderBinary* hlslShaderBinary = reinterpret_cast<HLSLFxcShaderBinary*>(permutation.shaderBinary.get());
    permutation.reflectionData = std::make_shared<IReflectionData>();

    IReflectionData* hlslReflectionData = reinterpret_cast<IReflectionData*>(permutation.reflectionData.get());

    CComPtr<ID3D11ShaderReflection> pReflection;
    m_FxcD3DReflect(hlslShaderBinary->BufferPointer(), hlslShaderBinary->BufferSize(), IID_ID3D11ShaderReflection, (void**)&pReflection);

    if (pReflection != nullptr)
    {
        D3D11_SHADER_DESC desc;
        pReflection->GetDesc(&desc);

        std::string lastResourceName;
        uint32_t bindCount = 1;
        for (auto i = 0; i < desc.BoundResources; ++i)
        {
            D3D11_SHADER_INPUT_BIND_DESC bindDesc;
            pReflection->GetResourceBindingDesc(i, &bindDesc);

            std::string resourceName(bindDesc.Name);

            // only count the first bind of an array
            auto idx = resourceName.find_first_of('[');
            if (idx != std::string::npos)
            {
                resourceName = resourceName.substr(0, idx);
            }

            if (resourceName == lastResourceName)
            {
                ++bindCount;
                continue;
            }
            else
            {
                lastResourceName = resourceName;
            }
            ShaderResourceInfo resourceInfo = { resourceName, bindDesc.BindPoint, bindCount, 1 };

            switch (bindDesc.Type)
            {
            case D3D_SIT_CBUFFER:
                hlslReflectionData->constantBuffers.push_back(resourceInfo);
                break;
            case D3D_SIT_TEXTURE:
                hlslReflectionData->srvTextures.push_back(resourceInfo);
                break;
            case D3D_SIT_SAMPLER:
                hlslReflectionData->samplers.push_back(resourceInfo);
                break;
            case D3D_SIT_UAV_RWTYPED:
                hlslReflectionData->uavTextures.push_back(resourceInfo);
                break;
            case D3D_SIT_STRUCTURED:
                hlslReflectionData->srvBuffers.push_back(resourceInfo);
                break;
            case D3D_SIT_UAV_RWSTRUCTURED:
                hlslReflectionData->uavBuffers.push_back(resourceInfo);
                break;
            case D3D_SIT_RTACCELERATIONSTRUCTURE:
                hlslReflectionData->rtAccelerationStructures.push_back(resourceInfo);
                break;

            case D3D_SIT_BYTEADDRESS:
                hlslReflectionData->srvBuffers.push_back(resourceInfo);
                break;
            case D3D_SIT_UAV_RWBYTEADDRESS:
                hlslReflectionData->uavBuffers.push_back(resourceInfo);
                break;

            case D3D_SIT_UAV_APPEND_STRUCTURED:
            case D3D_SIT_UAV_CONSUME_STRUCTURED:
            case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
            case D3D_SIT_UAV_FEEDBACKTEXTURE:
            case D3D_SIT_TBUFFER:
            default:
                throw std::runtime_error("Shader uses an unsupported resource type!");
                break;
            }

            bindCount = 1;
        }

        return true;
    }
    else
        return false;
}

bool HLSLCompiler::ExtractReflectionData(Permutation& permutation)
{
    switch (m_backend)
    {
    case HLSLCompiler::DXC:
    case HLSLCompiler::GDK_SCARLETT_X64:
    case HLSLCompiler::GDK_XBOXONE_X64:
        return ExtractDXCReflectionData(permutation);
    case HLSLCompiler::FXC:
        return ExtractFXCReflectionData(permutation);
    default:
        assert(false);
        return false;
    }
}

void HLSLCompiler::WriteBinaryHeaderReflectionData(FILE* fp, const Permutation& permutation, std::mutex& writeMutex)
{
    IReflectionData* hlslReflectionData = dynamic_cast<IReflectionData*>(permutation.reflectionData.get());

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

                fprintf(fp, "static const uint32_t g_%s_%sResourceSpaces[] = { ", permutationName.c_str(), resourceTypeString.c_str());

                for (int j = 0; j < resourceInfo.size(); j++)
                {
                    const ShaderResourceInfo& info = resourceInfo[j];

                    fprintf(fp, " %i,", info.space);
                }

                fprintf(fp, " };\n\n");
            }
        };

    // CBVs
    WriteResourceInfo(fp, permutation.name, hlslReflectionData->constantBuffers, "CBV");

    // SRV Textures
    WriteResourceInfo(fp, permutation.name, hlslReflectionData->srvTextures, "TextureSRV");

    // UAV Textures
    WriteResourceInfo(fp, permutation.name, hlslReflectionData->uavTextures, "TextureUAV");

    // SRV Buffers
    WriteResourceInfo(fp, permutation.name, hlslReflectionData->srvBuffers, "BufferSRV");

    // UAV Buffers
    WriteResourceInfo(fp, permutation.name, hlslReflectionData->uavBuffers, "BufferUAV");

    // Sampler
    WriteResourceInfo(fp, permutation.name, hlslReflectionData->samplers, "Sampler");

    // RT Acceleration Structure
    WriteResourceInfo(fp, permutation.name, hlslReflectionData->rtAccelerationStructures, "RTAccelerationStructure");
}

void HLSLCompiler::WritePermutationHeaderReflectionStructMembers(FILE* fp)
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

void HLSLCompiler::WritePermutationHeaderReflectionData(FILE* fp, const Permutation& permutation)
{
    IReflectionData* hlslReflectionData = dynamic_cast<IReflectionData*>(permutation.reflectionData.get());

    const auto WriteResourceInfo = [](FILE* fp, const int& numResources, const std::string& permutationName, const std::string& resourceTypeString) {
        if (numResources == 0)
        {
            fprintf(fp, "0, 0, 0, 0, 0, ");
        }
        else
        {
            fprintf(fp,
                    "%i, g_%s_%sResourceNames, g_%s_%sResourceBindings, g_%s_%sResourceCounts, g_%s_%sResourceSpaces, ",
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

    WriteResourceInfo(fp, hlslReflectionData->constantBuffers.size(), permutation.name, "CBV");
    WriteResourceInfo(fp, hlslReflectionData->srvTextures.size(), permutation.name, "TextureSRV");
    WriteResourceInfo(fp, hlslReflectionData->uavTextures.size(), permutation.name, "TextureUAV");
    WriteResourceInfo(fp, hlslReflectionData->srvBuffers.size(), permutation.name, "BufferSRV");
    WriteResourceInfo(fp, hlslReflectionData->uavBuffers.size(), permutation.name, "BufferUAV");
    WriteResourceInfo(fp, hlslReflectionData->samplers.size(), permutation.name, "Sampler");
    WriteResourceInfo(fp, hlslReflectionData->rtAccelerationStructures.size(), permutation.name, "RTAccelerationStructure");
}
