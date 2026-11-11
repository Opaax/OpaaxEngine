#include "DevConsole/DevConsoleLineEdit.h"

#include "Core/String/OpaaxUtf8.h"

namespace Opaax
{
    namespace
    {
        OpaaxString Encode(const TDynArray<Uint32>& InChars, const Uint32 InCount)
        {
            OpaaxString lText;
            for (Uint32 lIndex = 0; lIndex < InCount; ++lIndex)
            {
                Utf8::Encode(InChars[lIndex], lText);
            }
            return lText;
        }
    }

    OpaaxString DevConsoleLineEdit::GetText() const
    {
        return Encode(m_Chars, GetLength());
    }

    OpaaxString DevConsoleLineEdit::GetTextBeforeCaret() const
    {
        return Encode(m_Chars, m_Caret);
    }

    void DevConsoleLineEdit::SetText(const OpaaxString& InText)
    {
        Clear();
        Insert(InText);
    }

    void DevConsoleLineEdit::Clear()
    {
        m_Chars.clear();
        m_Caret = 0;
    }

    void DevConsoleLineEdit::Insert(const OpaaxString& InText)
    {
        TDynArray<Uint32> lCodepoints;
        const char*       lCursor = InText.CStr();
        while (const Uint32 lCodepoint = Utf8::Decode(lCursor))
        {
            // One line: a pasted line break would end it.
            if (lCodepoint != '\n' && lCodepoint != '\r')
            {
                lCodepoints.push_back(lCodepoint);
            }
        }
        Insert(lCodepoints);
    }

    void DevConsoleLineEdit::Insert(const TDynArray<Uint32>& InCodepoints)
    {
        m_Chars.insert(m_Chars.begin() + m_Caret, InCodepoints.begin(), InCodepoints.end());
        m_Caret += static_cast<Uint32>(InCodepoints.size());
    }

    void DevConsoleLineEdit::MoveLeft() noexcept
    {
        if (m_Caret > 0)
        {
            --m_Caret;
        }
    }

    void DevConsoleLineEdit::MoveRight() noexcept
    {
        if (m_Caret < GetLength())
        {
            ++m_Caret;
        }
    }

    void DevConsoleLineEdit::MoveHome() noexcept
    {
        m_Caret = 0;
    }

    void DevConsoleLineEdit::MoveEnd() noexcept
    {
        m_Caret = GetLength();
    }

    bool DevConsoleLineEdit::Backspace()
    {
        if (m_Caret == 0)
        {
            return false;
        }
        m_Chars.erase(m_Chars.begin() + (m_Caret - 1));
        --m_Caret;
        return true;
    }

    bool DevConsoleLineEdit::Delete()
    {
        if (m_Caret >= GetLength())
        {
            return false;
        }
        m_Chars.erase(m_Chars.begin() + m_Caret);
        return true;
    }
}
