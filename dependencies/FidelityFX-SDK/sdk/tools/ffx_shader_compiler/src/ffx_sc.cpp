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
#include "glsl_compiler.h"
#include "utils.h"

#include <Windows.h>
#include <pathcch.h>
#include <vector>
#include <string_view>
#include <filesystem>
#include <unordered_set>
#include <locale>
#include <stdexcept>


#pragma comment(lib, "pathcch.lib")

static const wchar_t* const APP_NAME    = L"FidelityFX-SC";
static const wchar_t* const EXE_NAME    = L"FidelityFX_SC";
static const wchar_t* const APP_VERSION = L"1.0.0";

inline bool Contains(std::wstring_view s, std::wstring_view subS)
{
    return s.find(subS) != s.npos;
}

inline bool Contains(std::string_view s, std::string_view subS)
{
    return s.find(subS) != s.npos;
}

inline bool StartsWith(std::wstring_view s, std::wstring_view subS)
{
    return s.substr(0, subS.size()) == subS;
}

inline bool StartsWith(std::string_view s, std::string_view subS)
{
    return s.substr(0, subS.size()) == subS;
}

inline void Split(std::wstring str, std::wstring_view token, std::vector<std::wstring>& result)
{
    while (str.size())
    {
        int index = str.find(token);
        if (index != std::wstring::npos)
        {
            result.push_back(str.substr(0, index));
            str = str.substr(index + token.size());
            if (str.size() == 0)
                result.push_back(str);
        }
        else
        {
            result.push_back(str);
            str = L"";
        }
    }
}

inline bool IsNumeric(std::wstring_view s)
{
    for (wchar_t c : s)
        if (!std::isdigit(c))
            return false;
    return !s.empty();
}

struct PermutationOption
{
    std::wstring              definition;
    std::string               definitionUtf8;
    std::vector<std::wstring> values;
    uint32_t                  numBits;
    bool                      isNumeric;
    bool                      foundInShader = false;
};

struct LaunchParameters
{
    std::vector<PermutationOption> permutationOptions;
    std::vector<std::wstring>      compilerArgs;
    std::wstring                   ouputPath;
    std::wstring                   inputFile;
    std::wstring                   shaderName;
    std::wstring                   compiler;
    std::wstring                   dxcDll;
    std::wstring                   d3dDll;
    std::wstring                   glslangExe;
    std::wstring                   deps;
    int                            numThreads         = 0;
    bool                           generateReflection = false;
    bool                           embedArguments     = false;
    bool                           printArguments     = false;
    bool                           disableLogs        = false;
    bool                           debugCompile       = false;

    static void PrintCommandLineSyntax();
    void        ParseCommandLine(int argCount, const wchar_t* const* args);

private:
    static void ParsePermutationOption(PermutationOption& outPermutationOption, const std::wstring arg);
    static void ParseString(std::wstring& outCompilerArg, const wchar_t* arg);
    static void ParseNumThreads(int& outNumThreads, const wchar_t* arg);
    static void EnsureOutputPathExistsAndMakeCanonical(std::wstring & inoutOutputPath);
};

class Application
{
private:
    LaunchParameters                     m_Params;
    std::unique_ptr<ICompiler>           m_Compiler;
    std::deque<Permutation>              m_MacroPermutations;
    std::vector<Permutation>             m_UniquePermutations;
    std::mutex                           m_ReadMutex;
    std::mutex                           m_WriteMutex;
    int                                  m_LastPermutationIndex = 0;
    std::unordered_map<int, int>         m_KeyToIndexMap;
    std::unordered_map<std::string, int> m_HashToIndexMap;
    std::wstring                         m_ShaderFileName;
    std::wstring                         m_ShaderName;

public:
    Application(const LaunchParameters& params);
    ~Application()
    {
    }
    void Process();

private:
    static std::wstring MakeFullPath(const std::wstring& outputPath, const std::wstring& fileName);
    void GenerateMacroPermutations(std::deque<Permutation>& permutations);
    void GenerateMacroPermutations(Permutation current, std::deque<Permutation>& permutations, int idx, int curBit);
    void OpenSourceFile();
    void ProcessPermutations();
    void CompilePermutation(Permutation& permutation);
    void WriteShaderBinaryHeader(Permutation& permutation);
    void PrintPermutationArguments(Permutation& permutation);
    void WriteShaderPermutationsHeader();
    void DumpDepfileGCC();
    void DumpDepfileMSVC();
};

void LaunchParameters::PrintCommandLineSyntax()
{
    wprintf(L"%ls %ls\n", APP_NAME, APP_VERSION);
    wprintf(L"Command line syntax:\n");
    wprintf(L"  %ls.exe [Options] <InputFile>\n", EXE_NAME);
    wprintf(
        L"Options:\n"
        L"<CompilerArgs>\n"
        L"  A list of arguments accepted by the target compiler, separated by spaces.\n"
        L"-output=<Path>\n"
        L"  Path to where the shader permutations should be output to.\n"
        L"-D<Name>\n"
        L"  Define a macro that is defined in all shader permutations.\n"
        L"-D<Name>={<Value1>, <Value2>, <Value3> ...}\n"
        L"  Declare a shader option that will generate permutations with the macro defined using the given values.\n"
        L"  Use a '-' to define a permutation where no macro is defined.\n"
        L"-num-threads=<Num>\n"
        L"  Number of threads to use for generating shaders.\n"
        L"  Sets to the max number of threads available on the current CPU by default.\n"
        L"-name=<Name>\n"
        L"  The name used for prefixing variables in the generated headers.\n"
        L"  Uses the file name by default.\n"
        L"-reflection\n"
        L"  Generate header containing reflection data.\n"
        L"-embed-arguments\n"
        L"  Write the compile arguments used for each permutation into their respective headers.\n"
        L"-print-arguments\n"
        L"  Print the compile arguments used for each permuations.\n"
        L"-disable-logs\n"
        L"  Prevent logging of compile warnings and errors.\n"
        L"-compiler=<Compiler>\n"
        L"  Select the compiler to generate permutations from (dxc, gdk.scarlett.x64, gdk.xboxone.x64, fxc, or glslang).\n"
        L"-dxcdll=<DXC DLL Path>\n"
        L"  Path to the dxccompiler dll to use.\n"
        L"-d3ddll=<D3D DLL Path>\n"
        L"  Path to the d3dcompiler dll to use.\n"
        L"-glslangexe=<glslangValidator.exe Path>\n"
        L"  Path to the glslangValidator executable to use.\n"
        L"-deps=<Format>\n"
        L"  Dump depfile which recorded the include file dependencies in format of (gcc or msvc).\n"
        L"-debugcompile\n"
        L"  Compile shader with debug information.\n"
        L"-debugcmdline\n"
        L"  Print all the input arguments.\n"
    );
}

void LaunchParameters::ParseCommandLine(int argCount, const wchar_t* const* args)
{
    int i = 0;

    // For easier debugging
    std::wstring debugOutput = L"FidelityFX_SC.exe Output:";
    debugOutput += L"\r\n";
    for (int count = 0; count < argCount; ++count)
    {
        // If we want to debug cmd line, don't include the debug cmd in what is spit out (since it's not needed)
        if (!wcscmp(args[count], L"-debugcmdline"))
            continue;

        debugOutput += args[count];
        debugOutput += L" ";
    }
    debugOutput += L"\r\n";

    // Options
    for (; i < argCount; ++i)
    {
        if (StartsWith(args[i], L"-D"))
        {
            if (Contains(args[i], L"{"))
            {
                std::wstring allPermutationOptionValues;

                // Keep appending the next few arguments which should be the macro values until we hit the closing brace.
                for (; i < argCount; i++)
                {
                    allPermutationOptionValues += args[i];

                    if (Contains(args[i], L"}"))
                        break;
                }

                PermutationOption permutationOption;
                ParsePermutationOption(permutationOption, allPermutationOptionValues);
                permutationOptions.push_back(permutationOption);
            }
            else
            {
                compilerArgs.push_back(L"-D");
                std::wstring arg = std::wstring(args[i]);
                compilerArgs.push_back(arg.substr(2, arg.size() - 2));
            }
        }
        else if (StartsWith(args[i], L"-debugcmdline"))
            wprintf(debugOutput.c_str());
        else if (StartsWith(args[i], L"-num-threads"))
            ParseNumThreads(numThreads, args[i]);
        else if (StartsWith(args[i], L"-output"))
            ParseString(ouputPath, args[i]);
        else if (StartsWith(args[i], L"-name"))
            ParseString(shaderName, args[i]);
        else if (StartsWith(args[i], L"-compiler"))
            ParseString(compiler, args[i]);
        else if (StartsWith(args[i], L"-dxcdll"))
            ParseString(dxcDll, args[i]);
        else if (StartsWith(args[i], L"-d3ddll"))
            ParseString(d3dDll, args[i]);
        else if (StartsWith(args[i], L"-glslangexe"))
            ParseString(glslangExe, args[i]);
        else if (StartsWith(args[i], L"-deps"))
            ParseString(deps, args[i]);
        else if (std::wstring(args[i]) == L"-reflection")
            generateReflection = true;
        else if (std::wstring(args[i]) == L"-embed-arguments")
            embedArguments = true;
        else if (std::wstring(args[i]) == L"-print-arguments")
            printArguments = true;
        else if (std::wstring(args[i]) == L"-disable-logs")
            disableLogs = true;
        else if (std::wstring(args[i]) == L"-debugcompile")
            debugCompile = true;
        else if (args[i][0] == L'-')
        {
            compilerArgs.push_back(args[i++]);

            // Attempt to parse the next arguments in case there are some parameters for the compiler args.
            for (; i < argCount; i++)
            {
                if (args[i][0] == L'-' || i == (argCount - 1))
                {
                    i--;
                    break;
                }
                else
                    compilerArgs.push_back(args[i]);
            }
        }
        else
            inputFile = args[i];
    }
    EnsureOutputPathExistsAndMakeCanonical(ouputPath);
}

void LaunchParameters::EnsureOutputPathExistsAndMakeCanonical(std::wstring& inoutOutputPath)
{
    std::replace(inoutOutputPath.begin(), inoutOutputPath.end(), L'/', L'\\');

    PWSTR canonicalOutputPath = NULL;

    // Make the path canonical, convert to long path if needed and add the trailing slash
    HRESULT hr = PathAllocCanonicalize(inoutOutputPath.c_str(), PATHCCH_ALLOW_LONG_PATHS | PATHCCH_ENSURE_TRAILING_SLASH, &canonicalOutputPath);
    if (hr == S_OK)
    {
        PWSTR componentStart = NULL;

        // Find the first character after "root" indicator -- which means a folder (path component) or file
        hr = PathCchSkipRoot(canonicalOutputPath, &componentStart);
        if (hr == S_OK)
        {
            // Try search for the next delimiter
            wchar_t* componentEnd = wcsstr(componentStart, L"\\");

            // If the delimiter is found, make sure the folder is created
            while (componentEnd != NULL)
            {
                // Temporally replace delimiter '\\' with null-terminator, create directory, and restore the delimiter
                *componentEnd = L'\0';
                CreateDirectoryW(canonicalOutputPath, NULL);
                *componentEnd = L'\\';

                // advance to the next component (file or folder) and try to find the next delimiter (meaning -- it's folder), and repeat the loop.
                componentStart = componentEnd + 1;
                componentEnd = wcsstr(componentStart, L"\\");
            }
        }
        inoutOutputPath = canonicalOutputPath;
        LocalFree(canonicalOutputPath);
    }
}

void LaunchParameters::ParsePermutationOption(PermutationOption& outPermutationOption, const std::wstring arg)
{
    size_t equalPos                 = arg.find_first_of(L"=", 0);
    outPermutationOption.definition = arg.substr(2, equalPos - 2);
    outPermutationOption.definitionUtf8 = WCharToUTF8(outPermutationOption.definition);

    size_t       openBracePos      = arg.find_first_of(L"{", 0);
    std::wstring multiOptionSubStr = arg.substr(openBracePos + 1, arg.length() - openBracePos - 2);

    Split(multiOptionSubStr, L",", outPermutationOption.values);

    outPermutationOption.isNumeric = true;

    bool hasAnyNumericValue = false;

    for (int i = 0; i < outPermutationOption.values.size(); i++)
    {
        outPermutationOption.isNumeric &= IsNumeric(outPermutationOption.values[i]);

        if (outPermutationOption.isNumeric)
            hasAnyNumericValue = true;
    }

    if (!outPermutationOption.isNumeric && hasAnyNumericValue)
        throw std::runtime_error("A shader option cannot mix numeric and string values!");

    outPermutationOption.numBits = static_cast<uint32_t>(ceilf(log2f(static_cast<float>(outPermutationOption.values.size()))));
}

void LaunchParameters::ParseString(std::wstring& outCompilerArg, const wchar_t* arg)
{
    std::wstring argStr   = std::wstring(arg);
    size_t       equalPos = argStr.find_first_of(L"=", 0);
    outCompilerArg        = argStr.substr(equalPos + 1, argStr.length() - equalPos);
}

void LaunchParameters::ParseNumThreads(int& outNumThreads, const wchar_t* arg)
{
    std::wstring argStr   = std::wstring(arg);
    size_t       equalPos = argStr.find_first_of(L"=", 0);
    outNumThreads         = std::stoi(argStr.substr(equalPos + 1, argStr.length() - equalPos));
}

Application::Application(const LaunchParameters& params)
    : m_Params(params) {}

void Application::Process()
{
    OpenSourceFile();

    GenerateMacroPermutations(m_MacroPermutations);

    size_t predictedDuplicates = std::count_if(m_MacroPermutations.begin(), m_MacroPermutations.end(), [](const Permutation& p) { return p.identicalTo.has_value(); });

    size_t totalPermutations = m_MacroPermutations.size();

    std::vector<std::thread> threads;

    if (m_Params.numThreads == 0)
        m_Params.numThreads = std::thread::hardware_concurrency();
    m_Params.numThreads = std::min(m_Params.numThreads, static_cast<int>(totalPermutations - predictedDuplicates));

    printf("%s\n", WCharToUTF8(m_ShaderFileName).c_str());

    for (int i = 0; i < (m_Params.numThreads - 1); i++)
        threads.push_back(std::thread(&Application::ProcessPermutations, this));

    ProcessPermutations();

    for (int i = 0; i < (m_Params.numThreads - 1); i++)
        threads[i].join();

    WriteShaderPermutationsHeader();

    // dump dependencies file if needed
    if (m_Params.deps == L"gcc")
        DumpDepfileGCC();
    else if (m_Params.deps == L"msvc")
        DumpDepfileMSVC();

    printf("%s: Processed %zu shader permutations, found %zu duplicates (%zu found early).\n",
           WCharToUTF8(m_ShaderFileName).c_str(),
           totalPermutations,
           totalPermutations - size_t(m_LastPermutationIndex),
           predictedDuplicates);
    if (totalPermutations - m_LastPermutationIndex < predictedDuplicates)
    {
        printf("\nERROR: Predicted %llu duplicates\n\n\n", predictedDuplicates);
    }
}

std::wstring Application::MakeFullPath(const std::wstring & outputPath, const std::wstring & fileName)
{
    // Append file name, optionally converting to long path again, because the outputPath alone could be normal path, but when filename is added -- it could become a long path
    PWSTR canonicalFileNameRaw = NULL;
    HRESULT hr = PathAllocCombine(outputPath.c_str(), fileName.c_str(), PATHCCH_ALLOW_LONG_PATHS, &canonicalFileNameRaw);

    if (S_OK == hr)
    {
        std::wstring canonicalFileName(canonicalFileNameRaw);
        LocalFree(canonicalFileNameRaw);
        return canonicalFileName;
    }
    return fileName;
}

void Application::GenerateMacroPermutations(std::deque<Permutation>& permutations)
{
    Permutation temp;
    temp.sourcePath = WCharToUTF8(m_Params.inputFile);
    // put the permutation options that appear in shaders first.
    std::stable_partition(m_Params.permutationOptions.begin(), m_Params.permutationOptions.end(), [](const PermutationOption& opt) { return opt.foundInShader; });
    GenerateMacroPermutations(temp, permutations, 0, 0);
}

void Application::GenerateMacroPermutations(Permutation current, std::deque<Permutation>& permutations, int idx, int curBit)
{
    if (idx == m_Params.permutationOptions.size())
    {
        permutations.push_back(current);
        return;
    }

    const PermutationOption& currentOption = m_Params.permutationOptions[idx];

    uint32_t size = currentOption.values.size();

    for (int i = 0; i < size; i++)
    {
        Permutation temp = current;

        if (!currentOption.foundInShader && !temp.identicalTo.has_value() && i != 0)
        {
            // this and all remaining permutations (in this recursion) have identical output to the last real one processed.
            temp.identicalTo = temp.key;
        }

        if (currentOption.values[i][0] != L'-')
        {
            if (currentOption.isNumeric)
            {
                temp.defines.push_back(L"-D");
                temp.defines.push_back(currentOption.definition + L"=" + currentOption.values[i]);
            }
            else
            {
                temp.defines.push_back(L"-D");
                temp.defines.push_back(currentOption.values[i]);
            }
        }

        temp.key |= (i << curBit);

        GenerateMacroPermutations(temp, permutations, idx + 1, curBit + currentOption.numBits);
    }
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

void Application::OpenSourceFile()
{
    size_t slashPos     = m_Params.inputFile.find_last_of('/');

    if (slashPos == std::wstring::npos)
    {
        slashPos = m_Params.inputFile.find_last_of('\\');

        if (slashPos == std::wstring::npos)
            slashPos = 0;
    }

    m_ShaderFileName = m_Params.inputFile.substr(slashPos == 0 ? 0 : slashPos + 1, m_Params.inputFile.size() - slashPos - 1);

    // If a shader name was not provided, use the file name as the shader name.
    if (m_Params.shaderName.empty())
    {
        size_t extensionPos = m_ShaderFileName.find_last_of('.');
        m_ShaderName        = m_ShaderFileName.substr(0, extensionPos);
    }
    else
        m_ShaderName = m_Params.shaderName;

    std::string dxcDll         = WCharToUTF8(m_Params.dxcDll);
    std::string d3dDll         = WCharToUTF8(m_Params.d3dDll);
    std::string glslangExe     = WCharToUTF8(m_Params.glslangExe);
    std::string shaderPath     = WCharToUTF8(m_Params.inputFile);
    std::string shaderName     = WCharToUTF8(m_ShaderName);
    std::string shaderFileName = WCharToUTF8(m_ShaderFileName);
    std::string outputPath     = WCharToUTF8(m_Params.ouputPath);

    if (m_Params.compiler.empty())
    {
        // Check file extension
        size_t       extensionPos = m_Params.inputFile.find_last_of('.');
        std::wstring extension    = m_Params.inputFile.substr(extensionPos + 1, m_Params.inputFile.size() - extensionPos - 1);

        if (extension == L"hlsl")
            m_Compiler = std::unique_ptr<HLSLCompiler>(
                new HLSLCompiler(HLSLCompiler::DXC, dxcDll, shaderPath, shaderName, shaderFileName, outputPath, m_Params.disableLogs, m_Params.debugCompile));
        else if (extension == L"glsl")
            m_Compiler = std::unique_ptr<GLSLCompiler>(new GLSLCompiler(glslangExe, shaderPath, shaderName, shaderFileName, outputPath, m_Params.disableLogs, m_Params.debugCompile));
        else
            throw std::runtime_error("Unknown shader source file extension. Please use the -compiler option to specify which compiler to use.");
    }
    else
    {
        if (m_Params.compiler == L"dxc")
            m_Compiler = std::unique_ptr<HLSLCompiler>(
                new HLSLCompiler(HLSLCompiler::DXC, dxcDll, shaderPath, shaderName, shaderFileName, outputPath, m_Params.disableLogs, m_Params.debugCompile));
        else if (m_Params.compiler == L"gdk.scarlett.x64")
            m_Compiler = std::unique_ptr<HLSLCompiler>(new HLSLCompiler(
                HLSLCompiler::GDK_SCARLETT_X64, dxcDll, shaderPath, shaderName, shaderFileName, outputPath, m_Params.disableLogs, m_Params.debugCompile));
        else if (m_Params.compiler == L"gdk.xboxone.x64")
            m_Compiler = std::unique_ptr<HLSLCompiler>(new HLSLCompiler(
                HLSLCompiler::GDK_XBOXONE_X64, dxcDll, shaderPath, shaderName, shaderFileName, outputPath, m_Params.disableLogs, m_Params.debugCompile));
        else if (m_Params.compiler == L"fxc")
            m_Compiler = std::unique_ptr<HLSLCompiler>(
                new HLSLCompiler(HLSLCompiler::FXC, d3dDll, shaderPath, shaderName, shaderFileName, outputPath, m_Params.disableLogs, m_Params.debugCompile));
        else if (m_Params.compiler == L"glslang")
            m_Compiler = std::unique_ptr<GLSLCompiler>(new GLSLCompiler(glslangExe, shaderPath, shaderName, shaderFileName, outputPath, m_Params.disableLogs, m_Params.debugCompile));
        else
            throw std::runtime_error("Unknown compiler requested (valid options: dxc, fxc or glslang)");
    }

    std::vector<fs::path> includeSearchPaths{};
    for (size_t i = 0; i < m_Params.compilerArgs.size(); i++)
    {
        auto& arg = m_Params.compilerArgs[i];
        if (arg == L"-I" && i + 1 < m_Params.compilerArgs.size())
        {
            includeSearchPaths.emplace_back(m_Params.compilerArgs[++i]);
        }
        else if (StartsWith(arg, L"-I "))
        {
            includeSearchPaths.emplace_back(arg.substr(3));
        }
        else if (StartsWith(arg, L"-I"))
        {
            includeSearchPaths.emplace_back(arg.substr(2));
        }
    }

    // early filter for duplicate permutations
    // find out which of the permutation options are mentioned in the file (+includes).
    if (m_Params.permutationOptions.size() > 0)
    {
        std::vector<std::string> searchFiles{shaderPath};
        std::unordered_set<fs::path> searchedFiles{};
        size_t numDefsFound = 0;
        while (!searchFiles.empty() && numDefsFound < m_Params.permutationOptions.size())
        {
            auto sourceFilename = searchFiles.back();
            searchFiles.pop_back();
            if (!searchedFiles.emplace(sourceFilename).second)
            {
                // already searched this file.
                continue;
            }

            std::ifstream source{sourceFilename};
            std::string line;
            while (std::getline(source, line))
            {
                auto startOfLine = line.find_first_not_of(" \t");
                std::string_view trimmedLine = std::string_view(line).substr(startOfLine > line.size() ? 0 : startOfLine);
                if (StartsWith(trimmedLine, "#include"))
                {
                    auto startOfFile = std::min(trimmedLine.find('"'), trimmedLine.find('<')) + 1;
                    auto endOfFile = std::min(trimmedLine.rfind('"'), trimmedLine.rfind('>'));
                    auto filename = trimmedLine.substr(startOfFile, endOfFile - startOfFile);

                    fs::path includeFilePath;
                    if (FindIncludeFilePath(std::string(filename), includeSearchPaths, includeFilePath))
                    {
                        searchFiles.push_back(includeFilePath.string());
                    }
                }

                for (auto& option : m_Params.permutationOptions)
                {
                    if (!option.foundInShader && Contains(trimmedLine, option.definitionUtf8))
                    {
                        option.foundInShader = true;
                        numDefsFound++;
                    }
                }
            }
        }
    }
    else
    {
        for (auto& option : m_Params.permutationOptions)
            option.foundInShader = true;
    }
}

void Application::ProcessPermutations()
{
    bool running = true;

    // Look over the permutations and compile each one
    while (true)
    {
        m_ReadMutex.lock();

        Permutation permutation;

        if (m_MacroPermutations.empty())
            running = false;
        else
        {
            permutation = m_MacroPermutations.back();
            m_MacroPermutations.pop_back();
        }

        m_ReadMutex.unlock();

        if (running)
            CompilePermutation(permutation);
        else
            break;
    }
}

void Application::CompilePermutation(Permutation& permutation)
{
    if (permutation.identicalTo.has_value())
    {
        std::unique_lock<std::mutex> guard(m_WriteMutex);

        if (auto it = m_KeyToIndexMap.find(*permutation.identicalTo); it != m_KeyToIndexMap.end())
        {
            auto index = it->second;
            m_KeyToIndexMap[permutation.key] = index;
            return;
        }
        else
        {
            guard.unlock();
            // add the permutation back to the end of the queue.
            std::lock_guard<std::mutex> readGuard(m_ReadMutex);
            m_MacroPermutations.push_front(permutation);
            return;
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Setup compiler args.
    // ------------------------------------------------------------------------------------------------
    std::vector<std::string> args = {};

    for (const std::wstring& arg : permutation.defines)
        args.push_back(WCharToUTF8(arg));

    for (const std::wstring& arg : m_Params.compilerArgs)
        args.push_back(WCharToUTF8(arg));

    // ------------------------------------------------------------------------------------------------
    // Print compiler args if requested.
    // ------------------------------------------------------------------------------------------------
    if (m_Params.printArguments)
        PrintPermutationArguments(permutation);

    // ------------------------------------------------------------------------------------------------
    // Compile it with specified arguments.
    // ------------------------------------------------------------------------------------------------
    if (!m_Compiler->Compile(permutation, args, m_WriteMutex))
    {   
        fprintf(stderr, "failed to compile shader : %s\n", permutation.sourcePath.generic_string().c_str());
        throw std::runtime_error("failed to compile shader: " + permutation.sourcePath.generic_string());
    }
        

    // ------------------------------------------------------------------------------------------------
    // Retrieve reflection data
    // ------------------------------------------------------------------------------------------------
    if (m_Params.generateReflection)
        m_Compiler->ExtractReflectionData(permutation);

    bool shouldWrite = false;

    m_WriteMutex.lock();

    // If a permutation with the same shader hash was previously inserted, we can skip writting this to disk.
    if (m_HashToIndexMap.find(permutation.hashDigest) == m_HashToIndexMap.end())
    {
        shouldWrite = true;

        // Assign an index to the current unique permutation.
        m_HashToIndexMap[permutation.hashDigest] = m_LastPermutationIndex++;

        // Add the unique permutations to a vector to make writing the permutations header easier.
        m_UniquePermutations.push_back(permutation);

        m_UniquePermutations.back().shaderBinary.reset();
    }

    // An extra map to make looking up the index of a permutation with its' shader key much easier.
    m_KeyToIndexMap[permutation.key] = m_HashToIndexMap[permutation.hashDigest];

    m_WriteMutex.unlock();

    // ------------------------------------------------------------------------------------------------
    // Write shader binary
    // ------------------------------------------------------------------------------------------------
    if (shouldWrite)
        WriteShaderBinaryHeader(permutation);

    permutation.shaderBinary.reset();
}


void Application::WriteShaderBinaryHeader(Permutation& permutation)
{
    std::string  permutationName = WCharToUTF8(m_ShaderName) + "_" + permutation.hashDigest;
    std::wstring headerFileName  = UTF8ToWChar(permutationName) + L".h";

    FILE* fp = NULL;

    std::wstring outputPath = MakeFullPath(m_Params.ouputPath, headerFileName);

    _wfopen_s(&fp, outputPath.c_str(), L"wb");

    // ------------------------------------------------------------------------------------------------
    // Write autogen comment
    // ------------------------------------------------------------------------------------------------#
    fprintf(fp, "// %s.h.\n", permutationName.c_str());
    fprintf(fp, "// Auto generated by FidelityFX-SC.\n\n");

    // ------------------------------------------------------------------------------------------------
    // Write compiler args
    // ------------------------------------------------------------------------------------------------
    if (m_Params.embedArguments)
    {
        for (int i = 0; i < m_Params.compilerArgs.size(); i++)
        {
            std::string arg = WCharToUTF8(m_Params.compilerArgs[i]);

            if (arg[0] == '-')
            {
                fprintf(fp, "\n// %s", arg.c_str());

                if (arg[1] != 'D')
                    fprintf(fp, " ");
            }
            else
                fprintf(fp, "%s", arg.c_str());
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Write shader options
    // ------------------------------------------------------------------------------------------------
    if (m_Params.embedArguments)
    {
        for (int32_t i = 0; i < permutation.defines.size(); i++)
        {
            std::string arg = WCharToUTF8(permutation.defines[i]);

            if (arg[0] == '-')
            {
                fprintf(fp, "\n// %s", arg.c_str());

                if (arg[1] != 'D')
                    fprintf(fp, " ");
            }
            else
                fprintf(fp, "%s", arg.c_str());
        }

        fprintf(fp, "\n\n");
    }

    // ------------------------------------------------------------------------------------------------
    // Write reflection data
    // ------------------------------------------------------------------------------------------------
    if (m_Params.generateReflection)
        m_Compiler->WriteBinaryHeaderReflectionData(fp, permutation, m_WriteMutex);

    // ------------------------------------------------------------------------------------------------
    // Write shader binary
    // ------------------------------------------------------------------------------------------------
    int32_t shaderBinarySize = permutation.shaderBinary->BufferSize();
    uint8_t* shaderBinary     = permutation.shaderBinary->BufferPointer();

    fprintf(fp, "static const uint32_t g_%s_size = %d;\n\n", permutationName.c_str(), (int)shaderBinarySize);

    fprintf(fp, "static const unsigned char g_%s_data[] = {\n", permutationName.c_str());

    for (int32_t i = 0; i < shaderBinarySize; ++i)
        fprintf(fp, "0x%02x%s", shaderBinary[i], i == shaderBinarySize - 1 ? "" : ((i + 1) % 16 == 0 ? ",\n" : ","));

    fprintf(fp, "\n};\n\n");

    fclose(fp);
}

void Application::PrintPermutationArguments(Permutation& permutation)
{
    m_WriteMutex.lock();

    printf("Permutation Arguments: ");

    if (m_Params.generateReflection)
        printf("-reflection ");

    // ------------------------------------------------------------------------------------------------
    // Print compiler args
    // ------------------------------------------------------------------------------------------------
    if (m_Params.printArguments)
    {
        for (int i = 0; i < m_Params.compilerArgs.size(); i++)
        {
            std::string arg = WCharToUTF8(m_Params.compilerArgs[i]);

            printf("%s", arg.c_str());

            if (arg[1] != 'D')
                printf(" ");
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Print shader options
    // ------------------------------------------------------------------------------------------------
    if (m_Params.printArguments)
    {
        for (int32_t i = 0; i < permutation.defines.size(); i++)
        {
            std::string arg = WCharToUTF8(permutation.defines[i]);

            printf("%s", arg.c_str());

            if (arg[1] != 'D')
                printf(" ");
        }
    }

    std::string outputPath = WCharToUTF8(m_Params.ouputPath);

    printf("-output=%s", outputPath.c_str());

    printf("\n\n");

    m_WriteMutex.unlock();
}

void Application::WriteShaderPermutationsHeader()
{
    if (m_UniquePermutations.empty())
        throw std::runtime_error("No shader permutations generated due to errors!");

    std::string shaderName = WCharToUTF8(m_ShaderName);

    FILE* fp = NULL;

    std::wstring outputPath = MakeFullPath(m_Params.ouputPath, m_ShaderName + L"_permutations.h");

    _wfopen_s(&fp, outputPath.c_str(), L"wb");

    // ------------------------------------------------------------------------------------------------
    // Write header includes
    // ------------------------------------------------------------------------------------------------
    for (int i = 0; i < m_UniquePermutations.size(); i++)
    {
        const Permutation& permutation = m_UniquePermutations[i];

        fprintf(fp, "#include \"%s\"\n", permutation.headerFileName.c_str());
    }

    fprintf(fp, "\n");

    // ------------------------------------------------------------------------------------------------
    // Write shader option enums
    // ------------------------------------------------------------------------------------------------
    for (int i = 0; i < m_Params.permutationOptions.size(); i++)
    {
        const PermutationOption& option = m_Params.permutationOptions[i];

        if (!option.isNumeric)
        {
            std::string enumName = WCharToUTF8(option.definition);

            fprintf(fp, "typedef enum %s {\n", enumName.c_str());

            for (int j = 0; j < option.values.size(); j++)
            {
                std::transform(enumName.begin(), enumName.end(), enumName.begin(), ::toupper);

                std::string valueString = WCharToUTF8(option.values[j]);
                std::transform(valueString.begin(), valueString.end(), valueString.begin(), ::toupper);
                valueString = "OPT_" + enumName + "_" + valueString + " = " + std::to_string(j);

                if (j == option.values.size() - 1)
                    fprintf(fp, "    %s\n", valueString.c_str());
                else
                    fprintf(fp, "    %s,\n", valueString.c_str());
            }

            fprintf(fp, "} %s;\n\n", enumName.c_str());
        }
    }

    // ------------------------------------------------------------------------------------------------
    // Write shader key union
    // ------------------------------------------------------------------------------------------------
    std::string unionName = shaderName + "_PermutationKey";

    fprintf(fp, "typedef union %s {\n", unionName.c_str());

    fprintf(fp, "    struct {\n");

    for (int i = 0; i < m_Params.permutationOptions.size(); i++)
    {
        const PermutationOption& option = m_Params.permutationOptions[i];

        std::string enumName = WCharToUTF8(option.definition);

        fprintf(fp, "        uint32_t %s : %i;\n", enumName.c_str(), option.numBits);
    }

    fprintf(fp, "    };\n");
    fprintf(fp, "    uint32_t index;\n");
    fprintf(fp, "} %s;\n\n", unionName.c_str());

    // ------------------------------------------------------------------------------------------------
    // Write permutation info struct
    // ------------------------------------------------------------------------------------------------
    fprintf(fp, "typedef struct %s_PermutationInfo {\n", shaderName.c_str());
    fprintf(fp, "    const uint32_t       blobSize;\n");
    fprintf(fp, "    const unsigned char* blobData;\n\n");

    if (m_Params.generateReflection)
        m_Compiler->WritePermutationHeaderReflectionStructMembers(fp);

    fprintf(fp, "} %s_PermutationInfo;\n\n", shaderName.c_str());

    // ------------------------------------------------------------------------------------------------
    // Write indirection table
    // ------------------------------------------------------------------------------------------------
    uint32_t usedBits = 0;

    for (const auto& option : m_Params.permutationOptions)
        usedBits += option.numBits;

    uint32_t totalPossiblePermutations = pow(2, usedBits);

    fprintf(fp, "static const uint32_t g_%s_IndirectionTable[] = {\n", shaderName.c_str());

    for (int i = 0; i < totalPossiblePermutations; i++)
        fprintf(fp, "    %i,\n", m_KeyToIndexMap.find(i) == m_KeyToIndexMap.end() ? 0 : m_KeyToIndexMap[i]);

    fprintf(fp, "};\n\n");

    // ------------------------------------------------------------------------------------------------
    // Write permutation info table
    // ------------------------------------------------------------------------------------------------
    if (m_UniquePermutations.size() > 0)
    {
        fprintf(fp, "static const %s_PermutationInfo g_%s_PermutationInfo[] = {\n", shaderName.c_str(), shaderName.c_str());

        for (int i = 0; i < m_UniquePermutations.size(); i++)
        {
            const Permutation& permutation = m_UniquePermutations[i];

            std::string permutationName = shaderName + "_" + permutation.hashDigest;

            fprintf(fp, "    { g_%s_size, g_%s_data, ", permutationName.c_str(), permutationName.c_str());

            if (m_Params.generateReflection)
                m_Compiler->WritePermutationHeaderReflectionData(fp, permutation);

            fprintf(fp, "},\n");
        }

        fprintf(fp, "};\n\n");
    }

    fclose(fp);
}

void Application::DumpDepfileGCC()
{
    if (m_UniquePermutations.empty())
        throw std::runtime_error("No shader permutations generated due to errors!");

    std::unordered_set<std::string> totalDependencies;

    for (auto& permutation : m_UniquePermutations)
    {
        totalDependencies.insert(permutation.dependencies.begin(), permutation.dependencies.end());
    }

    std::string shaderName = WCharToUTF8(m_ShaderName);

    FILE* fp = NULL;

    std::wstring outputFilename = MakeFullPath(m_Params.ouputPath, m_ShaderName + L"_permutations.h");
    std::wstring depfilePath = outputFilename + L".d";

    _wfopen_s(&fp, depfilePath.c_str(), L"wb");

    fs::path output = WCharToUTF8(outputFilename);

    output = fs::absolute(output);

    fprintf(fp, "%s:", output.generic_string().c_str());

    for (auto& dependency : totalDependencies)
    {
        fprintf(fp, " %s", dependency.c_str());
    }

    fclose(fp);
}

void Application::DumpDepfileMSVC()
{
    assert(false);

    printf("MSVC depfile not implemented yet.\n");
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        if (argc <= 1)
        {
            LaunchParameters::PrintCommandLineSyntax();
            return 1;
        }

        LaunchParameters params;
        params.ParseCommandLine(argc - 1, argv + 1);

        Application app(params);
        app.Process();

        return 0;
    }
    catch (const std::exception& ex)
    {   
        fprintf(stderr, "ffx_sc failed: %s\n", ex.what());
        fflush(stderr);
        return -1;
    }
}
