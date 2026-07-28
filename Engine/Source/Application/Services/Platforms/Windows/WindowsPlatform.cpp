#include "WindowsPlatform.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include "Core/String/OpaaxUtf8.h"   // I7 — the one UTF-8 <-> UTF-16 idiom

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
        const unsigned int lCount = Thread::hardware_concurrency();
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

        // Normalise to '/' (engine path convention) while still wide — one conversion, at the boundary.
        for (wchar_t& lCh : lWide) { if (lCh == L'\\') { lCh = L'/'; } }

        return Utf8::FromWide(lWide);
    }
}

#endif // OPAAX_PLATFORM_WINDOWS
