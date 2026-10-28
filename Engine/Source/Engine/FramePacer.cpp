#include "Engine/FramePacer.h"

#include "Core/Maths/Maths.h"
#include "Core/String/CString.h"

namespace Opaax
{
    namespace
    {
        /** The highest rate a pacing takes, frames a second. */
        constexpr double MAX_RATE = 10000.0;

        /** The longest frame a list of deltas takes, seconds. */
        constexpr double MAX_LISTED_DELTA = 1.0;

        /** What a key=value pair can name, for the errors. */
        constexpr const char* KNOWN_KEYS = "rate, jitter, hitch, seed and deltas";

        /** The whole of InText as a decimal number. */
        bool ReadNumber(const OpaaxStringView InText, double& OutValue)
        {
            const OpaaxString lText(InText.Trim());   // terminated, for ToDouble
            const char*       lEnd = nullptr;

            OutValue = CString::ToDouble(lText.CStr(), &lEnd);
            return !lText.IsEmpty() && lEnd == lText.CStr() + lText.GetLength();
        }

        /** The whole of InText as a whole number. */
        bool ReadWhole(const OpaaxStringView InText, Uint64& OutValue)
        {
            const OpaaxString lText(InText.Trim());
            const char*       lEnd = nullptr;

            OutValue = CString::ToUint64(lText.CStr(), &lEnd);
            return !lText.IsEmpty() && lText.CStr()[0] != '-' && lEnd == lText.CStr() + lText.GetLength();
        }

        /** One key=value of a pacing into InOutPacing. False with OutError when it is not one. */
        bool ReadPair(const OpaaxStringView InKey, const OpaaxStringView InValue, FramePacing& InOutPacing,
                      OpaaxString& OutError)
        {
            if (InKey == "rate")
            {
                if (!ReadNumber(InValue, InOutPacing.Rate) || !(InOutPacing.Rate > 0.0) || InOutPacing.Rate > MAX_RATE)
                {
                    OutError = "the rate is frames a second, above 0 and at most 10000: '"
                               + OpaaxString(InValue) + "'";
                    return false;
                }
                return true;
            }

            if (InKey == "jitter")
            {
                if (!ReadNumber(InValue, InOutPacing.Jitter) || !(InOutPacing.Jitter >= 0.0)
                    || InOutPacing.Jitter > FramePacing::MAX_JITTER)
                {
                    OutError = "the jitter is a fraction of a frame, from 0 to 0.45: '" + OpaaxString(InValue) + "'";
                    return false;
                }
                return true;
            }

            if (InKey == "hitch")
            {
                const Int32 lAt = InValue.Find('@');
                if (lAt < 0 || !ReadNumber(InValue.SubString(0, static_cast<Uint32>(lAt)), InOutPacing.HitchSeconds)
                    || !ReadNumber(InValue.SubString(static_cast<Uint32>(lAt) + 1), InOutPacing.HitchEvery)
                    || !(InOutPacing.HitchSeconds > 0.0) || !(InOutPacing.HitchEvery > 0.0))
                {
                    OutError = "a hitch is written <stall seconds>@<every seconds>, like 0.1@2: '"
                               + OpaaxString(InValue) + "'";
                    return false;
                }
                return true;
            }

            if (InKey == "seed")
            {
                if (!ReadWhole(InValue, InOutPacing.Seed))
                {
                    OutError = "the seed is a whole number: '" + OpaaxString(InValue) + "'";
                    return false;
                }
                return true;
            }

            if (InKey == "deltas")
            {
                OpaaxStringView lRest = InValue.Trim();
                while (!lRest.IsEmpty())
                {
                    const Int32           lSlash = lRest.Find('/');
                    const OpaaxStringView lItem  = lSlash < 0 ? lRest : lRest.SubString(0, static_cast<Uint32>(lSlash));
                    lRest = lSlash < 0 ? OpaaxStringView() : lRest.SubString(static_cast<Uint32>(lSlash) + 1);

                    double lDelta = 0.0;
                    if (!ReadNumber(lItem, lDelta) || !(lDelta > 0.0) || lDelta > MAX_LISTED_DELTA)
                    {
                        OutError = "deltas are seconds above 0 and at most 1, split by '/', like 0.016/0.033: '"
                                   + OpaaxString(InValue) + "'";
                        return false;
                    }
                    InOutPacing.Deltas.push_back(lDelta);
                }

                if (InOutPacing.Deltas.empty())
                {
                    OutError = "deltas need at least one, like deltas=0.016/0.033";
                    return false;
                }
                return true;
            }

            OutError = "unknown '" + OpaaxString(InKey) + "': a pacing takes " + KNOWN_KEYS;
            return false;
        }
    }

    // =========================================================================
    // FramePacing
    // =========================================================================
    bool FramePacing::Parse(const OpaaxStringView InText, FramePacing& OutPacing, OpaaxString& OutError)
    {
        FramePacing     lPacing;
        OpaaxStringView lRest = InText.Trim();

        if (lRest.IsEmpty() || lRest == "off")
        {
            OutPacing = lPacing;
            return true;
        }

        // A first part without '=' is the rate: "144,jitter=0.25".
        bool lFirst = true;
        while (!lRest.IsEmpty())
        {
            const Int32           lComma = lRest.Find(',');
            const OpaaxStringView lPart  = (lComma < 0 ? lRest : lRest.SubString(0, static_cast<Uint32>(lComma))).Trim();
            lRest = lComma < 0 ? OpaaxStringView() : lRest.SubString(static_cast<Uint32>(lComma) + 1);

            const Int32 lEquals = lPart.Find('=');
            if (lEquals < 0 && !lFirst)
            {
                OutError = "'" + OpaaxString(lPart) + "' has no value: write key=value (" + KNOWN_KEYS + ")";
                return false;
            }

            const OpaaxStringView lKey   = lEquals < 0 ? OpaaxStringView("rate")
                                                       : lPart.SubString(0, static_cast<Uint32>(lEquals)).Trim();
            const OpaaxStringView lValue = lEquals < 0 ? lPart : lPart.SubString(static_cast<Uint32>(lEquals) + 1);

            if (!ReadPair(lKey, lValue, lPacing, OutError))
            {
                return false;
            }
            lFirst = false;
        }

        if (lPacing.Rate > 0.0 && !lPacing.Deltas.empty())
        {
            OutError = "a pacing has a rate or deltas, not both";
            return false;
        }

        if (lPacing.Rate <= 0.0 && (lPacing.Jitter > 0.0 || lPacing.HitchEvery > 0.0))
        {
            OutError = "a jitter or a hitch needs a rate, like 144,jitter=0.25";
            return false;
        }

        OutPacing = Move(lPacing);
        return true;
    }

    OpaaxString FramePacing::ToString() const
    {
        if (!IsActive())
        {
            return OpaaxString("off");
        }

        OpaaxString lText;
        if (!Deltas.empty())
        {
            lText = "deltas=";
            for (Uint64 lIndex = 0; lIndex < Deltas.size(); ++lIndex)
            {
                lText += lIndex > 0 ? OpaaxString::Format("/{}", Deltas[lIndex]) : OpaaxString::Format("{}", Deltas[lIndex]);
            }
        }
        else
        {
            lText = OpaaxString::Format("{}", Rate);
            if (Jitter > 0.0)
            {
                lText += OpaaxString::Format(",jitter={}", Jitter);
            }
            if (HitchEvery > 0.0 && HitchSeconds > 0.0)
            {
                lText += OpaaxString::Format(",hitch={}@{}", HitchSeconds, HitchEvery);
            }
        }

        if (Seed != 1)
        {
            lText += OpaaxString::Format(",seed={}", Seed);
        }
        return lText;
    }

    // =========================================================================
    // FramePacer
    // =========================================================================
    FramePacer::FramePacer(const FramePacing& InPacing)
        : m_Pacing(InPacing)
        , m_Random(InPacing.Seed)
        , m_NextHitch(InPacing.HitchEvery)
    {
    }

    double FramePacer::Next()
    {
        if (!m_Pacing.Deltas.empty())
        {
            const double lDelta = m_Pacing.Deltas[m_Frame % m_Pacing.Deltas.size()];
            ++m_Frame;
            return lDelta;
        }

        if (m_Pacing.Rate <= 0.0)
        {
            return 0.0;
        }

        const double lTick   = 1.0 / m_Pacing.Rate;
        const double lJitter = Maths::Clamp(m_Pacing.Jitter, 0.0, FramePacing::MAX_JITTER);

        // This frame's distance from its tick: the delta is a tick plus the change of distance, so the
        // frames never drift off the rate.
        const double lOffset = lJitter > 0.0 ? lTick * lJitter * static_cast<double>(m_Random.Range(-1.f, 1.f)) : 0.0;
        double       lDelta  = lTick + lOffset - m_Offset;

        m_Offset   = lOffset;
        m_Elapsed += lTick;
        ++m_Frame;

        if (m_Pacing.HitchSeconds > 0.0 && m_Pacing.HitchEvery > 0.0 && m_Elapsed >= m_NextHitch)
        {
            lDelta      += m_Pacing.HitchSeconds;
            m_NextHitch += m_Pacing.HitchEvery;
        }

        return lDelta;
    }
}
