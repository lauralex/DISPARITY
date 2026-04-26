#include "Disparity/Core/Application.h"

#include "Disparity/Core/CrashHandler.h"
#include "Disparity/Core/Input.h"
#include "Disparity/Core/Layer.h"
#include "Disparity/Core/Log.h"
#include "Disparity/Core/Time.h"
#include "Disparity/Audio/AudioSystem.h"
#include "Disparity/Diagnostics/Profiler.h"
#include "Disparity/Editor/EditorGui.h"
#include "Disparity/Runtime/JobSystem.h"

#include <algorithm>

namespace Disparity
{
    namespace
    {
        constexpr wchar_t WindowClassName[] = L"DisparityWindowClass";
        constexpr double FixedTimeStepSeconds = 1.0 / 60.0;
    }

    Application::Application(ApplicationDesc desc)
        : m_desc(std::move(desc))
        , m_width(m_desc.Width)
        , m_height(m_desc.Height)
    {
    }

    Application::~Application()
    {
        DestroyMainWindow();
    }

    void Application::SetLayer(std::unique_ptr<Layer> layer)
    {
        m_layer = std::move(layer);
    }

    int Application::Run()
    {
        if (!CreateMainWindow())
        {
            return -1;
        }

        CrashHandler::Install();
        ShowWindow(m_windowHandle, SW_SHOW);
        UpdateWindow(m_windowHandle);

        AudioSystem::Initialize();
        JobSystem::Initialize();
        Input::Initialize(m_windowHandle);

        if (!m_renderer.Initialize(m_windowHandle, m_width, m_height))
        {
            Log(LogLevel::Error, "Renderer initialization failed.");
            return -2;
        }

        m_rendererInitialized = true;
        EditorGui::Initialize(m_windowHandle, m_renderer.GetDevice(), m_renderer.GetContext());

        if (m_layer && !m_layer->OnAttach(*this))
        {
            Log(LogLevel::Error, "Game layer failed to attach.");
            return -3;
        }

        Clock clock;
        double fixedAccumulator = 0.0;
        m_running = true;

        while (m_running)
        {
            Profiler::BeginFrame();
            Input::BeginFrame();

            MSG message = {};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    m_running = false;
                    break;
                }

                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            if (!m_running)
            {
                break;
            }

            TimeStep timeStep = clock.Tick();
            fixedAccumulator += timeStep.DeltaSecondsPrecise();

            while (fixedAccumulator >= FixedTimeStepSeconds)
            {
                if (m_layer)
                {
                    DISPARITY_PROFILE_SCOPE("FixedUpdate");
                    m_layer->OnFixedUpdate(TimeStep(FixedTimeStepSeconds, timeStep.ElapsedSeconds()));
                }

                fixedAccumulator -= FixedTimeStepSeconds;
            }

            if (Input::WasKeyPressed(VK_TAB))
            {
                Input::SetMouseCaptured(!Input::IsMouseCaptured());
            }

            if (Input::WasKeyPressed(VK_ESCAPE))
            {
                RequestExit();
            }

            if (m_layer)
            {
                DISPARITY_PROFILE_SCOPE("Update");
                m_layer->OnUpdate(timeStep);
            }

            const float clearColor[4] = { 0.03f, 0.04f, 0.06f, 1.0f };
            m_renderer.BeginFrame(clearColor);

            if (m_layer)
            {
                DISPARITY_PROFILE_SCOPE("Render");
                m_layer->OnRender(m_renderer);
            }

            EditorGui::BeginFrame();
            if (m_layer)
            {
                DISPARITY_PROFILE_SCOPE("EditorGui");
                m_layer->OnGui();
            }

            m_renderer.EndFrame();
            Profiler::EndFrame();
        }

        if (m_layer)
        {
            m_layer->OnDetach();
        }

        EditorGui::Shutdown();
        m_renderer.Shutdown();
        m_rendererInitialized = false;
        Input::Shutdown();
        JobSystem::Shutdown();
        AudioSystem::Shutdown();
        DestroyMainWindow();

        return 0;
    }

    void Application::RequestExit()
    {
        m_running = false;
        PostQuitMessage(0);
    }

    Renderer& Application::GetRenderer()
    {
        return m_renderer;
    }

    HWND Application::GetWindowHandle() const
    {
        return m_windowHandle;
    }

    uint32_t Application::GetWidth() const
    {
        return m_width;
    }

    uint32_t Application::GetHeight() const
    {
        return m_height;
    }

    LRESULT CALLBACK Application::StaticWindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        Application* application = nullptr;

        if (message == WM_NCCREATE)
        {
            const CREATESTRUCTW* createStruct = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            application = static_cast<Application*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application));
        }
        else
        {
            application = reinterpret_cast<Application*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
        }

        if (application)
        {
            return application->WindowProc(windowHandle, message, wParam, lParam);
        }

        return DefWindowProcW(windowHandle, message, wParam, lParam);
    }

    LRESULT Application::WindowProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        EditorGui::HandleWin32Message(windowHandle, message, wParam, lParam);
        Input::HandleMessage(windowHandle, message, wParam, lParam);

        switch (message)
        {
        case WM_SIZE:
        {
            const uint32_t width = static_cast<uint32_t>(LOWORD(lParam));
            const uint32_t height = static_cast<uint32_t>(HIWORD(lParam));

            if (wParam != SIZE_MINIMIZED && width > 0 && height > 0)
            {
                m_width = width;
                m_height = height;

                if (m_rendererInitialized)
                {
                    m_renderer.Resize(width, height);

                    if (m_layer)
                    {
                        m_layer->OnResize(width, height);
                    }
                }
            }
            break;
        }
        case WM_SETFOCUS:
            Input::SetMouseCaptured(true);
            break;
        case WM_KILLFOCUS:
            Input::SetMouseCaptured(false);
            break;
        case WM_CLOSE:
            RequestExit();
            DestroyWindow(windowHandle);
            return 0;
        case WM_DESTROY:
            m_windowHandle = nullptr;
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(windowHandle, message, wParam, lParam);
    }

    bool Application::CreateMainWindow()
    {
        WNDCLASSEXW windowClass = {};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = &Application::StaticWindowProc;
        windowClass.hInstance = m_desc.Instance;
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
        windowClass.lpszClassName = WindowClassName;

        RegisterClassExW(&windowClass);

        RECT windowRect = { 0, 0, static_cast<LONG>(m_desc.Width), static_cast<LONG>(m_desc.Height) };
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;
        const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

        m_windowHandle = CreateWindowExW(
            0,
            WindowClassName,
            m_desc.Title.c_str(),
            WS_OVERLAPPEDWINDOW,
            std::max(0, x),
            std::max(0, y),
            width,
            height,
            nullptr,
            nullptr,
            m_desc.Instance,
            this);

        if (!m_windowHandle)
        {
            Log(LogLevel::Error, "Win32 window creation failed.");
            return false;
        }

        return true;
    }

    void Application::DestroyMainWindow()
    {
        if (m_windowHandle)
        {
            DestroyWindow(m_windowHandle);
            m_windowHandle = nullptr;
        }

        UnregisterClassW(WindowClassName, m_desc.Instance);
    }
}
