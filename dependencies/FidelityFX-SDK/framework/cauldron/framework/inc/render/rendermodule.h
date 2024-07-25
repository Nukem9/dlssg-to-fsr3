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

#include "render/renderdefines.h"
#include "misc/helpers.h"

#include "json/json.h"
using json = nlohmann::ordered_json;

namespace cauldron
{
    class CommandList;

    /**
     * @class RenderModule
     *
     * The <c><i>FidelityFX Cauldron Framework</i></c> render module base class. All render features inherit from this base class
     * in order to execute gpu workloads.
     *
     * @ingroup CauldronRender
     */
    class RenderModule
    {
    public:

        /**
         * @brief   Construction. Does no setup.
         */
        RenderModule(const wchar_t* pName);

        /**
         * @brief   Destruction with default behavior.
         */
        virtual ~RenderModule() = default;

        /**
         * @brief   Rendermodule initialization function. This is where all setup code needs to happen.
         */
        virtual void Init(const json& initData) {}

        /**
         * @brief   Sets the enabled state of the render module.
         */
        virtual void EnableModule(bool enabled) { m_ModuleEnabled = enabled; }

        /**
         * @brief   Update the render module UI before Execution.
         */
        virtual void UpdateUI(double deltaTime) {};

        /**
         * @brief   Executes the render module.
         */
        virtual void Execute(double deltaTime, CommandList* pCmdList) = 0;

        /**
         * @brief   Callback used when OnResize events occur.
         */
        virtual void OnResize(const ResolutionInfo& resInfo) {}

        /**
         * @brief   Callback used when OnFocusLost events occur.
         */
        virtual void OnFocusLost() {}

        /**
         * @brief   Callback used when OnFocusGained events occur.
         */
        virtual void OnFocusGained() {}

        virtual void OnPreFrame() {}

        /**
         * @brief   Returns true if the render module is ready for execution.
         */
        bool ModuleReady() const { return m_ModuleReady; }

        /**
         * @brief   Returns true if the render module is enabled.
         */
        bool ModuleEnabled() const { return m_ModuleEnabled; }


        /**
         * @brief   Returns the render module name.
         */
        const wchar_t* GetName() const { return m_Name.c_str(); }

    private:
        // No copy, No move
        NO_COPY(RenderModule)
        NO_MOVE(RenderModule)

        std::atomic_bool    m_ModuleReady   = false;
        std::atomic_bool    m_ModuleEnabled = true;

    protected:
        RenderModule() = default;

        void SetModuleReady(bool state) { m_ModuleReady = state; }
        void SetModuleEnabled(bool state) { m_ModuleEnabled = state; }

    protected:

        std::wstring        m_Name = L"";
    };

    /// Type definition for RenderModule creation
    ///
    template<typename T> RenderModule* Create() { return new T(); }

    /**
     * @class RenderModuleFactory
     *
     * Factory class for RenderModule registration and creation.
     *
     * @ingroup CauldronRender
     */
    class RenderModuleFactory
    {
    public:
        typedef std::map<std::string, RenderModule*(*)()> ModuleConstructorMap;

        /**
         * @brief   Creates a render module instance of the correct type.
         */
        static RenderModule* CreateInstance(std::string const& renderModuleName)
        {
            ModuleConstructorMap::iterator it = GetConstructorMap().find(renderModuleName);
            if (it == GetConstructorMap().end())
                return 0;
            return it->second();
        }

        /**
         * @brief   Registers a render module type for creation.
         */
        template<typename T>
        static void RegisterModule(std::string const& renderModuleName)
        {
            m_ConstructionMap.insert(std::make_pair(renderModuleName, &Create<T>));
        }

    protected:
        static ModuleConstructorMap& GetConstructorMap()
        {
            return m_ConstructionMap;
        }

    private:
        static ModuleConstructorMap m_ConstructionMap;
    };

} //namespace cauldron
