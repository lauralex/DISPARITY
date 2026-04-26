#include "Disparity/Editor/EditorGui.h"

#include "Disparity/Core/Log.h"

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Disparity
{
    namespace
    {
        bool g_initialized = false;
    }

    bool EditorGui::Initialize(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (g_initialized)
        {
            return true;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.ConfigWindowsMoveFromTitleBarOnly = true;

        ImGui::StyleColorsDark();
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 4.0f;
        style.FrameRounding = 3.0f;
        style.WindowBorderSize = 1.0f;
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        if (!ImGui_ImplWin32_Init(windowHandle))
        {
            Log(LogLevel::Error, "Dear ImGui Win32 backend initialization failed.");
            return false;
        }

        if (!ImGui_ImplDX11_Init(device, context))
        {
            ImGui_ImplWin32_Shutdown();
            Log(LogLevel::Error, "Dear ImGui DirectX 11 backend initialization failed.");
            return false;
        }

        g_initialized = true;
        return true;
    }

    void EditorGui::Shutdown()
    {
        if (!g_initialized)
        {
            return;
        }

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_initialized = false;
    }

    void EditorGui::BeginFrame()
    {
        if (!g_initialized)
        {
            return;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void EditorGui::Render()
    {
        if (!g_initialized)
        {
            return;
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO();
        if ((io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    bool EditorGui::HandleWin32Message(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (!g_initialized)
        {
            return false;
        }

        return ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam) != 0;
    }

    bool EditorGui::IsInitialized()
    {
        return g_initialized;
    }
}
