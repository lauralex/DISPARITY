#include "Disparity/Core/ServiceRegistry.h"

#include <algorithm>
#include <utility>

namespace Disparity
{
    bool ServiceRegistry::RegisterService(ServiceDescriptor descriptor)
    {
        if (descriptor.Name.empty())
        {
            return false;
        }

        auto existing = std::find_if(m_services.begin(), m_services.end(), [&descriptor](const ServiceDescriptor& service) {
            return service.Name == descriptor.Name;
        });

        if (existing != m_services.end())
        {
            *existing = std::move(descriptor);
            ++m_duplicateRegistrations;
            return true;
        }

        m_services.push_back(std::move(descriptor));
        return true;
    }

    bool ServiceRegistry::SetInitialized(const std::string& name, bool initialized)
    {
        auto existing = std::find_if(m_services.begin(), m_services.end(), [&name](const ServiceDescriptor& service) {
            return service.Name == name;
        });
        if (existing == m_services.end())
        {
            return false;
        }

        existing->Initialized = initialized;
        return true;
    }

    bool ServiceRegistry::IsRegistered(const std::string& name) const
    {
        return Find(name) != nullptr;
    }

    bool ServiceRegistry::IsInitialized(const std::string& name) const
    {
        const ServiceDescriptor* service = Find(name);
        return service != nullptr && service->Initialized;
    }

    const ServiceDescriptor* ServiceRegistry::Find(const std::string& name) const
    {
        const auto existing = std::find_if(m_services.begin(), m_services.end(), [&name](const ServiceDescriptor& service) {
            return service.Name == name;
        });
        return existing == m_services.end() ? nullptr : &(*existing);
    }

    void ServiceRegistry::Clear()
    {
        m_services.clear();
        m_duplicateRegistrations = 0;
    }

    bool ServiceRegistry::ValidateRequired() const
    {
        return std::none_of(m_services.begin(), m_services.end(), [](const ServiceDescriptor& service) {
            return service.Required && !service.Initialized;
        });
    }

    ServiceRegistryDiagnostics ServiceRegistry::GetDiagnostics() const
    {
        ServiceRegistryDiagnostics diagnostics;
        diagnostics.RegisteredServices = static_cast<uint32_t>(m_services.size());
        diagnostics.DuplicateRegistrations = m_duplicateRegistrations;
        for (const ServiceDescriptor& service : m_services)
        {
            diagnostics.InitializedServices += service.Initialized ? 1u : 0u;
            diagnostics.RequiredServices += service.Required ? 1u : 0u;
            diagnostics.MissingRequiredServices += service.Required && !service.Initialized ? 1u : 0u;
            diagnostics.VersionChecksum += service.Version;
        }
        return diagnostics;
    }

    const std::vector<ServiceDescriptor>& ServiceRegistry::GetServices() const
    {
        return m_services;
    }
}
