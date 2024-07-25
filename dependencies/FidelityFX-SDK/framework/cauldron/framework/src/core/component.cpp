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

#include "core/component.h"
#include "core/entity.h"
#include "core/framework.h"
#include "misc/assert.h"

#include <functional>

namespace cauldron
{
    Component::Component(Entity* pOwner, ComponentData* pData, ComponentMgr* pManager)
        : m_pOwner{ pOwner }
        , m_pManager{ pManager }
    {
        CauldronAssert(ASSERT_CRITICAL, pManager != nullptr, L"The component manager is null");
    }

    const wchar_t* Component::GetType() const
    {
        return m_pManager->ComponentType();
    }

    ComponentMgr::ComponentMgr()
    {
        
    }

    ComponentMgr::~ComponentMgr()
    {
        // There should NOT be anything left here at this point
        CauldronAssert(ASSERT_ERROR, m_ManagedComponents.empty(), L"Component Manager is not empty at destruction time!");
    }

    void ComponentMgr::StartManagingComponent(Component* pComponent)
    {
#if _DEBUG
        // Make sure it's not already being managed
        for (auto* pComp : m_ManagedComponents)
        {
            if (pComp == pComponent)
            {
                CauldronError(L"Duplicate component being added to %ls manager", ComponentType());
                return;
            }
        }
#endif // _DEBUG

        m_ManagedComponents.push_back(pComponent);
    }

    void ComponentMgr::StopManagingComponent(Component* pComponent)
    {
        // Find this component
        std::vector<Component*>::iterator iter = m_ManagedComponents.begin();
        while (iter != m_ManagedComponents.end())
        {
            if (*iter == pComponent)
            {
                // Remove it from our own internal list
                m_ManagedComponents.erase(iter);
                return;
            }

            ++iter; // Next
        }

        // Error if we get here (unless we are shutting down -- possible we are stopping to manage components that were loaded but never started being managed)
        CauldronAssert(ASSERT_ERROR, !GetFramework()->IsRunning(), L"Could not find component for removal");
    }

    void ComponentMgr::UpdateComponents(double deltaTime)
    {
        // Update all components
        std::vector<Component*>::iterator iter  = m_ManagedComponents.begin();
        while (iter != m_ManagedComponents.end())
        {
            (*iter)->Update(deltaTime);
            ++iter; // Next
        }
    }

    Component* ComponentMgr::GetComponent(const Entity* pEntity) const
    {
        for (auto iter = m_ManagedComponents.begin(); iter != m_ManagedComponents.end(); ++iter)
            if ((*iter)->GetOwner() == pEntity)
                return (*iter);

        // Wasn't found
        return nullptr;
    }

    void ComponentMgr::OnFocusLost()
    {
        std::vector<Component*>::iterator iter  = m_ManagedComponents.begin();
        while (iter != m_ManagedComponents.end())
        {
            (*iter)->OnFocusLost();
            ++iter; // Next
        }
    }

    void ComponentMgr::OnFocusGained()
    {
        std::vector<Component*>::iterator iter  = m_ManagedComponents.begin();
        while (iter != m_ManagedComponents.end())
        {
            (*iter)->OnFocusGained();
            ++iter; // Next
        }
    }
} // namespace cauldron
