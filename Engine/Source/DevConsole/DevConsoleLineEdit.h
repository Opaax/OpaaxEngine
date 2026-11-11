#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // DevConsoleLineEdit — the console's line being typed, and its caret, on Unicode codepoints: a
    //   character of two bytes moves and deletes as one. Pure: the console feeds it the typed text
    //   and the keys, and draws GetText with the caret after GetTextBeforeCaret.
    // =============================================================================
    class DevConsoleLineEdit
    {
    public:
        /** The line, as UTF-8. */
        OpaaxString GetText() const;

        /** The line up to the caret, as UTF-8: where the caret is drawn. */
        OpaaxString GetTextBeforeCaret() const;

        /** The caret, in characters from the start (0 to GetLength). */
        Uint32 GetCaret() const noexcept { return m_Caret; }

        /** Characters in the line. */
        Uint32 GetLength() const noexcept { return static_cast<Uint32>(m_Chars.size()); }

        bool IsEmpty() const noexcept { return m_Chars.empty(); }

        /** Replaces the line (a history entry, a completion); the caret goes to its end. */
        void SetText(const OpaaxString& InText);

        /** Empties the line. */
        void Clear();

        /** Inserts UTF-8 text at the caret, which moves past it. */
        void Insert(const OpaaxString& InText);

        /** Inserts characters at the caret, which moves past them. */
        void Insert(const TDynArray<Uint32>& InCodepoints);

        /** Puts the caret before character InCaret (past the end: at the end). */
        void SetCaret(Uint32 InCaret) noexcept { m_Caret = InCaret < GetLength() ? InCaret : GetLength(); }

        void MoveLeft() noexcept;
        void MoveRight() noexcept;
        void MoveHome() noexcept;
        void MoveEnd() noexcept;

        /** Deletes the character before the caret. False when there is none. */
        bool Backspace();

        /** Deletes the character after the caret. False when there is none. */
        bool Delete();

    private:
        TDynArray<Uint32> m_Chars;
        Uint32            m_Caret = 0;
    };
}
