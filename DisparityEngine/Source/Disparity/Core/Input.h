#pragma once

#include <DirectXMath.h>
#include <windows.h>

namespace Disparity
{
    class Input
    {
    public:
        static bool Initialize(HWND windowHandle);
        static void Shutdown();
        static void BeginFrame();
        static bool HandleMessage(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

        [[nodiscard]] static bool IsKeyDown(int virtualKey);
        [[nodiscard]] static bool WasKeyPressed(int virtualKey);
        [[nodiscard]] static bool IsMouseButtonDown(int buttonIndex);
        [[nodiscard]] static bool WasMouseButtonPressed(int buttonIndex);
        [[nodiscard]] static DirectX::XMFLOAT2 GetMouseDelta();
        [[nodiscard]] static DirectX::XMFLOAT2 GetMousePosition();

        static void SetMouseCaptured(bool captured);
        [[nodiscard]] static bool IsMouseCaptured();
        static void RefreshMouseClip();
    };
}
