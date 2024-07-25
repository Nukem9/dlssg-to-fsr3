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
#include "misc/threadsafe_queue.h"

#include <cstdint>

namespace cauldron
{
    /**
     * @enum MouseButtonMappings
     *
     * Remaps for mouse button controls
     *
     * @ingroup CauldronCore
     */
    enum MouseButtonMappings
    {
        Mouse_LButton = 0,  ///< Mouse left button mapping
        Mouse_RButton,      ///< Mouse right button mapping
        Mouse_MButton,      ///< Mouse middle button mapping
        
        Mouse_ButtonCount   ///< Mouse button count
    };

    /**
     * @enum MouseAxisMappings
     *
     * Remaps for mouse axis controls
     *
     * @ingroup CauldronCore
     */
    enum MouseAxisMappings
    {
        Mouse_Wheel = 0,    ///< Mouse wheel axis mapping
        Mouse_XAxis,        ///< Mouse x (horizontal) axis mapping
        Mouse_YAxis,        ///< Mouse y (vertical) axis mapping

        Mouse_AxisCount     ///< Mouse axis count
    };

     /**
     * @enum KeyboardInputMappings
     *
     * Remaps for keyboard keys
     * Note: Being very selective about what keys we can map in order to try and 
     * fit everything into a 64-bit state representation
     *
     * @ingroup CauldronCore
     */
    enum KeyboardInputMappings
    {
        // Standard
        Key_0 = 0,
        Key_1,
        Key_2,
        Key_3,
        Key_4,
        Key_5,
        Key_6,
        Key_7,
        Key_8,
        Key_9,
        Key_A,
        Key_B,
        Key_C,
        Key_D,
        Key_E,
        Key_F,
        Key_G,
        Key_H,
        Key_I,
        Key_J,
        Key_K,
        Key_L,
        Key_M,
        Key_N,
        Key_O,
        Key_P,
        Key_Q,
        Key_R,
        Key_S,
        Key_T,
        Key_U,
        Key_V,
        Key_W,
        Key_X,
        Key_Y,
        Key_Z,

        // Special keys
        Key_Backspace,
        Key_Tab,
        Key_Enter,
        Key_Shift,
        Key_Ctrl,
        Key_Alt,
        Key_Pause,
        Key_CapsLock,
        Key_Space,
        Key_PrintScreen,

        // Directional keys
        Key_Left,
        Key_Up,
        Key_Right,
        Key_Down,
        
        // Function keys
        Key_F1,
        Key_F2,
        Key_F3,
        Key_F4,
        Key_F5,
        Key_F6,
        Key_F7,
        Key_F8,
        Key_F9,
        Key_F10,
        Key_F11,
        Key_F12,

        Key_Count
    };

    /**
     * @enum GamePadButtonMappings
     *
     * Remaps for gamepad digital controls
     *
     * @ingroup CauldronCore
     */
    enum GamePadButtonMappings
    {
        Pad_DPadUp = 0,     ///< Gamepad directional cross up
        Pad_DPadDown,       ///< Gamepad directional cross down
        Pad_DPadLeft,       ///< Gamepad directional cross left
        Pad_DPadRight,      ///< Gamepad directional cross right
        Pad_Start,          ///< Gamepad start button (or equivalent)
        Pad_Back,           ///< Gamepad back/select/capture button (or equivalent)
        Pad_L3,             ///< Gamepad Left Thumb Stick down
        Pad_R3,             ///< Gamepad Right Thumb Stick down
        Pad_LB,             ///< Gamepad Left Bumper/Button
        Pad_RB,             ///< Gamepad Right Bumper/Button
        Pad_A,              ///< Gamepad A button: XBox -> A, PS -> Cross
        Pad_B,              ///< Gamepad B button: XBox -> B, PS -> Circle
        Pad_X,              ///< Gamepad X button: XBox -> X, PS -> Square
        Pad_Y,              ///< Gamepad Y button: XBox -> Y, PS -> Triangle
        
        Pad_ButtonCount     ///< Gamepad button count 
    };

    /**
     * @enum GamePadAxisMappings
     *
     * Remaps for gamepad analogue controls
     *
     * @ingroup CauldronCore
     */
    enum GamePadAxisMappings
    {
        Pad_LTrigger = 0,   ///< Gamepad left trigger
        Pad_RTrigger,       ///< Gamepad right trigger
        Pad_LeftThumbX,     ///< Gamepad left thumbstick X-axis
        Pad_LeftThumbY,     ///< Gamepad left thumbstick Y-axis
        Pad_RightThumbX,    ///< Gamepad right thumbstick X-axis
        Pad_RightThumbY,    ///< Gamepad right thumbstick Y-axis
        
        Pad_AxisCount       ///< Gamepad axis count
    };

    /**
     * @struct MouseState
     *
     * Tracks mouse button and axis states per frame
     *
     * @ingroup CauldronCore
     */
    struct MouseState
    {
        static_assert(Mouse_ButtonCount <= 8, L"ButtonState is represented as an 8-bit mapping. Mouse_ButtonCount is now greater than 8, please grow bit map representation");
        uint8_t  ButtonState = 0;                       ///< Bitwise mapping of button down state for all mouse buttons
        uint8_t  ButtonUpState = 0;                     ///< Bitwise mapping of button up state for all mouse buttons
        uint64_t AxisState[Mouse_AxisCount] = { 0 };    ///< Mouse axis value
        int64_t  AxisDelta[Mouse_AxisCount] = { 0 };    ///< Mouse axis delta
    };

    /**
     * @struct GamePadState
     *
     * Tracks gamepad button and axis states per frame
     *
     * @ingroup CauldronCore
     */
    struct GamePadState
    {
        static_assert(Pad_ButtonCount <= 16, L"ButtonState is represented as a 16-bit mapping. Pad_ButtonCount is now greater than 16, please grow bit map representation");
        uint16_t    ButtonState = 0;                        ///< Bitwise mapping of button down state for all gamepad buttons
        uint16_t    ButtonUpState = 0;                      ///< Bitwise mapping of button up state for all gamepad buttons
        float       AxisState[Pad_AxisCount] = { 0.f };     ///< Gamepad axis value
    };

    /**
     * @struct InputState
     *
     * Represents the entire input state for the duration of a processed frame
     *
     * @ingroup CauldronCore
     */
    struct InputState
    {
        MouseState      Mouse;                  ///< Represents the state of mouse input
        GamePadState    GamePad;                ///< Represents the state of gamepad input

        static_assert(Key_Count <= 64, L"KeyboardState is represented as a 64-bit mapping. Key_Count is now greater than 64, please grow bit map representation");
        uint64_t        KeyboardState = 0;      ///< Bitwise mapping of keyboard key down states for all monitored keys
        uint64_t        KeyboardUpState = 0;    ///< Bitwise mapping of keyboard key up states for all monitored keys

        /**
         * @brief   Returns the key down state of the requested input mapping.
         */
        bool GetKeyState(const KeyboardInputMappings inputID) const
        {
            return (KeyboardState & 1ull << static_cast<uint64_t>(inputID));
        }

        /**
         * @brief   Returns the key up state of the requested input mapping.
         */
        bool GetKeyUpState(const KeyboardInputMappings inputID) const
        {
            return (KeyboardUpState & 1ull << static_cast<uint64_t>(inputID));
        }

        /**
         * @brief   Returns the button down state of the requested input mapping.
         */
        bool GetMouseButtonState(const MouseButtonMappings inputID) const
        {
            uint32_t button = 1 << static_cast<uint32_t>(inputID);
            return (Mouse.ButtonState & button);
        }

        /**
         * @brief   Returns the button up state of the requested input mapping.
         */
        bool GetMouseButtonUpState(const MouseButtonMappings inputID) const
        {
            uint32_t button = 1 << static_cast<uint32_t>(inputID);
            return (Mouse.ButtonUpState & button);
        }

        /**
         * @brief   Returns the axis value of the requested input mapping.
         */
        int64_t GetMouseAxisDelta(const MouseAxisMappings inputID) const
        {
            return Mouse.AxisDelta[inputID];
        }

        /**
         * @brief   Returns axis value of the requested input mapping.
         */
        float GetGamePadAxisState(const GamePadAxisMappings inputID) const
        {
            return GamePad.AxisState[inputID];
        }

        /**
         * @brief   Returns the button down state of the requested input mapping.
         */
        bool GetGamePadButtonState(const GamePadButtonMappings inputID) const
        {
            uint32_t button = 1 << static_cast<uint32_t>(inputID);
            return (GamePad.ButtonState & button);
        }

        /**
         * @brief   Returns the button up state of the requested input mapping.
         */
        bool GetGamePadButtonUpState(const GamePadButtonMappings inputID) const
        {
            uint32_t button = 1 << static_cast<uint32_t>(inputID);
            return (GamePad.ButtonUpState & button);
        }
    };
    
    /**
     * @enum InputSource
     *
     * Input source mappings
     *
     * @ingroup CauldronCore
     */
    enum InputSource
    {
        InputSrc_Keyboard = 0,
        InputSrc_Mouse,
        InputSrc_GamePad,

        InputSrc_Count
    };

    /**
     * @enum InputType
     *
     * Input type mappings (axis or button)
     *
     * @ingroup CauldronCore
     */
    enum InputType
    {
        InputType_Button = 0,
        InputType_Axis,

        InputType_Count
    };

    /**
     * @class InputManager
     *
     * The InputManager instance is responsible for registering the current input
     * state across all inputs for a frame, and responding to queries about individual
     * input states from the application layer.
     *
     * @ingroup CauldronCore
     */
    class InputManager
    {
    public:

        /**
         * @brief   InputManager destruction. 
         */
        virtual ~InputManager();

        /**
         * @brief   InputManager Update function. Called once a frame in order to poll all 
         *          connected devices to setup the <c><i>InputState</i></c> for the frame.
         */
        void Update();
        
        /**
         * @brief   Query input state for the current frame (or cached frames)
         */
        const InputState& GetInputState(int frameOffset = 0) const;

        /**
         * @brief   Creates a platform-specific InputManager. Function must be implemented for each platform supported.
         */
        static InputManager* CreateInputManager();

        /**
         * @brief   Tells the InputManager to ignore input for the current frame. Some platform updates require us to skip input polling for a frame.
         */
        void IgnoreInputForFrame() { m_IgnoreFrameInputs = true; }

    private:
        // No Copy, No Move
        NO_COPY(InputManager);
        NO_MOVE(InputManager);

    protected:
        InputManager();

        // This function, called from Update(), needs to be implemented for each platform
        virtual void PollInputStates() = 0;

    protected:
        
        static const uint8_t s_InputStateCacheSize = 3;         // Number of frames we will cache inputs for
        InputState  m_InputStateRep[s_InputStateCacheSize] = {};
        uint32_t    m_CurrentStateID = 0;
        bool        m_IgnoreFrameInputs = false;
    };

} // namespace cauldron
