#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Disparity
{
    struct ServiceDescriptor
    {
        std::string Name;
        std::string Domain;
        uint32_t Version = 1;
        bool Required = false;
        bool Initialized = false;
    };

    struct ServiceRegistryDiagnostics
    {
        uint32_t RegisteredServices = 0;
        uint32_t InitializedServices = 0;
        uint32_t RequiredServices = 0;
        uint32_t MissingRequiredServices = 0;
        uint32_t DuplicateRegistrations = 0;
        uint32_t VersionChecksum = 0;
    };

    class ServiceRegistry
    {
    public:
        [[nodiscard]] bool RegisterService(ServiceDescriptor descriptor);
        [[nodiscard]] bool SetInitialized(const std::string& name, bool initialized);
        [[nodiscard]] bool IsRegistered(const std::string& name) const;
        [[nodiscard]] bool IsInitialized(const std::string& name) const;
        [[nodiscard]] const ServiceDescriptor* Find(const std::string& name) const;

        void Clear();

        [[nodiscard]] bool ValidateRequired() const;
        [[nodiscard]] ServiceRegistryDiagnostics GetDiagnostics() const;
        [[nodiscard]] const std::vector<ServiceDescriptor>& GetServices() const;

    private:
        std::vector<ServiceDescriptor> m_services;
        uint32_t m_duplicateRegistrations = 0;
    };
}
