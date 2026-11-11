#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // DevConsoleHistory — the lines run in the console, oldest first, the oldest dropped past the
    //   cap; Up and Down walk them as a shell does, and the line being typed is kept as a draft
    //   that Down past the newest gives back. Saved as text, one line each. Pure.
    // =============================================================================
    class DevConsoleHistory
    {
    public:
        static constexpr Uint32 DEFAULT_CAPACITY = 100;

        explicit DevConsoleHistory(Uint32 InCapacity = DEFAULT_CAPACITY);

        /** The most lines kept (at least 1); fewer are dropped, the oldest first. */
        void SetCapacity(Uint32 InCapacity);

        Uint32 GetCapacity() const noexcept { return m_Capacity; }

        /**
         * A line was run: kept, unless empty or the same as the newest. Ends a walk through the
         * history.
         */
        void Add(const OpaaxString& InLine);

        /**
         * The line before the one shown. The first Up keeps InCurrent (the line being typed) as the
         * draft. False, OutLine untouched, at the oldest or with no history.
         */
        bool Up(const OpaaxString& InCurrent, OpaaxString& OutLine);

        /** The line after the one shown; past the newest, the draft. False when not walking. */
        bool Down(OpaaxString& OutLine);

        /** Stops walking: the next Up starts from the newest again. */
        void ResetWalk() noexcept { m_Walk = -1; }

        const TDynArray<OpaaxString>& GetLines() const noexcept { return m_Lines; }

        /** The lines as text, one each, oldest first. */
        OpaaxString ToText() const;

        /** Replaces the lines with InText's, one each (blank lines skipped), keeping the newest past the cap. */
        void FromText(const OpaaxString& InText);

    private:
        void Trim();

        TDynArray<OpaaxString> m_Lines;
        Uint32                 m_Capacity;

        /** The line shown while walking, or -1 when not walking. */
        Int64       m_Walk = -1;
        OpaaxString m_Draft;
    };
}
