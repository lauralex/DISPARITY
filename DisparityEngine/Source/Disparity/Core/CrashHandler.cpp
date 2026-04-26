#include "Disparity/Core/CrashHandler.h"

#include "Disparity/Core/FileSystem.h"
#include "Disparity/Core/Version.h"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <windows.h>

namespace Disparity
{
    namespace
    {
        std::filesystem::path MakeCrashPath()
        {
            SYSTEMTIME time = {};
            GetLocalTime(&time);

            std::ostringstream name;
            name
                << "crash-"
                << std::setfill('0') << std::setw(4) << time.wYear
                << std::setw(2) << time.wMonth
                << std::setw(2) << time.wDay
                << "-"
                << std::setw(2) << time.wHour
                << std::setw(2) << time.wMinute
                << std::setw(2) << time.wSecond
                << ".txt";

            return std::filesystem::path("Saved") / "CrashLogs" / name.str();
        }

        LONG WINAPI WriteCrashReport(EXCEPTION_POINTERS* exceptionPointers)
        {
            const std::filesystem::path crashPath = MakeCrashPath();
            std::ostringstream report;
            report << Version::Name << " " << Version::ToString() << " " << Version::BuildConfiguration() << "\n";
            report << "Unhandled exception\n";

            if (exceptionPointers && exceptionPointers->ExceptionRecord)
            {
                const EXCEPTION_RECORD* record = exceptionPointers->ExceptionRecord;
                report << "Code: 0x" << std::hex << std::uppercase << record->ExceptionCode << "\n";
                report << "Address: 0x" << reinterpret_cast<std::uintptr_t>(record->ExceptionAddress) << "\n";
                report << "Flags: 0x" << record->ExceptionFlags << "\n";
            }

            const bool wrote = FileSystem::WriteTextFile(crashPath, report.str());
            (void)wrote;
            return EXCEPTION_EXECUTE_HANDLER;
        }
    }

    void CrashHandler::Install()
    {
        SetUnhandledExceptionFilter(WriteCrashReport);
    }
}
