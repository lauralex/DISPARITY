#pragma once

#include "Disparity/Diagnostics/Profiler.h"
#include "Disparity/Math/Transform.h"

#include <string>
#include <windows.h>

namespace Disparity
{
    class EditorOverlay
    {
    public:
        void SetVisible(bool visible);
        void Toggle();
        [[nodiscard]] bool IsVisible() const;

        void Update(
            HWND windowHandle,
            const std::wstring& baseTitle,
            const ProfileSnapshot& profiler,
            size_t objectCount,
            const std::string& selectedName,
            const Transform& selectedTransform,
            const std::string& statusMessage);

    private:
        bool m_visible = true;
    };
}
