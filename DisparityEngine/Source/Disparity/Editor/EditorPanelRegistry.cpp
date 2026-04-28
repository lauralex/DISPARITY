#include "Disparity/Editor/EditorPanelRegistry.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <utility>

namespace Disparity
{
    namespace
    {
        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            return value;
        }

        bool ContainsCaseInsensitive(const std::string& haystack, const std::string& needle)
        {
            if (needle.empty())
            {
                return true;
            }

            return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
        }
    }

    bool EditorPanelRegistry::RegisterPanel(EditorPanelDescriptor descriptor)
    {
        if (descriptor.Name.empty() || Find(descriptor.Name) != nullptr)
        {
            return false;
        }

        descriptor.Visible = descriptor.DefaultVisible;
        m_panels.push_back(std::move(descriptor));
        std::sort(m_panels.begin(), m_panels.end(), [](const EditorPanelDescriptor& left, const EditorPanelDescriptor& right) {
            return left.Order == right.Order ? left.Name < right.Name : left.Order < right.Order;
        });
        return true;
    }

    bool EditorPanelRegistry::SetVisible(const std::string& name, bool visible)
    {
        EditorPanelDescriptor* panel = FindMutable(name);
        if (panel == nullptr)
        {
            return false;
        }

        if (panel->Visible != visible)
        {
            ++m_toggleOperations;
        }
        panel->Visible = visible;
        return true;
    }

    bool EditorPanelRegistry::Toggle(const std::string& name)
    {
        EditorPanelDescriptor* panel = FindMutable(name);
        if (panel == nullptr)
        {
            return false;
        }

        panel->Visible = !panel->Visible;
        ++m_toggleOperations;
        return true;
    }

    bool EditorPanelRegistry::IsVisible(const std::string& name) const
    {
        const EditorPanelDescriptor* panel = Find(name);
        return panel != nullptr && panel->Visible;
    }

    const EditorPanelDescriptor* EditorPanelRegistry::Find(const std::string& name) const
    {
        const auto found = std::find_if(m_panels.begin(), m_panels.end(), [&name](const EditorPanelDescriptor& panel) {
            return panel.Name == name;
        });
        return found == m_panels.end() ? nullptr : &(*found);
    }

    std::vector<EditorPanelDescriptor> EditorPanelRegistry::SearchPanels(const std::string& query)
    {
        ++m_searchOperations;

        std::vector<EditorPanelDescriptor> panels;
        for (const EditorPanelDescriptor& panel : m_panels)
        {
            if (ContainsCaseInsensitive(panel.Name, query) ||
                ContainsCaseInsensitive(panel.Category, query) ||
                ContainsCaseInsensitive(panel.DockTarget, query))
            {
                panels.push_back(panel);
            }
        }
        return panels;
    }

    const EditorPanelDescriptor* EditorPanelRegistry::FocusNextVisiblePanel(const std::string& currentPanel, int direction)
    {
        ++m_navigationOperations;

        std::vector<size_t> visibleIndices;
        for (size_t index = 0; index < m_panels.size(); ++index)
        {
            if (m_panels[index].Visible)
            {
                visibleIndices.push_back(index);
            }
        }

        if (visibleIndices.empty())
        {
            return nullptr;
        }

        size_t currentVisibleIndex = 0;
        for (size_t index = 0; index < visibleIndices.size(); ++index)
        {
            if (m_panels[visibleIndices[index]].Name == currentPanel)
            {
                currentVisibleIndex = index;
                break;
            }
        }

        const int step = direction < 0 ? -1 : 1;
        const int next = static_cast<int>(currentVisibleIndex) + step;
        const size_t wrapped = static_cast<size_t>((next + static_cast<int>(visibleIndices.size())) % static_cast<int>(visibleIndices.size()));
        return &m_panels[visibleIndices[wrapped]];
    }

    bool EditorPanelRegistry::RegisterWorkspace(EditorWorkspaceDescriptor descriptor)
    {
        if (descriptor.Name.empty() || FindWorkspace(descriptor.Name) != nullptr)
        {
            return false;
        }

        m_workspaces.push_back(std::move(descriptor));
        std::sort(m_workspaces.begin(), m_workspaces.end(), [](const EditorWorkspaceDescriptor& left, const EditorWorkspaceDescriptor& right) {
            return left.Name < right.Name;
        });
        return true;
    }

    bool EditorPanelRegistry::ApplyWorkspace(const std::string& name)
    {
        const EditorWorkspaceDescriptor* workspace = FindWorkspace(name);
        if (workspace == nullptr)
        {
            ++m_missingPanelReferences;
            return false;
        }

        std::set<std::string> visiblePanelNames(workspace->VisiblePanels.begin(), workspace->VisiblePanels.end());
        for (const std::string& panelName : visiblePanelNames)
        {
            if (Find(panelName) == nullptr)
            {
                ++m_missingPanelReferences;
            }
        }

        for (EditorPanelDescriptor& panel : m_panels)
        {
            panel.Visible = visiblePanelNames.find(panel.Name) != visiblePanelNames.end();
        }

        ++m_appliedWorkspaces;
        return true;
    }

    const EditorWorkspaceDescriptor* EditorPanelRegistry::FindWorkspace(const std::string& name) const
    {
        const auto found = std::find_if(m_workspaces.begin(), m_workspaces.end(), [&name](const EditorWorkspaceDescriptor& workspace) {
            return workspace.Name == name;
        });
        return found == m_workspaces.end() ? nullptr : &(*found);
    }

    void EditorPanelRegistry::ResetVisibility()
    {
        for (EditorPanelDescriptor& panel : m_panels)
        {
            panel.Visible = panel.DefaultVisible;
        }
    }

    void EditorPanelRegistry::Clear()
    {
        m_panels.clear();
        m_workspaces.clear();
        m_toggleOperations = 0;
        m_appliedWorkspaces = 0;
        m_missingPanelReferences = 0;
        m_searchOperations = 0;
        m_navigationOperations = 0;
    }

    EditorPanelRegistryDiagnostics EditorPanelRegistry::GetDiagnostics() const
    {
        EditorPanelRegistryDiagnostics diagnostics;
        diagnostics.PanelCount = static_cast<uint32_t>(m_panels.size());
        diagnostics.ToggleOperations = m_toggleOperations;
        diagnostics.WorkspaceCount = static_cast<uint32_t>(m_workspaces.size());
        diagnostics.AppliedWorkspaces = m_appliedWorkspaces;
        diagnostics.MissingPanelReferences = m_missingPanelReferences;
        diagnostics.SearchOperations = m_searchOperations;
        diagnostics.NavigationOperations = m_navigationOperations;
        for (const EditorPanelDescriptor& panel : m_panels)
        {
            diagnostics.VisiblePanels += panel.Visible ? 1u : 0u;
            diagnostics.HiddenPanels += panel.Visible ? 0u : 1u;
            diagnostics.DockedPanels += panel.DockTarget.empty() ? 0u : 1u;
        }
        for (const EditorWorkspaceDescriptor& workspace : m_workspaces)
        {
            diagnostics.WorkspacePanelBindings += static_cast<uint32_t>(workspace.VisiblePanels.size());
            diagnostics.SavedDockLayouts += workspace.LayoutFile.empty() ? 0u : 1u;
            diagnostics.TeamDefaultWorkspaces += workspace.TeamDefault ? 1u : 0u;
            diagnostics.WorkspaceCommandBindings += static_cast<uint32_t>(workspace.CommandBindings.size());
            diagnostics.ControllerNavigationHints += static_cast<uint32_t>(workspace.ControllerNavigationHints.size());
            diagnostics.ToolbarCustomizationSlots += workspace.ToolbarSlots;
            diagnostics.MigrationReadyWorkspaces += (!workspace.LayoutFile.empty() && !workspace.DockLayout.empty()) ? 1u : 0u;
            diagnostics.FocusTargetWorkspaces += workspace.PreferredFocusPanel.empty() ? 0u : 1u;
            diagnostics.GamepadNavigableWorkspaces += workspace.GamepadNavigable ? 1u : 0u;
            diagnostics.ToolbarProfileWorkspaces += workspace.ToolbarSlots > 0u ? 1u : 0u;
            diagnostics.CommandRoutedWorkspaces += workspace.CommandBindings.empty() ? 0u : 1u;
        }
        return diagnostics;
    }

    const std::vector<EditorPanelDescriptor>& EditorPanelRegistry::GetPanels() const
    {
        return m_panels;
    }

    const std::vector<EditorWorkspaceDescriptor>& EditorPanelRegistry::GetWorkspaces() const
    {
        return m_workspaces;
    }

    EditorPanelDescriptor* EditorPanelRegistry::FindMutable(const std::string& name)
    {
        const auto found = std::find_if(m_panels.begin(), m_panels.end(), [&name](const EditorPanelDescriptor& panel) {
            return panel.Name == name;
        });
        return found == m_panels.end() ? nullptr : &(*found);
    }
}
