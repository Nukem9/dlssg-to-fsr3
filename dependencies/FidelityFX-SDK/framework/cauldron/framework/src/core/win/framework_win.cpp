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

#if defined(_WINDOWS)

#include "core/win/framework_win.h"
#include "core/win/uibackend_win.h"
#include "core/win/inputmanager_win.h"
#include "core/uimanager.h"

#include "render/device.h"
#include "render/swapchain.h"

#include <windowsx.h>

using namespace std::experimental;

namespace cauldron
{
    static const wchar_t* const s_WINDOW_CLASS_NAME = L"CauldronSample";

    FrameworkInternal::FrameworkInternal(Framework* pFramework, const FrameworkInitParams* pInitParams) :
        FrameworkImpl(pFramework),
        m_InstanceHandle(((FrameworkInitParamsInternal*)pInitParams->AdditionalParams)->InstanceHandle),
        m_CmdShow(((FrameworkInitParamsInternal*)pInitParams->AdditionalParams)->CmdShow)
    {
        // Do Windows specific initialization
        WNDCLASSEXW windowClass;

        ZeroMemory(&windowClass, sizeof(WNDCLASSEXW));
        windowClass.cbSize = sizeof(WNDCLASSEXW);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = FrameworkInternal::WindowProc;
        windowClass.hInstance = m_InstanceHandle;
        windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpszClassName = s_WINDOW_CLASS_NAME;
        windowClass.hIcon = LoadIcon(m_InstanceHandle, MAKEINTRESOURCE(101)); // '#define ICON_IDI 101' is assumed to be included with resource.h
        RegisterClassExW(&windowClass);
    }

    void FrameworkInternal::Init()
    {
        // Store the exe name for identifier purposes (used in benchmark gathering)
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        filesystem::path filePath = buf;
        m_pFramework->m_Config.AppName = filePath.filename();

        // Setup the window for the sample
        m_WindowRect = { 0, 0, (LONG)m_pFramework->m_Config.Width, (LONG)m_pFramework->m_Config.Height };
        m_WindowStyle = WS_OVERLAPPEDWINDOW;
        AdjustWindowRect(&m_WindowRect, m_WindowStyle, FALSE);

        // This makes sure that in a multi-monitor setup with different resolutions, get monitor info returns correct dimensions
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        // Create the window and store a handle to it
        m_WindowHandle = CreateWindowExW(0,
            s_WINDOW_CLASS_NAME,         // name of the window class
            m_pFramework->m_Name.c_str(),
            m_WindowStyle,
            0,
            0,
            m_WindowRect.right - m_WindowRect.left,
            m_WindowRect.bottom - m_WindowRect.top,
            0,                           // we have no parent window, 0
            0,                           // we aren't using menus, 0
            m_InstanceHandle,            // application handle
            0);                          // used with multiple windows, 0

        // Save the old window rect so we can restore it when moving across monitors when hdr is on.
        GetWindowRect(m_WindowHandle, &m_WindowRect);

        // Get the monitor
        m_Monitor = MonitorFromWindow(m_WindowHandle, MONITOR_DEFAULTTONEAREST);
    }

    LRESULT CALLBACK FrameworkInternal::WindowProc(HWND wndHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        // Give first crack at our UI backend as it needs to intercept messages
        MessagePacket msgPacket = { wndHandle, message, wParam, lParam };
        if (GetUIManager() && GetUIManager()->UIBackendMessageHandler(&msgPacket))
            return true;

        Framework* pFramework = GetFramework();

        // Sort through and find what code to run for the message given
        switch (message)
        {

        // Quit the app
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            pFramework->m_pImpl->m_Quitting = true;
            return 0;
        }

        // When close button is clicked on window
        case WM_CLOSE:
        {
            DestroyWindow(wndHandle);
            return 0;
        }

        // Prevent resizing below 640x480 as it can cause issues with resource recreation
        case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
            lpmmi->ptMinTrackSize.x = 640;
            lpmmi->ptMinTrackSize.y = 480;
            return 0;
        }

        // Handle window resizing
        case WM_SIZE:
        {
            pFramework->m_pImpl->m_sendResizeEvent = true;

            // Did we minimize/restore?
            if (wParam == SIZE_MINIMIZED)
            {
                pFramework->m_pImpl->m_Minimized = true;
            }
            else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED)
            {
                pFramework->m_pImpl->m_Minimized = false;
            }
            break;
        }

        case WM_MOVE:
        {
            pFramework->m_pImpl->OnWindowMove();
            break;
        }

        // Turn off MessageBeep sound on Alt+Enter
        case WM_MENUCHAR: 
        {
            return MNC_CLOSE << 16;
        }

        // Handle key presses from keyboard
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            if (message == WM_KEYDOWN && wParam == VK_ESCAPE)
            {
                DestroyWindow(wndHandle);
                return 0;
            }
            break;
        }

        case WM_SYSCOMMAND:
        {
            // Swallow the menu key so it doesn't pause the application
            if (wParam == SC_KEYMENU)
            {
                return 0;
            }
            break;
        }
            
        // Handle system key presses from keyboard
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
            const bool altKeyDown = (lParam & (1 << 29));
            if (message == WM_SYSKEYDOWN && (wParam == VK_RETURN) && altKeyDown) // Alt+enter only toggles in/out windowed and border-less full screen
                pFramework->m_pImpl->ToggleFullscreen();
            break;
        }

        // Mouse wheel can't be polled, so push any state changes to input manager
        case WM_MOUSEWHEEL:
        {
            // Mouse wheel will be set to -1 -> 1 according to WHEEL_DELTA. i.e. full +WHEEL_DELTA == 1.f mouse wheel axis value
            int64_t wheelValue = GET_WHEEL_DELTA_WPARAM(wParam) / (WHEEL_DELTA);
            static_cast<InputManagerInternal*>(GetInputManager())->PushWheelChange(wheelValue);
            break;
        }

        // Double click caption bar
        case WM_NCLBUTTONDBLCLK:
        {
            if (wParam == HTCAPTION)
            {
                // Tell the InputManager to ignore input for this frame
                // as we don't want to track left clicks associated with NCL DBLCLK
                GetInputManager()->IgnoreInputForFrame();
            }
            break;
        }

        // Lost focus
        case WM_KILLFOCUS:
        {
            pFramework->m_pImpl->OnFocusLost();
            break;
        }

        // Gained focus
        case WM_SETFOCUS:
        {
            pFramework->m_pImpl->OnFocusGained();
            break;
        }

        }

        // Handle any messages the switch statement didn't
        return DefWindowProcW(wndHandle, message, wParam, lParam);
    }

    void FrameworkInternal::ToggleFullscreen()
    {
        m_PresentationMode = (m_PresentationMode == PRESENTATIONMODE_WINDOWED) ? PRESENTATIONMODE_BORDERLESS_FULLSCREEN : PRESENTATIONMODE_WINDOWED;
        CauldronAssert(ASSERT_CRITICAL, m_pFramework->m_pDevice != nullptr, L"Can't toggle presentation mode without a device");

        // Flush the GPU to make sure we don't change anything still active
        m_pFramework->m_pDevice->FlushAllCommandQueues();

        // Set the full screen mode
        if (m_PresentationMode == PRESENTATIONMODE_BORDERLESS_FULLSCREEN)
        {
            // Save the old window rect so we can restore it when exiting fullscreen mode.
            GetWindowRect(m_WindowHandle, &m_WindowRect);

            // Make the window border-less so that the client area can fill the screen.
            SetWindowLong(m_WindowHandle, GWL_STYLE, m_WindowStyle & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

            MONITORINFO monitorInfo;
            monitorInfo.cbSize = sizeof(monitorInfo);
            GetMonitorInfo(MonitorFromWindow(m_WindowHandle, MONITOR_DEFAULTTONEAREST), &monitorInfo);

            SetWindowPos(m_WindowHandle, HWND_NOTOPMOST, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_WindowHandle, SW_MAXIMIZE);
        }
        else
        {
            // Restore the window's attributes and size.
            SetWindowLong(m_WindowHandle, GWL_STYLE, m_WindowStyle);

            SetWindowPos(m_WindowHandle, HWND_NOTOPMOST, m_WindowRect.left, m_WindowRect.top,
                m_WindowRect.right - m_WindowRect.left, m_WindowRect.bottom - m_WindowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_WindowHandle, SW_NORMAL);
        }

        // Need to also update all of the resolution-size based resources
        RECT clientRect = {};
        GetClientRect(m_WindowHandle, &clientRect);
        OnResize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
    }

    void FrameworkInternal::OnWindowMove()
    {
        HMONITOR currentMonitor = MonitorFromWindow(m_WindowHandle, MONITOR_DEFAULTTONEAREST);
        if (m_Monitor != currentMonitor && GetFramework()->GetSwapChain()->GetSwapChainDisplayMode() != DisplayMode::DISPLAYMODE_LDR)
        {
            CauldronWarning(L"Cannot move window across monitors as we are rendering HDR output according to current display's HDR mode and colour volume.");

            SetWindowPos(m_WindowHandle,
                         HWND_NOTOPMOST,
                         m_WindowRect.left,
                         m_WindowRect.top,
                         m_WindowRect.right - m_WindowRect.left,
                         m_WindowRect.bottom - m_WindowRect.top,
                         SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_WindowHandle, SW_NORMAL);
        }
    }

    void FrameworkInternal::OnResize(uint32_t width, uint32_t height)
    {
        // Might not be created yet
        if (!m_pFramework->m_pSwapChain)
            return;

        // Detect minimize
        if (m_pFramework->m_pImpl->m_Minimized)
            return;

        // Store the new width/height (as both render and display resolution)
        // Pick the right method for doing the update
        if (m_pFramework->m_UpscalerEnabled && m_pFramework->m_ResolutionUpdaterFn)
            m_pFramework->m_ResolutionInfo = m_pFramework->m_ResolutionUpdaterFn(width, height);
        else
            m_pFramework->m_ResolutionInfo = {width, height, width, height, width, height};

        // Flush everything before resizing resources (can't have anything in the pipes)
        CauldronAssert(ASSERT_ERROR,
                       std::this_thread::get_id() == GetFramework()->MainThreadID(),
                       L"Cauldron: OnResize: Expecting OnResize to be called on MainThread. Not thread safe!");
        GetDevice()->FlushAllCommandQueues();

        // Resize swapchain (only takes display resolution)
        GetSwapChain()->OnResize(m_pFramework->m_ResolutionInfo.DisplayWidth, m_pFramework->m_ResolutionInfo.DisplayHeight);

        // Trigger a resize event for the framework
        m_pFramework->ResizeEvent();
    }

    void FrameworkInternal::OnFocusLost()
    {
        m_pFramework->FocusLostEvent();
    }

    void FrameworkInternal::OnFocusGained()
    {
        m_pFramework->FocusGainedEvent();
    }

    int32_t FrameworkInternal::Run()
    {
        // Show the window if needed
        ShowWindow(m_WindowHandle, m_CmdShow);

        // Init presentation mode
        m_PresentationMode = m_pFramework->m_Config.Fullscreen ? PRESENTATIONMODE_BORDERLESS_FULLSCREEN : PRESENTATIONMODE_WINDOWED;
        if (m_PresentationMode == PRESENTATIONMODE_BORDERLESS_FULLSCREEN)
        {
            // Save the old window rect so we can restore it when exiting full screen mode.
            GetWindowRect(m_WindowHandle, &m_WindowRect);

            // Make the window border-less so that the client area can fill the screen.
            SetWindowLong(m_WindowHandle, GWL_STYLE, m_WindowStyle & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

            MONITORINFO monitorInfo;
            monitorInfo.cbSize = sizeof(monitorInfo);
            GetMonitorInfo(MonitorFromWindow(m_WindowHandle, MONITOR_DEFAULTTONEAREST), &monitorInfo);

            SetWindowPos(m_WindowHandle,
                         HWND_NOTOPMOST,
                         monitorInfo.rcMonitor.left,
                         monitorInfo.rcMonitor.top,
                         monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                         monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                         SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(m_WindowHandle, SW_MAXIMIZE);
        }

        // Everything is now initialized and we are entering the "running" state
        m_pFramework->m_Running.store(true);

        // Main loop
        MSG msg = { 0 };
        while (msg.message != WM_QUIT)
        {
            GetDevice()->UpdateAntiLag2();

            // Check to see if any messages are waiting in the queue
            while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg); // Translate keystroke messages into the right format
                if (msg.message == WM_QUIT) // DispatchMessage resets the msg so the outer loop never sees the QUIT event
                {
                    break;
                }
                DispatchMessage(&msg);  // Send the message to the WindowProc function
            }

            // Only update if we aren't minimized and we aren't quitting
            if (!m_pFramework->m_pImpl->m_Minimized && !m_pFramework->m_pImpl->m_Quitting)
            {
                if (m_sendResizeEvent)
                {
                    RECT clientRect = {};
                    GetClientRect(m_WindowHandle, &clientRect);
                    uint32_t width  = clientRect.right - clientRect.left;
                    uint32_t height = clientRect.bottom - clientRect.top;
                    const ResolutionInfo& resInfo = m_pFramework->GetResolutionInfo();

                    if (width != resInfo.DisplayWidth || height != resInfo.DisplayHeight)
                        OnResize(width, height);

                    m_sendResizeEvent = false;
                }
                m_pFramework->MainLoop();
            }
        }

        return static_cast<char>(msg.wParam);
    }

} // namespace cauldron

#endif // #if defined(_WINDOWS)
