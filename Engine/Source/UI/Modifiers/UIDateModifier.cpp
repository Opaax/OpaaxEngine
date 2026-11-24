#include "UI/Modifiers/UIDateModifier.h"

#include "Core/Log/Logger.h"
#include "Core/Time/DateSource.h"
#include "UI/UIBinding.h"
#include "UI/UICanvas.h"
#include "UI/UIWidget.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(UIDate);

    namespace
    {
        /** Same day, and same second when InbSeconds. */
        bool SameAt(const DateTime& InA, const DateTime& InB, const bool bInSeconds) noexcept
        {
            const bool lSameDay = InA.Year == InB.Year && InA.Month == InB.Month && InA.Day == InB.Day;
            if (!bInSeconds)
            {
                return lSameDay;
            }
            return lSameDay && InA.Hour == InB.Hour && InA.Minute == InB.Minute && InA.Second == InB.Second;
        }
    }

    bool DateFormatShowsTime(const OpaaxStringView InPattern)
    {
        // The pattern written at the first and the last second of one day: only a time token tells them apart.
        DateTime lMorning;
        DateTime lNight;
        lNight.Hour   = 23;
        lNight.Minute = 59;
        lNight.Second = 59;
        return lMorning.Format(InPattern) != lNight.Format(InPattern);
    }

    UIDateModifier& UIDateModifier::SetFormat(const OpaaxString& InFormat)
    {
        Format = InFormat;
        Refresh(true);
        return *this;
    }

    void UIDateModifier::PostEditChange()
    {
        Refresh(true);
        InvalidateOwnerContent();   // a toggled bEnabled changes the text too
    }

    void UIDateModifier::OnAttach()
    {
        Refresh(true);
        InvalidateOwnerContent();
        WakeUp();
    }

    void UIDateModifier::OnDetach()
    {
        InvalidateOwnerContent();
    }

    void UIDateModifier::OnShow()
    {
        Refresh(true);
        WakeUp();
    }

    bool UIDateModifier::Tick(const float InSeconds)
    {
        (void)InSeconds;
        Refresh(false);
        return true;   // a date moves on by itself: stay awake
    }

    void UIDateModifier::ModifyText(OpaaxString& InOutText) const
    {
        // Read again here: a tree rebuilt outside its canvas (the editor's undo) shows the canvas's date
        // once it is in.
        InOutText = FormatBoundText(InOutText, ReadNow().Format(Format));
    }

    DateTime UIDateModifier::ReadNow() const
    {
        const UIWidget* const   lOwner  = GetOwner();
        const UICanvas* const   lCanvas = lOwner != nullptr ? lOwner->GetCanvas() : nullptr;
        const DateSource* const lDates  = lCanvas != nullptr ? lCanvas->GetDates() : nullptr;
        return lDates != nullptr ? lDates->Now() : DateTime::Now();
    }

    void UIDateModifier::Refresh(const bool bInForce)
    {
        const DateTime lNow       = ReadNow();
        const bool     lNewFormat = !m_bRefreshed || Format != m_UsedFormat;

        if (!bInForce && !lNewFormat && SameAt(lNow, m_Seen, m_bShowsTime))
        {
            return;   // nothing Format shows has moved: no text to write
        }

        if (lNewFormat)
        {
            m_UsedFormat = Format;
            m_bShowsTime = DateFormatShowsTime(Format);
        }
        m_Seen       = lNow;
        m_bRefreshed = true;

        OpaaxString lUnknown;
        OpaaxString lShown = lNow.Format(Format, &lUnknown);
        if (!lUnknown.IsEmpty() && Format != m_WarnedFormat)
        {
            m_WarnedFormat = Format;
            OPAAX_LOG(LogUIDate, Warn, "Date format \"{}\" has an unknown token {}: it shows as written "
                      "(the tokens are in the Format tooltip)", Format.CStr(), lUnknown.CStr());
        }

        if (lShown != m_Shown)
        {
            m_Shown = Move(lShown);
            InvalidateOwnerContent();
        }
    }
}
