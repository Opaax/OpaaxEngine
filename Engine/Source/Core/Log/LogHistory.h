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

    /** One log line: level, category and text. */
    struct LogEntry
    {
        /** Starts at 1, +1 per line. A reader resumes from the last one it saw. */
        Uint64                                Sequence = 0;
        std::chrono::system_clock::time_point Time;
        ELogLevel                             Level{};

        /** Interned: the category name lives in the module that declared it. */
        OpaaxStringID                         Category;

        /** Without the "[Category] " prefix. */
        OpaaxString                           Message;
    };

    // =============================================================================
    // LogHistory — the last N log lines, for the editor's Log panel. Capacity 0 = off.
    //   Not thread-safe; the Logger guards it.
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
        /** Drops the oldest line when full. Does nothing when capacity is 0. */
        void Push(ELogLevel InLevel, OpaaxStringID InCategory, std::string_view InMessage);

        /**
         * Appends every line newer than InAfterSequence to OutEntries, oldest first.
         * @return The newest sequence, to pass next time
         */
        Uint64 CopySince(Uint64 InAfterSequence, TDynArray<LogEntry>& OutEntries) const;

        /** Shrinking drops the oldest lines; 0 clears and turns the history off. */
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
