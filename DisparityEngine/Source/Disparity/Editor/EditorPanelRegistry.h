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

    struct EditorWorkspaceDescriptor
    {
        std::string Name;
        std::string DockLayout;
        std::vector<std::string> VisiblePanels;
        std::string PreferredFocusPanel;
        bool GamepadNavigable = false;
        std::string LayoutFile;
        bool TeamDefault = false;
        std::vector<std::string> CommandBindings;
        std::vector<std::string> ControllerNavigationHints;
        uint32_t ToolbarSlots = 0;
    };

    struct EditorPanelRegistryDiagnostics
    {
        uint32_t PanelCount = 0;
        uint32_t VisiblePanels = 0;
        uint32_t HiddenPanels = 0;
        uint32_t DockedPanels = 0;
        uint32_t ToggleOperations = 0;
        uint32_t WorkspaceCount = 0;
        uint32_t AppliedWorkspaces = 0;
        uint32_t WorkspacePanelBindings = 0;
        uint32_t MissingPanelReferences = 0;
        uint32_t SearchOperations = 0;
        uint32_t NavigationOperations = 0;
        uint32_t SavedDockLayouts = 0;
        uint32_t TeamDefaultWorkspaces = 0;
        uint32_t WorkspaceCommandBindings = 0;
        uint32_t ControllerNavigationHints = 0;
        uint32_t ToolbarCustomizationSlots = 0;
    };

    class EditorPanelRegistry
    {
    public:
        [[nodiscard]] bool RegisterPanel(EditorPanelDescriptor descriptor);
        [[nodiscard]] bool SetVisible(const std::string& name, bool visible);
        [[nodiscard]] bool Toggle(const std::string& name);
        [[nodiscard]] bool IsVisible(const std::string& name) const;
        [[nodiscard]] const EditorPanelDescriptor* Find(const std::string& name) const;
        [[nodiscard]] std::vector<EditorPanelDescriptor> SearchPanels(const std::string& query);
        [[nodiscard]] const EditorPanelDescriptor* FocusNextVisiblePanel(const std::string& currentPanel, int direction);

        [[nodiscard]] bool RegisterWorkspace(EditorWorkspaceDescriptor descriptor);
        [[nodiscard]] bool ApplyWorkspace(const std::string& name);
        [[nodiscard]] const EditorWorkspaceDescriptor* FindWorkspace(const std::string& name) const;

        void ResetVisibility();
        void Clear();

        [[nodiscard]] EditorPanelRegistryDiagnostics GetDiagnostics() const;
        [[nodiscard]] const std::vector<EditorPanelDescriptor>& GetPanels() const;
        [[nodiscard]] const std::vector<EditorWorkspaceDescriptor>& GetWorkspaces() const;

    private:
        [[nodiscard]] EditorPanelDescriptor* FindMutable(const std::string& name);

        std::vector<EditorPanelDescriptor> m_panels;
        std::vector<EditorWorkspaceDescriptor> m_workspaces;
        uint32_t m_toggleOperations = 0;
        uint32_t m_appliedWorkspaces = 0;
        uint32_t m_missingPanelReferences = 0;
        uint32_t m_searchOperations = 0;
        uint32_t m_navigationOperations = 0;
    };
}
