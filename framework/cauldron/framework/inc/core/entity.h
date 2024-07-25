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

#include "misc/helpers.h"
#include "misc/math.h"

#include <string>
#include <vector>

namespace cauldron
{
    class Component;
    class ComponentMgr;

    /**
     * @class Entity
     *
     * Represents an entity instance. An entity is any node that is present in our scene
     * representation. It can be a simple transform or contain multiple components adding 
     * various functionality support to the entity (i.e. Mesh, Lighting, Camera, Animation, etc.)
     *
     * @ingroup CauldronCore
     */
    class Entity
    {
    public:

        /**
         * @brief   Constructor. Sets the entity's name and parent in the scene hierarchy (if present)
         */
        Entity(const wchar_t* name, Entity* parent = nullptr);

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~Entity();

        /**
         * @brief   Sets the entity's transform.
         */
        void SetTransform(const Mat4& transform) { m_RootTransform = transform; }

        /**
         * @brief   Sets the entity's previous transform.
         */
        void SetPrevTransform(const Mat4& transform) { m_RootPrevTransform = transform; }

        /**
         * @brief   Gets the entity's current transform.
         */
        Mat4& GetTransform() { return m_RootTransform; }
        const Mat4& GetTransform() const { return m_RootTransform; }

        /**
         * @brief   Gets the entity's previous transform.
         */
        Mat4& GetPrevTransform() { return m_RootPrevTransform; }
        const Mat4& GetPrevTransform() const { return m_RootPrevTransform; }

        /**
         * @brief   Sets an entity's parent entity.
         */
        void SetParent(Entity* parent) { m_pParent = parent; }

        /**
         * @brief   Gets an entity's parent entity.
         */
        Entity* GetParent() { return m_pParent; }

        /**
         * @brief   Query entity status.
         */
        bool IsActive() const { return m_Active; }

        /**
         * @brief   Sets entity status.
         */
        void SetActive(bool active) { m_Active = active; }

        /**
         * @brief   Gets the entity's name.
         */
        const wchar_t* GetName() const { return m_Name.c_str(); }

        /**
         * @brief   Adds a child entity to this entity (parent).
         */
        void AddChildEntity(Entity* pEntity) { m_Children.push_back(pEntity); }

        /**
         * @brief   Returns the child count of an entity.
         */
        uint32_t GetChildCount() const { return static_cast<uint32_t>(m_Children.size()); }

        /**
         * @brief   Returns the list of children from an entity.
         */
        const std::vector<Entity*>& GetChildren() const { return m_Children; }

        /**
         * @brief   Adds a component to the entity.
         */
        void AddComponent(Component* pComponent);

        /**
         * @brief   Removes a component from the entity.
         */
        void RemoveComponent(Component* pComponent);

        /**
         * @brief   Checks if an entity has a specified component type (matched to <c><i>ComponentMgr</i></c>).
         */
        bool HasComponent(const ComponentMgr* pManager) const;

        /**
         * @brief   Retrieves a component from an entity by type (matched to <c><i>ComponentMgr</i></c>).
         */
        template<typename T>
        T* GetComponent(const ComponentMgr* pManager) const
        {
            return reinterpret_cast<T*>(GetComponent(pManager));
        }

    private:
        Component* GetComponent(const ComponentMgr* pManager) const;

        Entity() = delete;

        // No Copy, No Move
        NO_COPY(Entity);
        NO_MOVE(Entity);

        Entity*         m_pParent;

        Mat4            m_RootTransform;
        Mat4            m_RootPrevTransform;

        std::wstring    m_Name;
        bool            m_Active = true;

        std::vector<Entity*>    m_Children;
        std::vector<Component*> m_Components;
    };

} // namespace cauldron
