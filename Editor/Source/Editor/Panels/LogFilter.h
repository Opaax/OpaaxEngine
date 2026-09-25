#pragma once

#include "Core/Log/Logger.h"   // ELogLevel, LogEntry

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace Opaax::Editor
{
    /** The Log panel's level buttons. Critical shares Error's: both answer "what broke?". */
    enum class ELogLevelFilter : Uint8
    {
        Trace,
        Info,
        Warn,
        Error,
        Count
    };

    constexpr ELogLevelFilter ToLevelFilter(const ELogLevel InLevel) noexcept
    {
        switch (InLevel)
        {
        case ELogLevel::Trace: return ELogLevelFilter::Trace;
        case ELogLevel::Info:  return ELogLevelFilter::Info;
        case ELogLevel::Warn:  return ELogLevelFilter::Warn;
        default:               return ELogLevelFilter::Error;
        }
    }

    /** **I11** — the button's label. */
    inline const char* ToString(const ELogLevelFilter InFilter) noexcept
    {
        switch (InFilter)
        {
        case ELogLevelFilter::Trace: return "Trace";
        case ELogLevelFilter::Info:  return "Info";
        case ELogLevelFilter::Warn:  return "Warn";
        case ELogLevelFilter::Error: return "Error";
        default:                     return "?";
        }
    }

    /** ASCII case-insensitive substring. An empty needle is found everywhere. */
    inline bool ContainsNoCase(const std::string_view InText, const std::string_view InNeedle) noexcept
    {
        if (InNeedle.size() > InText.size()) { return false; }

        const auto lLower = [](const char InChar) { return std::tolower(static_cast<unsigned char>(InChar)); };

        for (size_t lStart = 0; lStart + InNeedle.size() <= InText.size(); ++lStart)
        {
            size_t lMatched = 0;
            while (lMatched < InNeedle.size() && lLower(InText[lStart + lMatched]) == lLower(InNeedle[lMatched]))
            {
                ++lMatched;
            }
            if (lMatched == InNeedle.size()) { return true; }
        }
        return false;
    }

    // =============================================================================
    // LogFilter — which lines the Log panel shows. Header-only and ImGui-free, so the rule is tested
    //   on its own; the panel only draws the controls that write it.
    // =============================================================================
    struct LogFilter
    {
        std::array<bool, static_cast<size_t>(ELogLevelFilter::Count)> ShowLevel{ true, true, true, true };

        /** Matched against the message, case-insensitively. Empty = every message. */
        std::string Search;

        /**
         * The categories switched OFF. A set of hidden rather than of shown, so a category that
         * first logs after the author filtered is shown — nobody has decided to hide it yet.
         */
        TUnorderedSet<OpaaxStringID> HiddenCategories;

        bool IsShown(const ELogLevelFilter InLevel) const noexcept { return ShowLevel[static_cast<size_t>(InLevel)]; }

        bool IsShown(const OpaaxStringID InCategory) const { return !HiddenCategories.contains(InCategory); }

        bool Passes(const LogEntry& InEntry) const
        {
            return IsShown(ToLevelFilter(InEntry.Level)) && IsShown(InEntry.Category)
                && ContainsNoCase(InEntry.Message.CStr(), Search);
        }
    };
}
