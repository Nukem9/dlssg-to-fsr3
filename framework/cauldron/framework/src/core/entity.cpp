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

#include "core/entity.h"
#include "misc/assert.h"
#include "core/component.h"

namespace cauldron
{
    Entity::Entity(const wchar_t* name,
        Entity* parent) :
        m_RootTransform(Mat4::identity()),
        m_Name(name),
        m_pParent(parent)
    {
    }

    Entity::~Entity()
    {
        // Remove all components on the entity
        m_Components.clear();   // Component memory is backed outside its management

        // Also delete all child entities
        for (auto childItr = m_Children.begin(); childItr != m_Children.end(); ++childItr)
            delete(*childItr);
        m_Children.clear();
    }

    void Entity::AddComponent(Component* pComponent)
    {
        CauldronAssert(ASSERT_CRITICAL, pComponent->GetOwner() == this, L"Appending a component which belongs to another entity.");

        // check that a component of the same type isn't already there
#if _DEBUG
        const ComponentMgr* pComponentManager = pComponent->GetManager();
        for (auto it = m_Components.begin(); it != m_Components.end(); ++it)
            CauldronAssert(ASSERT_CRITICAL, (*it)->GetManager() != pComponentManager, L"A component of the same type already exists in this entity.");
#endif // _DEBUG

        // Add the component to the entity's list of components
        m_Components.push_back(pComponent);
    }

    void Entity::RemoveComponent(Component* pComponent)
    {
        CauldronAssert(ASSERT_CRITICAL, pComponent->GetOwner() == this, L"Removing a component which belongs to another entity.");

        for (auto it = m_Components.begin(); it != m_Components.end(); ++it)
        {
            if (*it == pComponent)
            {
                m_Components.erase(it);
                return;
            }
        }
    }

    bool Entity::HasComponent(const ComponentMgr* pManager) const
    {
        for (auto pComponent : m_Components)
        {
            if (pComponent->GetManager() == pManager)
                return true;
        }
        return false;
    }

    Component* Entity::GetComponent(const ComponentMgr* pManager) const
    {
        for (auto pComponent : m_Components)
        {
            if (pComponent->GetManager() == pManager)
                return pComponent;
        }
        return nullptr;
    }

} // namespace cauldron
