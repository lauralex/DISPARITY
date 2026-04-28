#include "Disparity/Runtime/RuntimeCommandRegistry.h"

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

    bool RuntimeCommandRegistry::RegisterCommand(RuntimeCommandDescriptor descriptor)
    {
        if (descriptor.Name.empty())
        {
            return false;
        }

        RuntimeCommandDescriptor* existing = FindMutable(descriptor.Name);
        if (existing != nullptr)
        {
            *existing = std::move(descriptor);
            ++m_duplicateRegistrations;
            return true;
        }

        m_commands.push_back(std::move(descriptor));
        std::sort(m_commands.begin(), m_commands.end(), [](const RuntimeCommandDescriptor& left, const RuntimeCommandDescriptor& right) {
            if (left.Category == right.Category)
            {
                return left.Name < right.Name;
            }
            return left.Category < right.Category;
        });
        return true;
    }

    bool RuntimeCommandRegistry::SetEnabled(const std::string& name, bool enabled)
    {
        RuntimeCommandDescriptor* command = FindMutable(name);
        if (command == nullptr)
        {
            return false;
        }

        command->Enabled = enabled;
        return true;
    }

    bool RuntimeCommandRegistry::Execute(const std::string& name)
    {
        RuntimeCommandDescriptor* command = FindMutable(name);
        if (command == nullptr || !command->Enabled)
        {
            ++m_failedExecuteOperations;
            m_history.push_back({ name, command != nullptr ? command->Category : std::string(), false, m_nextHistorySequence++ });
            return false;
        }

        ++m_executeOperations;
        m_history.push_back({ command->Name, command->Category, true, m_nextHistorySequence++ });
        return true;
    }

    std::vector<RuntimeCommandDescriptor> RuntimeCommandRegistry::FindByCategory(const std::string& category) const
    {
        std::vector<RuntimeCommandDescriptor> commands;
        for (const RuntimeCommandDescriptor& command : m_commands)
        {
            if (command.Category == category)
            {
                commands.push_back(command);
            }
        }
        return commands;
    }

    std::vector<RuntimeCommandDescriptor> RuntimeCommandRegistry::Search(const std::string& query)
    {
        ++m_searchOperations;

        std::vector<RuntimeCommandDescriptor> commands;
        for (const RuntimeCommandDescriptor& command : m_commands)
        {
            if (ContainsCaseInsensitive(command.Name, query) ||
                ContainsCaseInsensitive(command.Category, query) ||
                ContainsCaseInsensitive(command.Description, query) ||
                ContainsCaseInsensitive(command.DefaultBinding, query))
            {
                commands.push_back(command);
            }
        }
        return commands;
    }

    const RuntimeCommandDescriptor* RuntimeCommandRegistry::Find(const std::string& name) const
    {
        const auto found = std::find_if(m_commands.begin(), m_commands.end(), [&name](const RuntimeCommandDescriptor& command) {
            return command.Name == name;
        });
        return found == m_commands.end() ? nullptr : &(*found);
    }

    std::vector<RuntimeCommandCategorySummary> RuntimeCommandRegistry::BuildCategorySummaries() const
    {
        std::vector<RuntimeCommandCategorySummary> summaries;
        for (const RuntimeCommandDescriptor& command : m_commands)
        {
            const std::string category = command.Category.empty() ? "Uncategorized" : command.Category;
            auto found = std::find_if(summaries.begin(), summaries.end(), [&category](const RuntimeCommandCategorySummary& summary) {
                return summary.Category == category;
            });
            if (found == summaries.end())
            {
                summaries.push_back({ category });
                found = summaries.end() - 1;
            }

            ++found->RegisteredCommands;
            found->EnabledCommands += command.Enabled ? 1u : 0u;
            found->DisabledCommands += command.Enabled ? 0u : 1u;
            found->BoundCommands += command.DefaultBinding.empty() ? 0u : 1u;
        }
        return summaries;
    }

    bool RuntimeCommandRegistry::ValidateRequiredCategories(const std::vector<std::string>& categories)
    {
        uint32_t satisfied = 0;
        for (const std::string& category : categories)
        {
            if (!FindByCategory(category).empty())
            {
                ++satisfied;
            }
        }

        m_requiredCategoriesSatisfied = satisfied;
        return satisfied == categories.size();
    }

    void RuntimeCommandRegistry::Clear()
    {
        m_commands.clear();
        m_duplicateRegistrations = 0;
        m_executeOperations = 0;
        m_failedExecuteOperations = 0;
        m_searchOperations = 0;
        m_requiredCategoriesSatisfied = 0;
        m_nextHistorySequence = 1;
        m_history.clear();
    }

    RuntimeCommandRegistryDiagnostics RuntimeCommandRegistry::GetDiagnostics() const
    {
        RuntimeCommandRegistryDiagnostics diagnostics;
        diagnostics.RegisteredCommands = static_cast<uint32_t>(m_commands.size());
        diagnostics.DuplicateRegistrations = m_duplicateRegistrations;
        diagnostics.ExecuteOperations = m_executeOperations;
        diagnostics.FailedExecuteOperations = m_failedExecuteOperations;
        diagnostics.SearchOperations = m_searchOperations;
        diagnostics.HistoryEntries = static_cast<uint32_t>(m_history.size());
        diagnostics.BindingConflicts = CountBindingConflicts();
        diagnostics.RequiredCategoriesSatisfied = m_requiredCategoriesSatisfied;

        std::set<std::string> categories;
        std::set<std::string> uniqueBindings;
        for (const RuntimeCommandDescriptor& command : m_commands)
        {
            diagnostics.EnabledCommands += command.Enabled ? 1u : 0u;
            diagnostics.DisabledCommands += command.Enabled ? 0u : 1u;
            diagnostics.DocumentedCommands += command.Description.empty() ? 0u : 1u;
            if (!command.DefaultBinding.empty())
            {
                ++diagnostics.BoundCommands;
                uniqueBindings.insert(command.DefaultBinding);
            }
            if (!command.Category.empty())
            {
                categories.insert(command.Category);
            }
        }
        for (const RuntimeCommandHistoryEntry& entry : m_history)
        {
            diagnostics.HistorySuccesses += entry.Succeeded ? 1u : 0u;
            diagnostics.HistoryFailures += entry.Succeeded ? 0u : 1u;
        }
        diagnostics.Categories = static_cast<uint32_t>(categories.size());
        diagnostics.UniqueBindings = static_cast<uint32_t>(uniqueBindings.size());
        diagnostics.CategorySummaries = static_cast<uint32_t>(BuildCategorySummaries().size());
        return diagnostics;
    }

    const std::vector<RuntimeCommandDescriptor>& RuntimeCommandRegistry::GetCommands() const
    {
        return m_commands;
    }

    const std::vector<RuntimeCommandHistoryEntry>& RuntimeCommandRegistry::GetHistory() const
    {
        return m_history;
    }

    RuntimeCommandDescriptor* RuntimeCommandRegistry::FindMutable(const std::string& name)
    {
        const auto found = std::find_if(m_commands.begin(), m_commands.end(), [&name](const RuntimeCommandDescriptor& command) {
            return command.Name == name;
        });
        return found == m_commands.end() ? nullptr : &(*found);
    }

    uint32_t RuntimeCommandRegistry::CountBindingConflicts() const
    {
        uint32_t conflicts = 0;
        std::set<std::string> bindings;
        std::set<std::string> conflictedBindings;
        for (const RuntimeCommandDescriptor& command : m_commands)
        {
            if (command.DefaultBinding.empty())
            {
                continue;
            }

            if (!bindings.insert(command.DefaultBinding).second)
            {
                conflictedBindings.insert(command.DefaultBinding);
            }
        }
        conflicts = static_cast<uint32_t>(conflictedBindings.size());
        return conflicts;
    }
}
