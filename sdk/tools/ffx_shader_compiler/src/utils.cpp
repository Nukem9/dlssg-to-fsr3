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

#include "utils.h"

std::string WCharToUTF8(const std::wstring& wstr)
{
    if (wstr.empty())
        return std::string();

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.size(), nullptr, 0, nullptr, nullptr);

    std::string str;
    str.resize(size);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), wstr.size(), &str[0], size, nullptr, nullptr);

    return str;
}

std::wstring UTF8ToWChar(const std::string& str)
{
    if (str.empty())
        return std::wstring();

    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), nullptr, 0);

    std::wstring wstr;
    wstr.resize(size);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.size(), &wstr[0], size);

    return wstr;
}
