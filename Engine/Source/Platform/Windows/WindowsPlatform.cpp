#include "Platform/Windows/WindowsPlatform.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include "Core/String/OpaaxUtf8.h"
#include "Core/Time/Clock.h"

#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <ShlObj.h>

// Older SDKs do not name it; Windows before 10 1803 refuses it, and the wait spins.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace Opaax
{
    Uint32 WindowsPlatform::GetLogicalCoreCount() const
    {
        const unsigned int lCount = Thread::hardware_concurrency();
        return lCount == 0u ? 1u : static_cast<Uint32>(lCount);
    }

    double WindowsPlatform::GetTimeSeconds() const
    {
        return Clock::NowSeconds();
    }

    void WindowsPlatform::SleepPrecise(const double InSeconds) const
    {
        const double lEnd   = Clock::NowSeconds() + InSeconds;
        const double lSleep = InSeconds - PRECISE_SLEEP_SPIN;

        // Sleep() wakes on the system tick (15.6 ms unless a program raised it); a high-resolution
        // waitable timer (Windows 10 1803 and later) within a fraction of a millisecond. Without one,
        // the whole wait spins.
        if (lSleep > 0.0)
        {
            HANDLE lTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                                   TIMER_ALL_ACCESS);
            if (lTimer != nullptr)
            {
                LARGE_INTEGER lDue;
                lDue.QuadPart = -static_cast<LONGLONG>(lSleep * 1.0e7);   // relative, in 100 ns
                if (SetWaitableTimerEx(lTimer, &lDue, 0, nullptr, nullptr, nullptr, 0))
                {
                    WaitForSingleObject(lTimer, INFINITE);
                }
                CloseHandle(lTimer);
            }
        }

        while (Clock::NowSeconds() < lEnd)
        {
            ThisThread::YieldNow();
        }
    }

    OpaaxString WindowsPlatform::GetExecutablePath() const
    {
        // Grow the buffer until the path fits (long paths exceed MAX_PATH).
        std::wstring lWide(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD lLen = GetModuleFileNameW(nullptr, lWide.data(), static_cast<DWORD>(lWide.size()));
            if (lLen == 0)            { return OpaaxString(); }            // failed
            if (lLen < lWide.size())  { lWide.resize(lLen); break; }       // fits (no trailing null)
            lWide.resize(lWide.size() * 2);                                // truncated -> grow
        }

        // Convert to '/' while still wide, then one conversion.
        for (wchar_t& lCh : lWide) { if (lCh == L'\\') { lCh = L'/'; } }

        return Utf8::FromWide(lWide);
    }

    OpaaxString WindowsPlatform::GetUserDataDirectory() const
    {
        // %APPDATA%: roams with the user's profile, as most games' saves do.
        PWSTR lRaw = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &lRaw)))
        {
            CoTaskMemFree(lRaw);
            return OpaaxString();
        }

        std::wstring lWide(lRaw);
        CoTaskMemFree(lRaw);

        for (wchar_t& lCh : lWide) { if (lCh == L'\\') { lCh = L'/'; } }

        return Utf8::FromWide(lWide);
    }
}

#endif // OPAAX_PLATFORM_WINDOWS
