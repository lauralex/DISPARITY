#include "Disparity/Core/Input.h"

#include "Disparity/Core/Log.h"

#include <array>
#include <windowsx.h>

namespace Disparity
{
    namespace
    {
        constexpr size_t KeyCount = 256;
        constexpr size_t MouseButtonCount = 5;

        HWND g_windowHandle = nullptr;
        std::array<bool, KeyCount> g_currentKeys = {};
        std::array<bool, KeyCount> g_previousKeys = {};
        std::array<bool, MouseButtonCount> g_currentMouseButtons = {};
        std::array<bool, MouseButtonCount> g_previousMouseButtons = {};
        DirectX::XMFLOAT2 g_mousePosition = {};
        DirectX::XMFLOAT2 g_mouseDelta = {};
        bool g_mouseCaptured = false;

        void SetCursorVisible(bool visible)
        {
            if (visible)
            {
                while (ShowCursor(TRUE) < 0)
                {
                }
            }
            else
            {
                while (ShowCursor(FALSE) >= 0)
                {
                }
            }
        }

        void UpdateMouseButton(size_t index, bool down)
        {
            if (index < g_currentMouseButtons.size())
            {
                g_currentMouseButtons[index] = down;
            }
        }
    }

    bool Input::Initialize(HWND windowHandle)
    {
        g_windowHandle = windowHandle;

        RAWINPUTDEVICE mouseDevice = {};
        mouseDevice.usUsagePage = 0x01;
        mouseDevice.usUsage = 0x02;
        mouseDevice.dwFlags = RIDEV_INPUTSINK;
        mouseDevice.hwndTarget = windowHandle;

        if (RegisterRawInputDevices(&mouseDevice, 1, sizeof(mouseDevice)) == FALSE)
        {
            Log(LogLevel::Warning, "Raw mouse input registration failed; camera input may be limited.");
            return false;
        }

        SetMouseCaptured(true);
        return true;
    }

    void Input::Shutdown()
    {
        SetMouseCaptured(false);
        g_windowHandle = nullptr;
        g_currentKeys.fill(false);
        g_previousKeys.fill(false);
        g_currentMouseButtons.fill(false);
        g_previousMouseButtons.fill(false);
    }

    void Input::BeginFrame()
    {
        g_previousKeys = g_currentKeys;
        g_previousMouseButtons = g_currentMouseButtons;
        g_mouseDelta = {};
    }

    bool Input::HandleMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        (void)windowHandle;

        switch (message)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam < KeyCount)
            {
                g_currentKeys[static_cast<size_t>(wParam)] = true;
            }
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wParam < KeyCount)
            {
                g_currentKeys[static_cast<size_t>(wParam)] = false;
            }
            break;
        case WM_LBUTTONDOWN:
            UpdateMouseButton(0, true);
            break;
        case WM_LBUTTONUP:
            UpdateMouseButton(0, false);
            break;
        case WM_RBUTTONDOWN:
            UpdateMouseButton(1, true);
            break;
        case WM_RBUTTONUP:
            UpdateMouseButton(1, false);
            break;
        case WM_MBUTTONDOWN:
            UpdateMouseButton(2, true);
            break;
        case WM_MBUTTONUP:
            UpdateMouseButton(2, false);
            break;
        case WM_MOUSEMOVE:
            g_mousePosition.x = static_cast<float>(GET_X_LPARAM(lParam));
            g_mousePosition.y = static_cast<float>(GET_Y_LPARAM(lParam));
            break;
        case WM_INPUT:
            if (g_mouseCaptured)
            {
                UINT size = 0;
                if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == 0 && size > 0)
                {
                    std::array<unsigned char, sizeof(RAWINPUT)> buffer = {};
                    if (size <= buffer.size())
                    {
                        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) == size)
                        {
                            const RAWINPUT* rawInput = reinterpret_cast<const RAWINPUT*>(buffer.data());
                            if (rawInput->header.dwType == RIM_TYPEMOUSE)
                            {
                                g_mouseDelta.x += static_cast<float>(rawInput->data.mouse.lLastX);
                                g_mouseDelta.y += static_cast<float>(rawInput->data.mouse.lLastY);
                            }
                        }
                    }
                }
            }
            break;
        case WM_SIZE:
            RefreshMouseClip();
            break;
        default:
            break;
        }

        return false;
    }

    bool Input::IsKeyDown(int virtualKey)
    {
        if (virtualKey < 0 || virtualKey >= static_cast<int>(KeyCount))
        {
            return false;
        }

        return g_currentKeys[static_cast<size_t>(virtualKey)];
    }

    bool Input::WasKeyPressed(int virtualKey)
    {
        if (virtualKey < 0 || virtualKey >= static_cast<int>(KeyCount))
        {
            return false;
        }

        const size_t index = static_cast<size_t>(virtualKey);
        return g_currentKeys[index] && !g_previousKeys[index];
    }

    bool Input::IsMouseButtonDown(int buttonIndex)
    {
        if (buttonIndex < 0 || buttonIndex >= static_cast<int>(MouseButtonCount))
        {
            return false;
        }

        return g_currentMouseButtons[static_cast<size_t>(buttonIndex)];
    }

    bool Input::WasMouseButtonPressed(int buttonIndex)
    {
        if (buttonIndex < 0 || buttonIndex >= static_cast<int>(MouseButtonCount))
        {
            return false;
        }

        const size_t index = static_cast<size_t>(buttonIndex);
        return g_currentMouseButtons[index] && !g_previousMouseButtons[index];
    }

    DirectX::XMFLOAT2 Input::GetMouseDelta()
    {
        return g_mouseDelta;
    }

    DirectX::XMFLOAT2 Input::GetMousePosition()
    {
        return g_mousePosition;
    }

    void Input::SetMouseCaptured(bool captured)
    {
        if (g_mouseCaptured == captured)
        {
            if (captured)
            {
                RefreshMouseClip();
            }
            return;
        }

        g_mouseCaptured = captured;

        if (captured && g_windowHandle != nullptr)
        {
            SetCapture(g_windowHandle);
            RefreshMouseClip();
            SetCursorVisible(false);
        }
        else
        {
            ClipCursor(nullptr);
            ReleaseCapture();
            SetCursorVisible(true);
        }
    }

    bool Input::IsMouseCaptured()
    {
        return g_mouseCaptured;
    }

    void Input::RefreshMouseClip()
    {
        if (!g_mouseCaptured || g_windowHandle == nullptr)
        {
            return;
        }

        RECT clientRect = {};
        if (GetClientRect(g_windowHandle, &clientRect) == FALSE)
        {
            return;
        }

        POINT upperLeft = { clientRect.left, clientRect.top };
        POINT lowerRight = { clientRect.right, clientRect.bottom };
        ClientToScreen(g_windowHandle, &upperLeft);
        ClientToScreen(g_windowHandle, &lowerRight);

        RECT screenRect = { upperLeft.x, upperLeft.y, lowerRight.x, lowerRight.y };
        ClipCursor(&screenRect);
    }
}
