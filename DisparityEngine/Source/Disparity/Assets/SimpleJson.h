#pragma once

#include <map>
#include <string>
#include <vector>

namespace Disparity
{
    class JsonValue
    {
    public:
        enum class Type
        {
            Null,
            Bool,
            Number,
            String,
            Array,
            Object
        };

        Type ValueType = Type::Null;
        bool Bool = false;
        double Number = 0.0;
        std::string String;
        std::vector<JsonValue> Array;
        std::map<std::string, JsonValue> Object;

        [[nodiscard]] bool IsObject() const;
        [[nodiscard]] bool IsArray() const;
        [[nodiscard]] bool IsString() const;
        [[nodiscard]] bool IsNumber() const;
        [[nodiscard]] bool Has(const std::string& key) const;
        [[nodiscard]] const JsonValue* Find(const std::string& key) const;
        [[nodiscard]] const JsonValue* At(size_t index) const;
        [[nodiscard]] std::string AsString(const std::string& fallback = {}) const;
        [[nodiscard]] double AsNumber(double fallback = 0.0) const;
        [[nodiscard]] int AsInt(int fallback = 0) const;
    };

    class SimpleJson
    {
    public:
        [[nodiscard]] static bool Parse(const std::string& text, JsonValue& outValue, std::string* outError = nullptr);
    };
}
