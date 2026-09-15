#include "UI/UIBinding.h"

#include <cstdio>   // std::snprintf — the shortest float spelling

namespace Opaax
{
    OpaaxString UIBoundValue::ToText() const
    {
        switch (Kind)
        {
            case EKind::Bool:    return OpaaxString(Number != 0.0 ? "true" : "false");
            case EKind::Integer: return OpaaxString::FromInt(static_cast<Int64>(Number));
            case EKind::Number:
            {
                char lBuffer[32];
                const int lWritten = std::snprintf(lBuffer, sizeof(lBuffer), "%g", Number);
                return lWritten > 0 ? OpaaxString(lBuffer) : OpaaxString();
            }
            case EKind::Text:    return Text;
            case EKind::None:    break;
        }
        return OpaaxString();
    }

    // =============================================================================
    // UIBindingTable
    // =============================================================================

    void UIBindingTable::Add(const OpaaxStringID InName, UIBindingReader InReader)
    {
        for (Uint64 lIndex = 0; lIndex < m_Names.size(); ++lIndex)
        {
            if (m_Names[lIndex] == InName)
            {
                m_Readers[lIndex] = Move(InReader);
                return;
            }
        }

        m_Names.emplace_back(InName);
        m_Readers.emplace_back(Move(InReader));
    }

    void UIBindingTable::Remove(const OpaaxStringID InName)
    {
        for (Uint64 lIndex = 0; lIndex < m_Names.size(); ++lIndex)
        {
            if (m_Names[lIndex] == InName)
            {
                m_Names.erase(m_Names.begin() + static_cast<std::ptrdiff_t>(lIndex));
                m_Readers.erase(m_Readers.begin() + static_cast<std::ptrdiff_t>(lIndex));
                return;
            }
        }
    }

    bool UIBindingTable::Has(const OpaaxStringID InName) const noexcept
    {
        for (const OpaaxStringID& lName : m_Names)
        {
            if (lName == InName) { return true; }
        }
        return false;
    }

    bool UIBindingTable::Read(const OpaaxString& InPath, UIBoundValue& OutValue)
    {
        const Int32 lDot = InPath.Find(".");

        const OpaaxStringID lSource   = OpaaxStringID(lDot > 0 ? InPath.SubString(0, static_cast<Uint32>(lDot)) : OpaaxString());
        const OpaaxString   lProperty = lDot > 0 ? InPath.SubString(static_cast<Uint32>(lDot) + 1) : OpaaxString();

        bool lOk = false;
        for (Uint64 lIndex = 0; lIndex < m_Names.size(); ++lIndex)
        {
            if (m_Names[lIndex] == lSource)
            {
                lOk = !lProperty.IsEmpty() && m_Readers[lIndex](lProperty.CStr(), OutValue);
                break;
            }
        }

        if (!lOk)
        {
            for (const OpaaxString& lWarned : m_Warned)
            {
                if (lWarned == InPath) { return false; }
            }
            m_Warned.emplace_back(InPath);

            OPAAX_LOG(LogUIBinding, Warn, "Binding '{}' resolves to nothing — {}; the widget keeps its authored value.",
                      InPath.CStr(), Has(lSource) ? "no readable property of that name" : "no source of that name");
        }

        return lOk;
    }

    OpaaxString FormatBoundText(const OpaaxString& InFormat, const OpaaxString& InValue)
    {
        const Int32 lAt = InFormat.Find("{}");
        if (lAt < 0)
        {
            return InValue;
        }

        return InFormat.SubString(0, static_cast<Uint32>(lAt)) + InValue + InFormat.SubString(static_cast<Uint32>(lAt) + 2);
    }
}
