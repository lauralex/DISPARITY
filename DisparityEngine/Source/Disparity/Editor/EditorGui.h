#pragma once

#include <d3d11.h>
#include <windows.h>

namespace Disparity
{
    class EditorGui
    {
    public:
        static bool Initialize(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* context);
        static void Shutdown();
        static void BeginFrame();
        static void Render();
        static bool HandleWin32Message(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
        [[nodiscard]] static bool IsInitialized();
    };
}
