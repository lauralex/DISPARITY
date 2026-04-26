#include "Disparity/Assets/SimpleJson.h"

#include <charconv>
#include <cctype>

namespace Disparity
{
    namespace
    {
        class Parser
        {
        public:
            explicit Parser(const std::string& text)
                : m_text(text)
            {
            }

            bool Parse(JsonValue& outValue, std::string* outError)
            {
                SkipWhitespace();
                if (!ParseValue(outValue))
                {
                    if (outError)
                    {
                        *outError = "Invalid JSON near byte " + std::to_string(m_position);
                    }
                    return false;
                }

                SkipWhitespace();
                if (m_position != m_text.size())
                {
                    if (outError)
                    {
                        *outError = "Unexpected trailing JSON content near byte " + std::to_string(m_position);
                    }
                    return false;
                }

                return true;
            }

        private:
            void SkipWhitespace()
            {
                while (m_position < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_position])) != 0)
                {
                    ++m_position;
                }
            }

            bool Match(char value)
            {
                SkipWhitespace();
                if (m_position < m_text.size() && m_text[m_position] == value)
                {
                    ++m_position;
                    return true;
                }

                return false;
            }

            bool ParseValue(JsonValue& outValue)
            {
                SkipWhitespace();
                if (m_position >= m_text.size())
                {
                    return false;
                }

                const char value = m_text[m_position];
                if (value == '{')
                {
                    return ParseObject(outValue);
                }
                if (value == '[')
                {
                    return ParseArray(outValue);
                }
                if (value == '"')
                {
                    outValue.ValueType = JsonValue::Type::String;
                    return ParseString(outValue.String);
                }
                if (value == '-' || std::isdigit(static_cast<unsigned char>(value)) != 0)
                {
                    outValue.ValueType = JsonValue::Type::Number;
                    return ParseNumber(outValue.Number);
                }
                if (m_text.compare(m_position, 4, "true") == 0)
                {
                    m_position += 4;
                    outValue.ValueType = JsonValue::Type::Bool;
                    outValue.Bool = true;
                    return true;
                }
                if (m_text.compare(m_position, 5, "false") == 0)
                {
                    m_position += 5;
                    outValue.ValueType = JsonValue::Type::Bool;
                    outValue.Bool = false;
                    return true;
                }
                if (m_text.compare(m_position, 4, "null") == 0)
                {
                    m_position += 4;
                    outValue.ValueType = JsonValue::Type::Null;
                    return true;
                }

                return false;
            }

            bool ParseObject(JsonValue& outValue)
            {
                if (!Match('{'))
                {
                    return false;
                }

                outValue.ValueType = JsonValue::Type::Object;
                outValue.Object.clear();

                if (Match('}'))
                {
                    return true;
                }

                while (m_position < m_text.size())
                {
                    std::string key;
                    if (!ParseString(key) || !Match(':'))
                    {
                        return false;
                    }

                    JsonValue value;
                    if (!ParseValue(value))
                    {
                        return false;
                    }

                    outValue.Object.emplace(std::move(key), std::move(value));

                    if (Match('}'))
                    {
                        return true;
                    }

                    if (!Match(','))
                    {
                        return false;
                    }
                }

                return false;
            }

            bool ParseArray(JsonValue& outValue)
            {
                if (!Match('['))
                {
                    return false;
                }

                outValue.ValueType = JsonValue::Type::Array;
                outValue.Array.clear();

                if (Match(']'))
                {
                    return true;
                }

                while (m_position < m_text.size())
                {
                    JsonValue value;
                    if (!ParseValue(value))
                    {
                        return false;
                    }

                    outValue.Array.push_back(std::move(value));

                    if (Match(']'))
                    {
                        return true;
                    }

                    if (!Match(','))
                    {
                        return false;
                    }
                }

                return false;
            }

            bool ParseString(std::string& outString)
            {
                SkipWhitespace();
                if (m_position >= m_text.size() || m_text[m_position] != '"')
                {
                    return false;
                }

                ++m_position;
                outString.clear();

                while (m_position < m_text.size())
                {
                    const char value = m_text[m_position++];
                    if (value == '"')
                    {
                        return true;
                    }

                    if (value == '\\')
                    {
                        if (m_position >= m_text.size())
                        {
                            return false;
                        }

                        const char escaped = m_text[m_position++];
                        switch (escaped)
                        {
                        case '"': outString.push_back('"'); break;
                        case '\\': outString.push_back('\\'); break;
                        case '/': outString.push_back('/'); break;
                        case 'b': outString.push_back('\b'); break;
                        case 'f': outString.push_back('\f'); break;
                        case 'n': outString.push_back('\n'); break;
                        case 'r': outString.push_back('\r'); break;
                        case 't': outString.push_back('\t'); break;
                        default: outString.push_back(escaped); break;
                        }
                    }
                    else
                    {
                        outString.push_back(value);
                    }
                }

                return false;
            }

            bool ParseNumber(double& outNumber)
            {
                SkipWhitespace();
                const size_t begin = m_position;

                if (m_position < m_text.size() && m_text[m_position] == '-')
                {
                    ++m_position;
                }

                while (m_position < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_position])) != 0)
                {
                    ++m_position;
                }

                if (m_position < m_text.size() && m_text[m_position] == '.')
                {
                    ++m_position;
                    while (m_position < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_position])) != 0)
                    {
                        ++m_position;
                    }
                }

                if (m_position < m_text.size() && (m_text[m_position] == 'e' || m_text[m_position] == 'E'))
                {
                    ++m_position;
                    if (m_position < m_text.size() && (m_text[m_position] == '+' || m_text[m_position] == '-'))
                    {
                        ++m_position;
                    }
                    while (m_position < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_position])) != 0)
                    {
                        ++m_position;
                    }
                }

                const std::string numberText = m_text.substr(begin, m_position - begin);
                try
                {
                    outNumber = std::stod(numberText);
                    return true;
                }
                catch (...)
                {
                    return false;
                }
            }

            const std::string& m_text;
            size_t m_position = 0;
        };
    }

    bool JsonValue::IsObject() const
    {
        return ValueType == Type::Object;
    }

    bool JsonValue::IsArray() const
    {
        return ValueType == Type::Array;
    }

    bool JsonValue::IsString() const
    {
        return ValueType == Type::String;
    }

    bool JsonValue::IsNumber() const
    {
        return ValueType == Type::Number;
    }

    bool JsonValue::Has(const std::string& key) const
    {
        return Find(key) != nullptr;
    }

    const JsonValue* JsonValue::Find(const std::string& key) const
    {
        const auto found = Object.find(key);
        return found == Object.end() ? nullptr : &found->second;
    }

    const JsonValue* JsonValue::At(size_t index) const
    {
        return index < Array.size() ? &Array[index] : nullptr;
    }

    std::string JsonValue::AsString(const std::string& fallback) const
    {
        return IsString() ? String : fallback;
    }

    double JsonValue::AsNumber(double fallback) const
    {
        return IsNumber() ? Number : fallback;
    }

    int JsonValue::AsInt(int fallback) const
    {
        return IsNumber() ? static_cast<int>(Number) : fallback;
    }

    bool SimpleJson::Parse(const std::string& text, JsonValue& outValue, std::string* outError)
    {
        return Parser(text).Parse(outValue, outError);
    }
}
