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

#include "core/inputmanager.h"

namespace cauldron
{
    class InputManagerInternal : public InputManager
    {
    public:
        InputManagerInternal();
        virtual ~InputManagerInternal();
        
        void PushWheelChange(int64_t wheelChange);

    protected:

        // This function, called from Update(), needs to be implemented for each platform
        virtual void PollInputStates() override;

    private:
        uint32_t CauldronToWinKeyMapping(KeyboardInputMappings inputID);

        float GetStickValue(int32_t axisValue, int32_t inputDeadZone);
        float GetTriggerValue(int32_t axisValue);

    protected:
        
        uint32_t    m_ChangeNumber = 0; // Used to track changes in input state
        int64_t     m_WheelDelta = 0;
        std::mutex  m_WheelMutex;
        bool        m_CapturingState = false;
    };

} // namespace cauldron
