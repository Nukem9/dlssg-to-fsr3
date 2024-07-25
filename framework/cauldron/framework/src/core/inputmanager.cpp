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

#include "core/inputmanager.h"
#include "misc/assert.h"

namespace cauldron
{
    InputManager::InputManager()
    {

    }

    InputManager::~InputManager()
    {

    }

    void InputManager::Update()
    {
        // Update the current frame index
        m_CurrentStateID = (m_CurrentStateID + 1) % s_InputStateCacheSize;

        // Clear the current input state
        memset(&m_InputStateRep[m_CurrentStateID], 0, sizeof(InputState));

        // Go through platform specific device polling
        PollInputStates();

        // Reset m_IgnoreFrameInputs flag in case it was set
        m_IgnoreFrameInputs = false;
    }

    // Query input state for frame (or cached frames)
    const InputState& InputManager::GetInputState(int frameOffset/*= 0*/) const
    {
        int32_t frameID = m_CurrentStateID;

        // It doesn't matter if input is negative or positive, we're using it to go back
        CauldronAssert(ASSERT_CRITICAL, abs(frameOffset) < s_InputStateCacheSize, L"Requesting frameOffset > number of cached states. Out of bound indexing imminent.");
        if (frameOffset)
        {
            frameID = m_CurrentStateID - abs(frameOffset);
            if (frameID < 0)
                frameID += s_InputStateCacheSize;
        }

        return m_InputStateRep[frameID];
    }

} // namespace cauldron
