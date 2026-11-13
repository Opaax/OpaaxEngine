#include "DevConsole/DevConsoleOutput.h"

#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    const char* ToString(const EDevConsoleRow InKind) noexcept
    {
        switch (InKind)
        {
        case EDevConsoleRow::Input:   return "input";
        case EDevConsoleRow::Answer:  return "answer";
        case EDevConsoleRow::Warning: return "warning";
        case EDevConsoleRow::Error:   return "error";
        case EDevConsoleRow::Info:    break;
        }
        return "info";
    }

    ELogLevel ToLogLevel(const EDevConsoleRow InKind) noexcept
    {
        return InKind == EDevConsoleRow::Warning || InKind == EDevConsoleRow::Error ? ELogLevel::Warn : ELogLevel::Info;
    }

    OpaaxString FormatCommandAnswer(const nlohmann::json& InValue, const int InIndent)
    {
        if (InValue.is_null() || (InValue.is_object() && InValue.empty()))
        {
            return OpaaxString();
        }
        if (InValue.is_string())
        {
            return InValue.get<OpaaxString>();
        }
        return ToJsonText(InValue, InIndent);
    }

    DevConsoleOutput::DevConsoleOutput(const Uint32 InMaxRows)
        : m_MaxRows(Maths::Max(InMaxRows, 1u))
    {
    }

    void DevConsoleOutput::SetMaxRows(const Uint32 InMaxRows)
    {
        m_MaxRows = Maths::Max(InMaxRows, 1u);
        while (m_Rows.size() > m_MaxRows)
        {
            m_Rows.pop_front();
        }
        ++m_Revision;
    }

    void DevConsoleOutput::Add(const EDevConsoleRow InKind, const OpaaxString& InText)
    {
        Uint32 lStart = 0;
        for (Uint32 lAt = 0; lAt <= InText.GetLength(); ++lAt)
        {
            if (lAt == InText.GetLength() || InText[lAt] == '\n')
            {
                // A CRLF line loses its CR; a tab is drawn as spaces.
                Uint32 lEnd = lAt;
                if (lEnd > lStart && InText[lEnd - 1] == '\r')
                {
                    --lEnd;
                }
                Push(InKind, InText.SubString(lStart, lEnd - lStart).Replace("\t", "    "));
                lStart = lAt + 1;
            }
        }
    }

    void DevConsoleOutput::AddAnswer(const nlohmann::json& InValue)
    {
        const OpaaxString lText = FormatCommandAnswer(InValue, JSON_INDENT);
        if (!lText.IsEmpty() || InValue.is_string())
        {
            Add(EDevConsoleRow::Answer, lText);
        }
    }

    void DevConsoleOutput::Clear()
    {
        m_Rows.clear();
        ++m_Revision;
    }

    void DevConsoleOutput::Push(const EDevConsoleRow InKind, OpaaxString InText)
    {
        m_Rows.push_back(DevConsoleRow{ InKind, Move(InText) });
        if (m_Rows.size() > m_MaxRows)
        {
            m_Rows.pop_front();
        }
        ++m_Revision;
    }
}
