#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace Disparity
{
    enum class ConfigVarType : uint32_t
    {
        Bool,
        Int,
        Float,
        String
    };

    using ConfigVarValue = std::variant<bool, int32_t, float, std::string>;

    struct ConfigVar
    {
        std::string Name;
        std::string Description;
        ConfigVarType Type = ConfigVarType::Bool;
        ConfigVarValue Value = false;
        ConfigVarValue DefaultValue = false;
        bool Dirty = false;
    };

    struct ConfigVarRegistryDiagnostics
    {
        uint32_t RegisteredVars = 0;
        uint32_t DirtyVars = 0;
        uint32_t BoolVars = 0;
        uint32_t IntVars = 0;
        uint32_t FloatVars = 0;
        uint32_t StringVars = 0;
        uint32_t SetOperations = 0;
    };

    class ConfigVarRegistry
    {
    public:
        [[nodiscard]] bool RegisterBool(std::string name, bool value, std::string description = {});
        [[nodiscard]] bool RegisterInt(std::string name, int32_t value, std::string description = {});
        [[nodiscard]] bool RegisterFloat(std::string name, float value, std::string description = {});
        [[nodiscard]] bool RegisterString(std::string name, std::string value, std::string description = {});

        [[nodiscard]] bool SetBool(const std::string& name, bool value);
        [[nodiscard]] bool SetInt(const std::string& name, int32_t value);
        [[nodiscard]] bool SetFloat(const std::string& name, float value);
        [[nodiscard]] bool SetString(const std::string& name, std::string value);

        [[nodiscard]] bool GetBool(const std::string& name, bool fallback = false) const;
        [[nodiscard]] int32_t GetInt(const std::string& name, int32_t fallback = 0) const;
        [[nodiscard]] float GetFloat(const std::string& name, float fallback = 0.0f) const;
        [[nodiscard]] std::string GetString(const std::string& name, std::string fallback = {}) const;

        void ClearDirty();
        void Clear();

        [[nodiscard]] const ConfigVar* Find(const std::string& name) const;
        [[nodiscard]] ConfigVarRegistryDiagnostics GetDiagnostics() const;
        [[nodiscard]] const std::vector<ConfigVar>& GetVars() const;

    private:
        [[nodiscard]] bool Register(ConfigVar variable);
        [[nodiscard]] ConfigVar* FindMutable(const std::string& name);

        std::vector<ConfigVar> m_vars;
        uint32_t m_setOperations = 0;
    };
}
