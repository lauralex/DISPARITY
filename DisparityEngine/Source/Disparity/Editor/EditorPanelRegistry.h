#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Disparity
{
    struct EditorPanelDescriptor
    {
        std::string Name;
        std::string Category;
        std::string DockTarget;
        int Order = 0;
        bool DefaultVisible = true;
        bool Visible = true;
    };

    struct EditorPanelRegistryDiagnostics
    {
        uint32_t PanelCount = 0;
        uint32_t VisiblePanels = 0;
        uint32_t HiddenPanels = 0;
        uint32_t DockedPanels = 0;
        uint32_t ToggleOperations = 0;
    };

    class EditorPanelRegistry
    {
    public:
        [[nodiscard]] bool RegisterPanel(EditorPanelDescriptor descriptor);
        [[nodiscard]] bool SetVisible(const std::string& name, bool visible);
        [[nodiscard]] bool Toggle(const std::string& name);
        [[nodiscard]] bool IsVisible(const std::string& name) const;
        [[nodiscard]] const EditorPanelDescriptor* Find(const std::string& name) const;

        void ResetVisibility();
        void Clear();

        [[nodiscard]] EditorPanelRegistryDiagnostics GetDiagnostics() const;
        [[nodiscard]] const std::vector<EditorPanelDescriptor>& GetPanels() const;

    private:
        [[nodiscard]] EditorPanelDescriptor* FindMutable(const std::string& name);

        std::vector<EditorPanelDescriptor> m_panels;
        uint32_t m_toggleOperations = 0;
    };
}
