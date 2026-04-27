#include "Disparity/Runtime/ConfigVarRegistry.h"

#include <algorithm>
#include <utility>

namespace Disparity
{
    bool ConfigVarRegistry::RegisterBool(std::string name, bool value, std::string description)
    {
        return Register({ std::move(name), std::move(description), ConfigVarType::Bool, value, value, false });
    }

    bool ConfigVarRegistry::RegisterInt(std::string name, int32_t value, std::string description)
    {
        return Register({ std::move(name), std::move(description), ConfigVarType::Int, value, value, false });
    }

    bool ConfigVarRegistry::RegisterFloat(std::string name, float value, std::string description)
    {
        return Register({ std::move(name), std::move(description), ConfigVarType::Float, value, value, false });
    }

    bool ConfigVarRegistry::RegisterString(std::string name, std::string value, std::string description)
    {
        const std::string defaultValue = value;
        return Register({ std::move(name), std::move(description), ConfigVarType::String, std::move(value), defaultValue, false });
    }

    bool ConfigVarRegistry::SetBool(const std::string& name, bool value)
    {
        ConfigVar* variable = FindMutable(name);
        if (variable == nullptr || variable->Type != ConfigVarType::Bool)
        {
            return false;
        }

        variable->Value = value;
        variable->Dirty = variable->Value != variable->DefaultValue;
        ++m_setOperations;
        return true;
    }

    bool ConfigVarRegistry::SetInt(const std::string& name, int32_t value)
    {
        ConfigVar* variable = FindMutable(name);
        if (variable == nullptr || variable->Type != ConfigVarType::Int)
        {
            return false;
        }

        variable->Value = value;
        variable->Dirty = variable->Value != variable->DefaultValue;
        ++m_setOperations;
        return true;
    }

    bool ConfigVarRegistry::SetFloat(const std::string& name, float value)
    {
        ConfigVar* variable = FindMutable(name);
        if (variable == nullptr || variable->Type != ConfigVarType::Float)
        {
            return false;
        }

        variable->Value = value;
        variable->Dirty = variable->Value != variable->DefaultValue;
        ++m_setOperations;
        return true;
    }

    bool ConfigVarRegistry::SetString(const std::string& name, std::string value)
    {
        ConfigVar* variable = FindMutable(name);
        if (variable == nullptr || variable->Type != ConfigVarType::String)
        {
            return false;
        }

        variable->Value = std::move(value);
        variable->Dirty = variable->Value != variable->DefaultValue;
        ++m_setOperations;
        return true;
    }

    bool ConfigVarRegistry::GetBool(const std::string& name, bool fallback) const
    {
        const ConfigVar* variable = Find(name);
        return variable != nullptr && variable->Type == ConfigVarType::Bool ? std::get<bool>(variable->Value) : fallback;
    }

    int32_t ConfigVarRegistry::GetInt(const std::string& name, int32_t fallback) const
    {
        const ConfigVar* variable = Find(name);
        return variable != nullptr && variable->Type == ConfigVarType::Int ? std::get<int32_t>(variable->Value) : fallback;
    }

    float ConfigVarRegistry::GetFloat(const std::string& name, float fallback) const
    {
        const ConfigVar* variable = Find(name);
        return variable != nullptr && variable->Type == ConfigVarType::Float ? std::get<float>(variable->Value) : fallback;
    }

    std::string ConfigVarRegistry::GetString(const std::string& name, std::string fallback) const
    {
        const ConfigVar* variable = Find(name);
        return variable != nullptr && variable->Type == ConfigVarType::String ? std::get<std::string>(variable->Value) : fallback;
    }

    void ConfigVarRegistry::ClearDirty()
    {
        for (ConfigVar& variable : m_vars)
        {
            variable.Dirty = false;
            variable.DefaultValue = variable.Value;
        }
    }

    void ConfigVarRegistry::Clear()
    {
        m_vars.clear();
        m_setOperations = 0;
    }

    const ConfigVar* ConfigVarRegistry::Find(const std::string& name) const
    {
        const auto found = std::find_if(m_vars.begin(), m_vars.end(), [&name](const ConfigVar& variable) {
            return variable.Name == name;
        });
        return found == m_vars.end() ? nullptr : &(*found);
    }

    ConfigVarRegistryDiagnostics ConfigVarRegistry::GetDiagnostics() const
    {
        ConfigVarRegistryDiagnostics diagnostics;
        diagnostics.RegisteredVars = static_cast<uint32_t>(m_vars.size());
        diagnostics.SetOperations = m_setOperations;
        for (const ConfigVar& variable : m_vars)
        {
            diagnostics.DirtyVars += variable.Dirty ? 1u : 0u;
            diagnostics.BoolVars += variable.Type == ConfigVarType::Bool ? 1u : 0u;
            diagnostics.IntVars += variable.Type == ConfigVarType::Int ? 1u : 0u;
            diagnostics.FloatVars += variable.Type == ConfigVarType::Float ? 1u : 0u;
            diagnostics.StringVars += variable.Type == ConfigVarType::String ? 1u : 0u;
        }
        return diagnostics;
    }

    const std::vector<ConfigVar>& ConfigVarRegistry::GetVars() const
    {
        return m_vars;
    }

    bool ConfigVarRegistry::Register(ConfigVar variable)
    {
        if (variable.Name.empty() || Find(variable.Name) != nullptr)
        {
            return false;
        }

        m_vars.push_back(std::move(variable));
        return true;
    }

    ConfigVar* ConfigVarRegistry::FindMutable(const std::string& name)
    {
        const auto found = std::find_if(m_vars.begin(), m_vars.end(), [&name](const ConfigVar& variable) {
            return variable.Name == name;
        });
        return found == m_vars.end() ? nullptr : &(*found);
    }
}
