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

#include "core/entity.h"
#include "misc/helpers.h"

#include <vector>

namespace cauldron
{
    class ComponentMgr;
    class Entity;

    /// @defgroup CauldronComponent Components
    /// FidelityFX Cauldron Framework Components Reference Documentation
    ///
    /// @ingroup CauldronCore

    /**
     * @struct ComponentData
     *
     * Base ComponentData structure. Overridden by each <c><i>Component</i></c> to represent the needed setup data.
     *
     * @ingroup CauldronComponent
     */
    struct ComponentData
    {
    };
    
    /**
     * @class Component
     *
     * Base class from which all components inherit.
     *
     * @ingroup CauldronComponent
     */
    class Component
    {
    public:

        /**
         * @brief   Component Destruction.
         */
        virtual ~Component() {}

        /**
         * @brief   Gets the component's owner <c><i>Entity</i></c> instance.
         */
        Entity* GetOwner() const { return m_pOwner; }

        /**
         * @brief   Get the <c><i>ComponentMgr</i></c> associated with this component type.
         */
        ComponentMgr* GetManager() const { return m_pManager; }

        /**
         * @brief   Gets the component's type.
         */
        const wchar_t* GetType() const;
        
        /**
         * @brief   Component Update function. Must be overridden by each derived component class.
         */
        virtual void Update(double deltaTime) = 0;

        /**
         * @brief   Component focus lost event.
         */
        virtual void OnFocusLost() {};

        /**
         * @brief   Component focus gained event.
         */
        virtual void OnFocusGained() {};

    private:
        Component() = delete;

        // No Copy, No Move
        NO_COPY(Component);
        NO_MOVE(Component);

    protected:
        Component(Entity* pOwner, ComponentData* pData, ComponentMgr* pManager);

        Entity*       m_pOwner = nullptr;       // Keep a pointer on our owner
        ComponentMgr* m_pManager = nullptr;
    };

    /**
     * @class ComponentMgr
     *
     * Base class from which all component managers inherit.
     *
     * @ingroup CauldronComponent
     */
    class ComponentMgr
    {
    public:

        /**
         * @brief   ComponentMgr construction.
         */
        ComponentMgr();

        /**
         * @brief   ComponentMgr destruction.
         */
        virtual ~ComponentMgr();
        
        /**
         * @brief   Spawn's a new component. Must be overridden by derived classes.
         */
        virtual Component* SpawnComponent(Entity* pOwner, ComponentData* pData) = 0;

        /**
         * @brief   Get the component type string representation.
         */
        virtual const wchar_t* ComponentType() const = 0;

        /**
         * @brief   Get the number of managed components.
         */
        uint32_t GetComponentCount() const { return static_cast<uint32_t>(m_ManagedComponents.size()); }

        /**
         * @brief   Get the component associated with a specific <c><i>Entity</i></c> instance.
         */
        Component* GetComponent(const Entity* pEntity) const;

        /**
         * @brief   Get the full list of managed components from this ComponentMgr.
         */
        const std::vector<Component*>& GetComponentList() const { return m_ManagedComponents; }
        
        /**
         * @brief   ComponentMgr Initialization. Override as needed.
         */
        virtual void Initialize() {}

        /**
         * @brief   ComponentMgr Shutdown. Override as needed.
         */
        virtual void Shutdown() {}

        /**
         * @brief   Updates all managed components. Override to {} when update support isn't needed
         *          or to support other functionality.
         */
        virtual void UpdateComponents(double deltaTime);

        /**
         * @brief   Indicates the ComponentMgr should start managing the passed in <c><i>Component</i></c>.
         */
        void StartManagingComponent(Component* pComponent);

        /**
         * @brief   Indicates the ComponentMgr should stop managing the passed in <c><i>Component</i></c>.
         */
        void StopManagingComponent(Component* pComponent);

        /**
         * @brief   Focus lost.
         */
        void OnFocusLost();

        /**
         * @brief   Focus gained.
         */
        void OnFocusGained();

    private:

        // No Copy, No Move
        NO_COPY(ComponentMgr);
        NO_MOVE(ComponentMgr);

    protected:
        std::vector<Component*> m_ManagedComponents;
    };

} // namespace cauldron
