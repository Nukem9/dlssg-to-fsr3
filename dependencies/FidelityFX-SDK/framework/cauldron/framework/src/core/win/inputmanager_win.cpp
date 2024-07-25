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

// Only compile for windows
#if defined(_WINDOWS)
    
#include "core/win/inputmanager_win.h"
#include "core/win/framework_win.h"

#include "imgui/imgui.h"

#include <windows.h>
#include <Xinput.h>


namespace cauldron
{
    //////////////////////////////////////////////////////////////////////////
    // Helpers

    uint32_t InputManagerInternal::CauldronToWinKeyMapping(KeyboardInputMappings inputID)
    {
        static uint32_t s_CauldronMapping[Key_Count] = {
            0x30/*Key_0*/,
            0x31/*Key_1*/,
            0x32/*Key_2*/,
            0x33/*Key_3*/,
            0x34/*Key_4*/,
            0x35/*Key_5*/,
            0x36/*Key_6*/,
            0x37/*Key_7*/,
            0x38/*Key_8*/,
            0x39/*Key_9*/,
            0x41/*Key_A*/,
            0x42/*Key_B*/,
            0x43/*Key_C*/,
            0x44/*Key_D*/,
            0x45/*Key_E*/,
            0x46/*Key_F*/,
            0x47/*Key_G*/,
            0x48/*Key_H*/,
            0x49/*Key_I*/,
            0x4A/*Key_J*/,
            0x4B/*Key_K*/,
            0x4C/*Key_L*/,
            0x4D/*Key_M*/,
            0x4E/*Key_N*/,
            0x4F/*Key_O*/,
            0x50/*Key_P*/,
            0x51/*Key_Q*/,
            0x52/*Key_R*/,
            0x53/*Key_S*/,
            0x54/*Key_T*/,
            0x55/*Key_U*/,
            0x56/*Key_V*/,
            0x57/*Key_W*/,
            0x58/*Key_X*/,
            0x59/*Key_Y*/,
            0x5A/*Key_Z*/,
            VK_BACK/*Key_Backspace*/,
            VK_TAB/*Key_Tab*/,
            VK_RETURN/*Key_Enter*/,
            VK_SHIFT/*Key_Shift*/,
            VK_CONTROL/*Key_Ctrl*/,
            VK_MENU/*Key_Alt*/,
            VK_PAUSE/*Key_Pause*/,
            VK_CAPITAL/*Key_CapsLock*/,
            VK_SPACE/*Key_Space*/,
            VK_SNAPSHOT/*Key_PrintScreen*/,
            VK_LEFT/*Key_Left*/,
            VK_UP/*Key_Up*/,
            VK_RIGHT/*Key_Right*/,
            VK_DOWN/*Key_Down*/,
            VK_F1/*Key_F1*/,
            VK_F2/*Key_F2*/,
            VK_F3/*Key_F3*/,
            VK_F4/*Key_F4*/,
            VK_F5/*Key_F5*/,
            VK_F6/*Key_F6*/,
            VK_F7/*Key_F7*/,
            VK_F8/*Key_F8*/,
            VK_F9/*Key_F9*/,
            VK_F10/*Key_F10*/,
            VK_F11/*Key_F11*/,
            VK_F12/*Key_F12*/ };

        return s_CauldronMapping[inputID];
    }

    //////////////////////////////////////////////////////////////////////////
    // Input Manager

    InputManager* InputManager::CreateInputManager()
    {
        return new InputManagerInternal();
    }

    InputManagerInternal::InputManagerInternal() :
        InputManager()
    {

    }

    InputManagerInternal::~InputManagerInternal()
    {

    }

    void InputManagerInternal::PushWheelChange(int64_t wheelChange)
    {
        std::unique_lock<std::mutex> lock(m_WheelMutex);
        m_WheelDelta += wheelChange;
    }

    float InputManagerInternal::GetStickValue(int32_t axisValue, int32_t inputDeadZone)
    {
        int32_t stickInput;
        if (axisValue > 0)
            stickInput = std::max(0, axisValue - inputDeadZone);
        else
            stickInput = std::min(0, axisValue + inputDeadZone);

        int32_t stickRange = 32767 - inputDeadZone;

        return std::max(std::min((float)stickInput / (float)stickRange, 1.f), -1.f );
    }

    float InputManagerInternal::GetTriggerValue(int32_t axisValue)
    {
        float triggerInput = (axisValue >= XINPUT_GAMEPAD_TRIGGER_THRESHOLD)? axisValue - XINPUT_GAMEPAD_TRIGGER_THRESHOLD : 0.f;
        static float s_FullValue = 255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD;

        return triggerInput / s_FullValue;
    }

    void InputManagerInternal::PollInputStates()
    {
        // Don't poll input if we are not active
        if (GetForegroundWindow() != GetFramework()->GetImpl()->GetHWND())
            return;

        // Use last frame information to calculate deltas and button release states
        uint32_t prevFrameID = (m_CurrentStateID == 0) ? s_InputStateCacheSize - 1 : m_CurrentStateID - 1;

        // Because we are using ImGui, we need to poll whether or not to ignore certain I/O for frame
        ImGuiIO& io = ImGui::GetIO();

        // Clear input state for keyboard for the frame
        m_InputStateRep[m_CurrentStateID].KeyboardState = 0;
        m_InputStateRep[m_CurrentStateID].KeyboardUpState = 0;

        // Poll all mapped keyboard keys if not hijacked by UI
        if (!io.WantCaptureKeyboard && !m_IgnoreFrameInputs)
        {
            for (uint32_t keyID = 0; keyID < static_cast<uint32_t>(Key_Count); ++keyID)
            {
                if (GetKeyState(CauldronToWinKeyMapping(static_cast<KeyboardInputMappings>(keyID))) < 0)
                    m_InputStateRep[m_CurrentStateID].KeyboardState |= (1ull << static_cast<uint64_t>(keyID));      // Assign pressed

                // Set keyboard button release state information
                else if (m_InputStateRep[prevFrameID].GetKeyState(static_cast<KeyboardInputMappings>(keyID)))
                    m_InputStateRep[m_CurrentStateID].KeyboardUpState |= (1ull << static_cast<uint64_t>(keyID));    // Assign up
            }
        }

        // Clear input state for mouse for the frame
        m_InputStateRep[m_CurrentStateID].Mouse = { 0 };

        // Reset for next frame
        int64_t wheelDelta;
        {
            std::unique_lock<std::mutex> lock(m_WheelMutex);
            wheelDelta = m_WheelDelta;
            m_WheelDelta = 0;
        }

        // Poll mouse inputs if not hijacked by UI
        if (!io.WantCaptureMouse && !m_IgnoreFrameInputs)
        {
            bool lButtonDown = (GetKeyState(VK_LBUTTON) & 0x80) ? true : false;
            bool rButtonDown = (GetKeyState(VK_RBUTTON) & 0x80) ? true : false;
            bool mButtonDown = (GetKeyState(VK_MBUTTON) & 0x80) ? true : false;

            if (lButtonDown)
                m_InputStateRep[m_CurrentStateID].Mouse.ButtonState |= 1 << static_cast<uint32_t>(Mouse_LButton);
            if (rButtonDown)
                m_InputStateRep[m_CurrentStateID].Mouse.ButtonState |= 1 << static_cast<uint32_t>(Mouse_RButton);
            if (mButtonDown)
                m_InputStateRep[m_CurrentStateID].Mouse.ButtonState |= 1 << static_cast<uint32_t>(Mouse_MButton);

            // Set mouse button release state information
            for (uint32_t buttonID = 0; buttonID < static_cast<uint32_t>(Mouse_ButtonCount); ++buttonID)
                m_InputStateRep[m_CurrentStateID].Mouse.ButtonUpState |= ((m_InputStateRep[prevFrameID].Mouse.ButtonState & 1 << buttonID) && !(m_InputStateRep[m_CurrentStateID].Mouse.ButtonState & 1 << buttonID)) ? 1 << buttonID : 0;

            // Update wheel (in this case, the axis data is also the delta
            std::unique_lock<std::mutex> lock(m_WheelMutex);
            m_InputStateRep[m_CurrentStateID].Mouse.AxisState[Mouse_Wheel] = wheelDelta;
            m_InputStateRep[m_CurrentStateID].Mouse.AxisDelta[Mouse_Wheel] = wheelDelta;
        }

        // And mouse axis information (always track mouse axis as some things need this information regardless of ImgGui hijacking it)
        {
            // Get the mouse coordinates in relation to the application window
            POINT mouseCoords;
            GetCursorPos(&mouseCoords);
            ScreenToClient(GetFramework()->GetImpl()->GetHWND(), &mouseCoords);

            m_InputStateRep[m_CurrentStateID].Mouse.AxisState[Mouse_XAxis] = mouseCoords.x;
            m_InputStateRep[m_CurrentStateID].Mouse.AxisState[Mouse_YAxis] = mouseCoords.y;

            int64_t mouseXDelta = mouseCoords.x - m_InputStateRep[prevFrameID].Mouse.AxisState[Mouse_XAxis];
            int64_t mouseYDelta = mouseCoords.y - m_InputStateRep[prevFrameID].Mouse.AxisState[Mouse_YAxis];
            m_InputStateRep[m_CurrentStateID].Mouse.AxisDelta[Mouse_XAxis] = mouseXDelta;
            m_InputStateRep[m_CurrentStateID].Mouse.AxisDelta[Mouse_YAxis] = mouseYDelta;
        }

        // NOTE: we may need to also check if UI hijacks controller input at some point in the future
        // Poll to see if any controllers are attached (note, always poll input '0')
        XINPUT_STATE    controllerState;
        if (XInputGetState(0, &controllerState) == ERROR_SUCCESS)
        {
            if (m_ChangeNumber == controllerState.dwPacketNumber)
            {
                int32_t lastStateID = m_CurrentStateID - 1;
                if (lastStateID < 0)
                    lastStateID += s_InputStateCacheSize;

                // Input hasn't changed state, copy existing state
                m_InputStateRep[m_CurrentStateID].GamePad = m_InputStateRep[lastStateID].GamePad;
            }
            else
            {
                // Reset input state
                m_InputStateRep[m_CurrentStateID].GamePad = { 0 };

                // Start by checking the button state
                if (controllerState.Gamepad.wButtons& XINPUT_GAMEPAD_DPAD_UP)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_DPadUp);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_DPadDown);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_DPadLeft);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_DPadRight);

                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_START)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_Start);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_Back);


                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_L3);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_R3);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_LB);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_RB);

                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_A)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_A);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_B)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_B);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_X)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_X);
                if (controllerState.Gamepad.wButtons & XINPUT_GAMEPAD_Y)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonState |= 1 << static_cast<uint32_t>(Pad_Y);

                // Set all button up state information
                for (uint32_t buttonID = 0; buttonID < static_cast<uint32_t>(Pad_ButtonCount); ++buttonID)
                    m_InputStateRep[m_CurrentStateID].GamePad.ButtonUpState |= ((m_InputStateRep[prevFrameID].GamePad.ButtonState & 1 << buttonID) && !(m_InputStateRep[m_CurrentStateID].GamePad.ButtonState & 1 << buttonID)) ? 1 << buttonID : 0;

                // Get the trigger states (take into account trigger threshold when reading values)
                m_InputStateRep[m_CurrentStateID].GamePad.AxisState[Pad_LTrigger] = GetTriggerValue(controllerState.Gamepad.bLeftTrigger);
                m_InputStateRep[m_CurrentStateID].GamePad.AxisState[Pad_RTrigger] = GetTriggerValue(controllerState.Gamepad.bRightTrigger);

                // Get stick values
                m_InputStateRep[m_CurrentStateID].GamePad.AxisState[Pad_LeftThumbX] = GetStickValue(controllerState.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                m_InputStateRep[m_CurrentStateID].GamePad.AxisState[Pad_LeftThumbY] = GetStickValue(controllerState.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                m_InputStateRep[m_CurrentStateID].GamePad.AxisState[Pad_RightThumbX] = GetStickValue(controllerState.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
                m_InputStateRep[m_CurrentStateID].GamePad.AxisState[Pad_RightThumbY] = GetStickValue(controllerState.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

                // Update the change number of the controller state (tracks if state has changed frame over frame)
                m_ChangeNumber = controllerState.dwPacketNumber;
            }
        }
    }

} // namespace cauldron

#endif // defined(_WINDOWS)
