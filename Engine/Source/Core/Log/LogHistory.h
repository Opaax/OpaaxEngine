#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"

#include <chrono>
#include <deque>
#include <string_view>

namespace Opaax
{
    enum class ELogLevel : Uint8;

    /** One line as the call site produced it — the level and category kept apart from the text. */
    struct LogEntry
    {
        /** 1 for the first line ever pushed, then +1 per line. A reader resumes from the last it saw. */
        Uint64                                Sequence = 0;
        std::chrono::system_clock::time_point Time;
        ELogLevel                             Level{};

        /** Interned, not a const char*: the name lives in the module that declared the category. */
        OpaaxStringID                         Category;

        /** Without the "[Category] " prefix the sinks print. */
        OpaaxString                           Message;
    };

    // =============================================================================
    // LogHistory — the last N lines, for a reader that wants them STRUCTURED (the editor's Log
    //   panel). A capacity of 0 is off: Push returns before copying anything.
    //
    //   Not thread-safe on its own; the Logger owns one under its mutex. A reader never iterates it —
    //   it copies the lines it has not seen yet (CopySince) and draws its own copy.
    // =============================================================================
    class OPAAX_API LogHistory final
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit LogHistory(Uint32 InCapacity = 0);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Oldest line dropped first once full. A no-op while the capacity is 0. */
        void Push(ELogLevel InLevel, OpaaxStringID InCategory, std::string_view InMessage);

        /**
         * Appends to OutEntries every held line newer than InAfterSequence, oldest first. A reader that
         * fell behind by more than the capacity gets what is still held.
         *
         * @return The newest sequence pushed so far — what the reader passes next time.
         */
        Uint64 CopySince(Uint64 InAfterSequence, TDynArray<LogEntry>& OutEntries) const;

        /** Shrinking drops the oldest lines; 0 drops them all and turns the history off. */
        void SetCapacity(Uint32 InCapacity);

        // =============================================================================
        // Getters
        // =============================================================================
    public:
        Uint32 GetCapacity() const noexcept { return m_Capacity; }
        Uint32 GetCount() const noexcept { return static_cast<Uint32>(m_Entries.size()); }
        bool   IsEnabled() const noexcept { return m_Capacity > 0; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        std::deque<LogEntry> m_Entries;
        Uint32               m_Capacity     = 0;
        Uint64               m_LastSequence = 0;
    };
}
