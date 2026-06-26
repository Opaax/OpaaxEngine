#include "WindowsPlatform.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include <thread>
#include <chrono>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace Opaax
{
    Uint32 WindowsPlatform::GetLogicalCoreCount() const
    {
        const unsigned int lCount = std::thread::hardware_concurrency();
        return lCount == 0u ? 1u : static_cast<Uint32>(lCount);
    }

    double WindowsPlatform::GetTimeSeconds() const
    {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    OpaaxString WindowsPlatform::GetExecutablePath() const
    {
        // Grow the buffer until the full path fits — long paths exceed MAX_PATH.
        std::wstring lWide(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD lLen = GetModuleFileNameW(nullptr, lWide.data(), static_cast<DWORD>(lWide.size()));
            if (lLen == 0)            { return OpaaxString(); }            // failed
            if (lLen < lWide.size())  { lWide.resize(lLen); break; }       // fit (no trailing null)
            lWide.resize(lWide.size() * 2);                                // truncated -> grow
        }

        // UTF-16 -> UTF-8.
        const int lSize = WideCharToMultiByte(CP_UTF8, 0, lWide.data(), static_cast<int>(lWide.size()),
                                              nullptr, 0, nullptr, nullptr);
        std::string lUtf8(static_cast<size_t>(lSize), '\0');
        WideCharToMultiByte(CP_UTF8, 0, lWide.data(), static_cast<int>(lWide.size()),
                            lUtf8.data(), lSize, nullptr, nullptr);

        // Normalise to '/' (engine path convention).
        for (char& lCh : lUtf8) { if (lCh == '\\') { lCh = '/'; } }
        return OpaaxString(lUtf8.c_str());
    }
}

#endif // OPAAX_PLATFORM_WINDOWS
