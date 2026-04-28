#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Disparity
{
    struct RuntimeCommandDescriptor
    {
        std::string Name;
        std::string Category;
        std::string Description;
        std::string DefaultBinding;
        bool Enabled = true;
    };

    struct RuntimeCommandRegistryDiagnostics
    {
        uint32_t RegisteredCommands = 0;
        uint32_t EnabledCommands = 0;
        uint32_t DisabledCommands = 0;
        uint32_t Categories = 0;
        uint32_t CategorySummaries = 0;
        uint32_t BindingConflicts = 0;
        uint32_t DuplicateRegistrations = 0;
        uint32_t ExecuteOperations = 0;
        uint32_t FailedExecuteOperations = 0;
        uint32_t SearchOperations = 0;
        uint32_t HistoryEntries = 0;
        uint32_t RequiredCategoriesSatisfied = 0;
    };

    struct RuntimeCommandHistoryEntry
    {
        std::string Name;
        std::string Category;
        bool Succeeded = false;
        uint32_t Sequence = 0;
    };

    struct RuntimeCommandCategorySummary
    {
        std::string Category;
        uint32_t RegisteredCommands = 0;
        uint32_t EnabledCommands = 0;
        uint32_t DisabledCommands = 0;
        uint32_t BoundCommands = 0;
    };

    class RuntimeCommandRegistry
    {
    public:
        [[nodiscard]] bool RegisterCommand(RuntimeCommandDescriptor descriptor);
        [[nodiscard]] bool SetEnabled(const std::string& name, bool enabled);
        [[nodiscard]] bool Execute(const std::string& name);
        [[nodiscard]] std::vector<RuntimeCommandDescriptor> FindByCategory(const std::string& category) const;
        [[nodiscard]] std::vector<RuntimeCommandDescriptor> Search(const std::string& query);
        [[nodiscard]] const RuntimeCommandDescriptor* Find(const std::string& name) const;
        [[nodiscard]] std::vector<RuntimeCommandCategorySummary> BuildCategorySummaries() const;
        [[nodiscard]] bool ValidateRequiredCategories(const std::vector<std::string>& categories);

        void Clear();

        [[nodiscard]] RuntimeCommandRegistryDiagnostics GetDiagnostics() const;
        [[nodiscard]] const std::vector<RuntimeCommandDescriptor>& GetCommands() const;
        [[nodiscard]] const std::vector<RuntimeCommandHistoryEntry>& GetHistory() const;

    private:
        [[nodiscard]] RuntimeCommandDescriptor* FindMutable(const std::string& name);
        [[nodiscard]] uint32_t CountBindingConflicts() const;

        std::vector<RuntimeCommandDescriptor> m_commands;
        std::vector<RuntimeCommandHistoryEntry> m_history;
        uint32_t m_duplicateRegistrations = 0;
        uint32_t m_executeOperations = 0;
        uint32_t m_failedExecuteOperations = 0;
        uint32_t m_searchOperations = 0;
        uint32_t m_requiredCategoriesSatisfied = 0;
        uint32_t m_nextHistorySequence = 1;
    };
}
