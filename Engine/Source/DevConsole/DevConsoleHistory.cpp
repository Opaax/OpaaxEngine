#include "DevConsole/DevConsoleHistory.h"

#include "Core/Maths/Maths.h"
#include "Core/String/Char.h"

namespace Opaax
{
    namespace
    {
        /** InText without spaces at either end. */
        OpaaxString Trimmed(const OpaaxString& InText)
        {
            Uint32 lStart = 0;
            Uint32 lEnd   = InText.GetLength();
            while (lStart < lEnd && Char::IsSpace(InText[lStart]))
            {
                ++lStart;
            }
            while (lEnd > lStart && Char::IsSpace(InText[lEnd - 1]))
            {
                --lEnd;
            }
            return InText.SubString(lStart, lEnd - lStart);
        }
    }

    DevConsoleHistory::DevConsoleHistory(const Uint32 InCapacity)
        : m_Capacity(Maths::Max(InCapacity, 1u))
    {
    }

    void DevConsoleHistory::SetCapacity(const Uint32 InCapacity)
    {
        m_Capacity = Maths::Max(InCapacity, 1u);
        Trim();
    }

    void DevConsoleHistory::Add(const OpaaxString& InLine)
    {
        ResetWalk();

        const OpaaxString lLine = Trimmed(InLine);
        if (lLine.IsEmpty() || (!m_Lines.empty() && m_Lines.back() == lLine))
        {
            return;
        }
        m_Lines.push_back(lLine);
        Trim();
    }

    bool DevConsoleHistory::Up(const OpaaxString& InCurrent, OpaaxString& OutLine)
    {
        if (m_Lines.empty() || m_Walk == 0)
        {
            return false;
        }

        if (m_Walk < 0)
        {
            m_Draft = InCurrent;
            m_Walk  = static_cast<Int64>(m_Lines.size());
        }
        --m_Walk;
        OutLine = m_Lines[static_cast<Uint64>(m_Walk)];
        return true;
    }

    bool DevConsoleHistory::Down(OpaaxString& OutLine)
    {
        if (m_Walk < 0)
        {
            return false;
        }

        ++m_Walk;
        if (m_Walk >= static_cast<Int64>(m_Lines.size()))
        {
            OutLine = m_Draft;
            ResetWalk();
            return true;
        }
        OutLine = m_Lines[static_cast<Uint64>(m_Walk)];
        return true;
    }

    OpaaxString DevConsoleHistory::ToText() const
    {
        OpaaxString lText;
        for (const OpaaxString& lLine : m_Lines)
        {
            lText.Append(lLine);
            lText.Append('\n');
        }
        return lText;
    }

    void DevConsoleHistory::FromText(const OpaaxString& InText)
    {
        m_Lines.clear();
        ResetWalk();

        Uint32 lStart = 0;
        for (Uint32 lAt = 0; lAt <= InText.GetLength(); ++lAt)
        {
            if (lAt == InText.GetLength() || InText[lAt] == '\n')
            {
                const OpaaxString lLine = Trimmed(InText.SubString(lStart, lAt - lStart));
                if (!lLine.IsEmpty())
                {
                    m_Lines.push_back(lLine);
                }
                lStart = lAt + 1;
            }
        }
        Trim();
    }

    void DevConsoleHistory::Trim()
    {
        if (m_Lines.size() > m_Capacity)
        {
            m_Lines.erase(m_Lines.begin(), m_Lines.begin() + static_cast<Int64>(m_Lines.size() - m_Capacity));
            ResetWalk();
        }
    }
}
