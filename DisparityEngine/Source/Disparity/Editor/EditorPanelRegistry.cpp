#include "Disparity/Editor/EditorPanelRegistry.h"

#include <algorithm>
#include <utility>

namespace Disparity
{
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
        m_toggleOperations = 0;
    }

    EditorPanelRegistryDiagnostics EditorPanelRegistry::GetDiagnostics() const
    {
        EditorPanelRegistryDiagnostics diagnostics;
        diagnostics.PanelCount = static_cast<uint32_t>(m_panels.size());
        diagnostics.ToggleOperations = m_toggleOperations;
        for (const EditorPanelDescriptor& panel : m_panels)
        {
            diagnostics.VisiblePanels += panel.Visible ? 1u : 0u;
            diagnostics.HiddenPanels += panel.Visible ? 0u : 1u;
            diagnostics.DockedPanels += panel.DockTarget.empty() ? 0u : 1u;
        }
        return diagnostics;
    }

    const std::vector<EditorPanelDescriptor>& EditorPanelRegistry::GetPanels() const
    {
        return m_panels;
    }

    EditorPanelDescriptor* EditorPanelRegistry::FindMutable(const std::string& name)
    {
        const auto found = std::find_if(m_panels.begin(), m_panels.end(), [&name](const EditorPanelDescriptor& panel) {
            return panel.Name == name;
        });
        return found == m_panels.end() ? nullptr : &(*found);
    }
}
