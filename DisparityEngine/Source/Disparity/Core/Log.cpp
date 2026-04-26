#include "Disparity/Core/Log.h"

#include <cstdio>
#include <string>
#include <windows.h>

namespace Disparity
{
    namespace
    {
        const char* Prefix(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Info:
                return "[DISPARITY][Info] ";
            case LogLevel::Warning:
                return "[DISPARITY][Warning] ";
            case LogLevel::Error:
                return "[DISPARITY][Error] ";
            default:
                return "[DISPARITY] ";
            }
        }
    }

    void Log(LogLevel level, std::string_view message)
    {
        std::string line = Prefix(level);
        line.append(message.data(), message.size());
        line.push_back('\n');

        OutputDebugStringA(line.c_str());
        std::fputs(line.c_str(), stderr);
    }

    void AssertFailed(const char* expression, const char* message, const char* file, int line)
    {
        char buffer[1024] = {};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "Assertion failed: %s\nMessage: %s\nFile: %s\nLine: %d",
            expression,
            message,
            file,
            line);

        Log(LogLevel::Error, buffer);
        MessageBoxA(nullptr, buffer, "DISPARITY Assertion Failed", MB_ICONERROR | MB_OK);
        DebugBreak();
    }
}
