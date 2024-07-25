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

#include "core/uimanager.h"
#include "core/framework.h"
#include "core/uibackend.h"

namespace cauldron
{
    UIManager::UIManager()
    {
        // Create the UI back end
        m_pUIBackEnd = UIBackend::CreateUIBackend();
    }

    UIManager::~UIManager()
    {
        for (auto& i : GetGeneralLayout())
        {
            i.second->Release();
        }

        delete m_pUIBackEnd;
    }

    void UIManager::Update(double deltaTime)
    {
        if (!m_pUIBackEnd->Ready())
            return;

        m_ProcessingUI = true;

        // Does back end updates, sets up input for the platform
        // and calls all UI building blocks
        m_pUIBackEnd->Update(deltaTime);

        m_ProcessingUI = false;
    }

    bool UIManager::UIBackendMessageHandler(void* pMessage)
    {
        return m_pUIBackEnd->MessageHandler(pMessage);
    }

    UISection ::~UISection()
    {
        for (auto& i : GetElements())
        {
            i.second->Release();
        }
    }

    UISection* UIManager::RegisterUIElements(const char* name, UISectionType type)
    {
        UISection* uiSection = CreateUIElements(name, type);
        RegisterUIElements(uiSection);
        return uiSection;
    }

    UISection* UIManager::CreateUIElements(const char* name, UISectionType type)
    {   
        uint64_t priority = type != UISectionType::Sample ? LowestPriority : 0;
        priority <<= 32;
        return new UISection(priority | m_SectionIDGenerator++, name, type);
    }


} // namespace cauldron
