#pragma once

#include "Core/Log/Logger.h"   // ELogLevel, LogEntry

#include "Core/OpaaxTypes.h"
#include "Core/String/Char.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    /** The Log panel's level buttons. Critical shares Error's. */
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

    /** The button's label. */
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

    /** ASCII case-insensitive substring. An empty needle always matches. */
    inline bool ContainsNoCase(const OpaaxStringView InText, const OpaaxStringView InNeedle) noexcept
    {
        if (InNeedle.GetLength() > InText.GetLength()) { return false; }

        const auto lLower = [](const char InChar) { return Char::ToLower(InChar); };

        for (Uint32 lStart = 0; lStart + InNeedle.GetLength() <= InText.GetLength(); ++lStart)
        {
            Uint32 lMatched = 0;
            while (lMatched < InNeedle.GetLength() && lLower(InText[lStart + lMatched]) == lLower(InNeedle[lMatched]))
            {
                ++lMatched;
            }
            if (lMatched == InNeedle.GetLength()) { return true; }
        }
        return false;
    }

    // =============================================================================
    // LogFilter — which lines the Log panel shows. No ImGui, so it can be tested; the panel draws the controls.
    // =============================================================================
    struct LogFilter
    {
        TFixedArray<bool, static_cast<size_t>(ELogLevelFilter::Count)> ShowLevel{ true, true, true, true };

        /** Matched against the message, case-insensitively. Empty = every message. */
        OpaaxString Search;

        /**
         * Hidden categories (a set of hidden ones, so a new category is shown by default).
         */
        TUnorderedSet<OpaaxStringID> HiddenCategories;

        bool IsShown(const ELogLevelFilter InLevel) const noexcept { return ShowLevel[static_cast<size_t>(InLevel)]; }

        bool IsShown(const OpaaxStringID InCategory) const { return !HiddenCategories.contains(InCategory); }

        bool Passes(const LogEntry& InEntry) const
        {
            return IsShown(ToLevelFilter(InEntry.Level)) && IsShown(InEntry.Category)
                && ContainsNoCase(InEntry.Message, Search);
        }
    };
}
