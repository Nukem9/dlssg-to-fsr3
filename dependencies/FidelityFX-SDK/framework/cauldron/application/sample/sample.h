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

#include "core/framework.h"

class Sample : public cauldron::Framework
{
public:
    Sample(const cauldron::FrameworkInitParams* pInitParams) : cauldron::Framework(pInitParams) {}
    virtual ~Sample() = default;

    // Overrides
    virtual void ParseSampleConfig() override;
    virtual void ParseSampleCmdLine(const wchar_t* cmdLine) override;
    virtual void RegisterSampleModules() override;

    virtual void DoSampleInit() override;
    virtual void DoSampleUpdates(double deltaTime) override;
    virtual void DoSampleResize(const cauldron::ResolutionInfo& resInfo) override;
    virtual void DoSampleShutdown() override {}
};
