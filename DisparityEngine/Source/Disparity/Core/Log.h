#pragma once

#include <string_view>

namespace Disparity
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error
    };

    void Log(LogLevel level, std::string_view message);
    void AssertFailed(const char* expression, const char* message, const char* file, int line);
}

#if defined(_DEBUG)
#define DISPARITY_ASSERT(expression, message) \
    do \
    { \
        if (!(expression)) \
        { \
            ::Disparity::AssertFailed(#expression, message, __FILE__, __LINE__); \
        } \
    } while (false)
#else
#define DISPARITY_ASSERT(expression, message) do { (void)sizeof(expression); } while (false)
#endif
