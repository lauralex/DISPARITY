#pragma once

#include <cstdint>
#include <string>

namespace Disparity::Version
{
    inline constexpr uint32_t Major = 0;
    inline constexpr uint32_t Minor = 27;
    inline constexpr uint32_t Patch = 0;
    inline constexpr const char* Name = "DISPARITY Engine";

    [[nodiscard]] inline std::string ToString()
    {
        return std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch);
    }

    [[nodiscard]] inline const char* BuildConfiguration()
    {
#if defined(_DEBUG)
        return "Debug";
#else
        return "Release";
#endif
    }
}
