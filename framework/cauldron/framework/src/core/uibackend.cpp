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

#include "core/uibackend.h"
#include "core/entity.h"
#include "core/framework.h"
#include "core/inputmanager.h"

#include <algorithm>

namespace cauldron
{
    UIBackend::UIBackend()
    {
        // Init filters
        memset(m_FilterEnabled, 1, sizeof(bool) * LOGLEVEL_COUNT);
    }

    UIBackend::~UIBackend()
    {
    }

    void UIBackend::Update(double deltaTime)
    {
        // Do platform specific updates for back end
        PlatformUpdate(deltaTime);

        // Try to keep things in relatively the same place even if resolution changes
        const ResolutionInfo resInfo = GetFramework()->GetResolutionInfo();
        Vec2 resScale(resInfo.fDisplayWidth() / 1920.f, resInfo.fDisplayHeight() / 1080.f);   // Percentage higher/lower from 1080p

        // Trigger start of a new frame
        BeginUIUpdates();

        // Test input for enabling/disabling ui elements
        const InputState& inputState = GetInputManager()->GetInputState();
        if (inputState.GetKeyUpState(Key_F1))
            m_ShowTabbedDialog = !m_ShowTabbedDialog;
        if (inputState.GetKeyUpState(Key_F2))
            m_ShowPerfDialog = !m_ShowPerfDialog;
        if (inputState.GetKeyUpState(Key_F3))
            m_ShowOutputDialog = !m_ShowOutputDialog;

        // Setup the tabbed frame that will render most of our registered UI
        if (m_ShowTabbedDialog)
            BuildTabbedDialog(resScale);

        // Setup the performance results dialog
        if (m_ShowPerfDialog)
            BuildPerfDialog(resScale);

        // Output window (enabled by default in debug)
        if (m_ShowOutputDialog)
            BuildOutputDialog(resScale);

        // Done with updates
        EndUIUpdates();
    }

} // namespace cauldron
