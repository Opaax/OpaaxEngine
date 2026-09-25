#include "LogHistory.h"

#include "Core/Log/Logger.h"   // ELogLevel's definition

namespace Opaax
{
    LogHistory::LogHistory(const Uint32 InCapacity)
        : m_Capacity(InCapacity)
    {
    }

    void LogHistory::Push(const ELogLevel InLevel, const OpaaxStringID InCategory, const std::string_view InMessage)
    {
        if (m_Capacity == 0) { return; }

        if (m_Entries.size() >= m_Capacity)
        {
            m_Entries.pop_front();
        }

        m_Entries.push_back({ ++m_LastSequence, std::chrono::system_clock::now(), InLevel, InCategory,
                              OpaaxString(InMessage) });
    }

    Uint64 LogHistory::CopySince(const Uint64 InAfterSequence, TDynArray<LogEntry>& OutEntries) const
    {
        // Sequences are contiguous, so the first unseen line is found by arithmetic, not a search.
        const Uint64 lFirstHeld = m_Entries.empty() ? 0 : m_Entries.front().Sequence;
        const Uint64 lSkip      = InAfterSequence >= lFirstHeld ? InAfterSequence - lFirstHeld + 1 : 0;

        if (lSkip < m_Entries.size())
        {
            OutEntries.insert(OutEntries.end(), m_Entries.begin() + static_cast<std::ptrdiff_t>(lSkip), m_Entries.end());
        }

        return m_LastSequence;
    }

    void LogHistory::SetCapacity(const Uint32 InCapacity)
    {
        m_Capacity = InCapacity;

        while (m_Entries.size() > m_Capacity)
        {
            m_Entries.pop_front();
        }
    }
}
