#include "Disparity/Editor/EditorOverlay.h"

#include <iomanip>
#include <sstream>

namespace Disparity
{
    void EditorOverlay::SetVisible(bool visible)
    {
        m_visible = visible;
    }

    void EditorOverlay::Toggle()
    {
        m_visible = !m_visible;
    }

    bool EditorOverlay::IsVisible() const
    {
        return m_visible;
    }

    void EditorOverlay::Update(
        HWND windowHandle,
        const std::wstring& baseTitle,
        const ProfileSnapshot& profiler,
        size_t objectCount,
        const std::string& selectedName,
        const Transform& selectedTransform,
        const std::string& statusMessage)
    {
        if (!windowHandle)
        {
            return;
        }

        if (!m_visible)
        {
            SetWindowTextW(windowHandle, baseTitle.c_str());
            return;
        }

        std::wstringstream title;
        title << baseTitle
            << L" | Editor: objects " << objectCount
            << L" | FPS " << std::fixed << std::setprecision(1) << profiler.FramesPerSecond
            << L" | Frame " << std::setprecision(2) << profiler.FrameMilliseconds << L" ms";

        if (!selectedName.empty())
        {
            title << L" | Selected " << std::wstring(selectedName.begin(), selectedName.end())
                << L" @ "
                << std::setprecision(1)
                << selectedTransform.Position.x << L","
                << selectedTransform.Position.y << L","
                << selectedTransform.Position.z;
        }

        if (!statusMessage.empty())
        {
            title << L" | " << std::wstring(statusMessage.begin(), statusMessage.end());
        }

        SetWindowTextW(windowHandle, title.str().c_str());
    }
}
