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
#include "misc/log.h"
#include "misc/math.h"

namespace cauldron
{
    static const int32_t s_UIDialogXSpacing = 10;
    static const int32_t s_UIDialogYSpacing = 10;
    
    static const int32_t s_UITabDialogWidth = 400;
    static const int32_t s_UITabDialogHeight = 700;

    static const int32_t s_UIPerfDialogWidth = s_UITabDialogWidth;

    /**
     * @class UIBackend
     *
     * The base class from which platform-specific UI backends need to derive.
     *
     * @ingroup CauldronCore
     */
    class UIBackend
    {
    public:

        /**
         * @brief   Constructor with default behavior.
         */
        UIBackend();

        /**
         * @brief   Destructor with default behavior.
         */
        virtual ~UIBackend();

        /**
         * @brief   Creates a platform-specific UIBackend. Function must be implemented for each platform supported.
         */
        static UIBackend* CreateUIBackend();

        /**
         * @brief   Queries if the UI backend is ready.
         */
        virtual bool Ready() const { return m_BackendReady.load(); }

        /**
         * @brief   Frame update function for the UIBackend.
         */
        virtual void Update(double deltaTime);
        
        /**
         * @brief   Platform-specific update function. Must be overridden per platform.
         */
        virtual void PlatformUpdate(double deltaTime) = 0;

        /**
         * @brief   Platform-specific message handler function. Must be overridden per platform.
         */
        virtual bool MessageHandler(const void* pMessage) = 0;

    protected:

        // Begins UI updates for frame
        virtual void BeginUIUpdates() = 0;

        // Ends UI updates for frame
        virtual void EndUIUpdates() = 0;

        // Build out the tabbed window dialog which represent all registered UI elements
        virtual void BuildTabbedDialog(Vec2 uiscale) = 0;

        // Builds the performance dialog
        virtual void BuildPerfDialog(Vec2 uiscale) = 0;

        // Builds the output log dialog
        virtual void BuildOutputDialog(Vec2 uiscale) = 0;

        // Builds the general tab
        virtual void BuildGeneralTab() = 0;

        // Builds the scene tab
        virtual void BuildSceneTab() = 0;

    private:
        // No Copy, No Move
        NO_COPY(UIBackend);
        NO_MOVE(UIBackend);

    protected:

        std::atomic_bool    m_BackendReady = false;

        bool    m_ShowTabbedDialog = true;
        bool    m_ShowPerfDialog = true;

        // Show the output window by default in Debug
#if _DEBUG
        bool    m_ShowOutputDialog = true;
#else
        bool    m_ShowOutputDialog = false;
#endif // _DEBUG
        bool    m_OutputAutoScroll = true;
        bool    m_FilterEnabled[LOGLEVEL_COUNT];
    };

} // namespace cauldron
