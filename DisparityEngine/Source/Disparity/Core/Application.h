#pragma once

#include "Disparity/Rendering/Renderer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <windows.h>

namespace Disparity
{
    class Layer;

    struct ApplicationDesc
    {
        HINSTANCE Instance = nullptr;
        std::wstring Title = L"DISPARITY";
        uint32_t Width = 1280;
        uint32_t Height = 720;
    };

    class Application
    {
    public:
        explicit Application(ApplicationDesc desc);
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void SetLayer(std::unique_ptr<Layer> layer);
        int Run();
        void RequestExit(int exitCode = 0);

        [[nodiscard]] Renderer& GetRenderer();
        [[nodiscard]] HWND GetWindowHandle() const;
        [[nodiscard]] uint32_t GetWidth() const;
        [[nodiscard]] uint32_t GetHeight() const;

    private:
        static LRESULT CALLBACK StaticWindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
        LRESULT WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

        bool CreateMainWindow();
        void DestroyMainWindow();

        ApplicationDesc m_desc;
        HWND m_windowHandle = nullptr;
        Renderer m_renderer;
        std::unique_ptr<Layer> m_layer;
        bool m_running = false;
        bool m_rendererInitialized = false;
        int m_exitCode = 0;
        uint32_t m_width = 0;
        uint32_t m_height = 0;
    };
}
