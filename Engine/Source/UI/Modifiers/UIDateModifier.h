#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Core/Time/DateTime.h"
#include "UI/UIModifier.h"

namespace Opaax
{
    // =============================================================================
    // UIDateModifier — today's date in its text widget, written along Format ({date} {month} {year}).
    //   The date fills the first {} left in the widget's text, as a Binding's value does, or replaces
    //   the text when there is none: "Today: {}" shows "Today: 24 November 2026". The date is read from
    //   the canvas's DateSource (a pinned one in tests), the wall clock without one. It stays awake and
    //   rebuilds its widget only when the text it gives changes: at midnight, or each second when
    //   Format shows the time.
    // =============================================================================
    class UIDateModifier final : public UIModifier
    {
        OPAAX_OBJECT(UIDateModifier, UIModifier)

    public:
        OpaaxString Format = "{date} {month} {year}";

        OPAAX_PROPERTIES(UIDateModifier,
                         OPAAX_PROP(Format).SetTooltip(
                             "The date, written with tokens: {date} 4, {dd} 04, {month} November, {mon} Nov, "
                             "{MM} 11, {year} 2026, {yy} 26, {weekday} Tuesday, {wd} Tue, {hh} {mm} {ss} 09 05 07. "
                             "{{ and }} write a brace; an unknown token shows as written."))

        UIDateModifier& SetFormat(const OpaaxString& InFormat);

        //~Begin Object interface
        /** An edited Format shows at once, the editor's preview too. */
        void PostEditChange() override;
        //~End Object interface

    protected:
        //~Begin UIModifier interface
        void OnAttach() override;
        void OnDetach() override;
        void OnShow() override;
        bool Tick(float InSeconds) override;
        void ModifyText(OpaaxString& InOutText) const override;
        //~End UIModifier interface

    private:
        /** The date my owner's canvas reads, else the wall clock. */
        DateTime ReadNow() const;

        /**
         * Reads the date; when it moved at Format's resolution (or bInForce), writes it again, and rebuilds
         * my owner only if the text changed (ModifyText then writes it). Warns once per Format holding an
         * unknown token.
         */
        void Refresh(bool bInForce);

        OpaaxString m_Shown;          // the text the last Refresh gave: what a change is told by
        DateTime    m_Seen;           // the date it was written from
        OpaaxString m_UsedFormat;     // the Format it was written with
        OpaaxString m_WarnedFormat;   // the last Format warned about
        bool        m_bShowsTime = false;
        bool        m_bRefreshed = false;
    };

    /** Whether InPattern writes the time of day (hh, mm or ss): a date written with it changes each second. Pure. */
    bool DateFormatShowsTime(OpaaxStringView InPattern);
}
