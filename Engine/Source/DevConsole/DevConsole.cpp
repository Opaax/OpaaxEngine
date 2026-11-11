#include "DevConsole/DevConsole.h"

#include "Application/Services/ICommandRegistry.h"
#include "Commands/CommandText.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "DevConsole/DevConsoleCompletion.h"
#include "Input/InputManager.h"

namespace Opaax
{
    namespace
    {
        /** Pressed this frame, or repeated by the OS while held. */
        bool Typed(const InputManager& InInput, const EKeyCode InKey)
        {
            return InInput.WasPressedThisFrame(InKey) || InInput.WasRepeatedThisFrame(InKey);
        }
    }

    DevConsole::DevConsole(ICommandRegistry& InCommands, const ConsoleSettings& InSettings)
        : m_Commands(InCommands)
        , m_Runner(InCommands)
        , m_ToggleKey(InSettings.ToggleKey)
        , m_History(InSettings.HistorySize)
        , m_Output(InSettings.MaxLines)
    {
        m_Runner.SetLogResponses(false);
        m_Runner.SetResponseSink([this](const AutomationResponse& InResponse) { Respond(InResponse); });
    }

    // =========================================================================
    // Open
    // =========================================================================
    void DevConsole::Open()
    {
        m_bOpen = true;
    }

    void DevConsole::Close()
    {
        m_bOpen = false;
        m_History.ResetWalk();
    }

    void DevConsole::Toggle()
    {
        if (m_bOpen)
        {
            Close();
        }
        else
        {
            Open();
        }
    }

    // =========================================================================
    // Frame
    // =========================================================================
    void DevConsole::HandleInput(const InputManager& InInput)
    {
        if (m_ToggleKey != EKeyCode::None && InInput.WasPressedThisFrame(m_ToggleKey))
        {
            Toggle();
            return;
        }

        if (!m_bOpen)
        {
            return;
        }

        if (InInput.WasPressedThisFrame(EKeyCode::Escape))
        {
            Close();
            return;
        }

        if (!InInput.GetTypedText().empty())
        {
            m_Line.Insert(InInput.GetTypedText());
            m_History.ResetWalk();
        }

        if (Typed(InInput, EKeyCode::Left))      { m_Line.MoveLeft(); }
        if (Typed(InInput, EKeyCode::Right))     { m_Line.MoveRight(); }
        if (Typed(InInput, EKeyCode::Home))      { m_Line.MoveHome(); }
        if (Typed(InInput, EKeyCode::End))       { m_Line.MoveEnd(); }
        if (Typed(InInput, EKeyCode::Backspace)) { m_Line.Backspace(); }
        if (Typed(InInput, EKeyCode::Delete))    { m_Line.Delete(); }
        if (Typed(InInput, EKeyCode::PageUp))    { ScrollBy(static_cast<Int64>(m_PageRows)); }
        if (Typed(InInput, EKeyCode::PageDown))  { ScrollBy(-static_cast<Int64>(m_PageRows)); }

        if (Typed(InInput, EKeyCode::Up))
        {
            OpaaxString lLine;
            if (m_History.Up(m_Line.GetText(), lLine))
            {
                m_Line.SetText(lLine);
            }
        }
        if (Typed(InInput, EKeyCode::Down))
        {
            OpaaxString lLine;
            if (m_History.Down(lLine))
            {
                m_Line.SetText(lLine);
            }
        }

        if (Typed(InInput, EKeyCode::Tab))
        {
            Complete();
        }

        if (InInput.WasPressedThisFrame(EKeyCode::Enter) || InInput.WasPressedThisFrame(EKeyCode::Numpad_Enter))
        {
            const OpaaxString lLine = m_Line.GetText();
            m_Line.Clear();
            Submit(lLine);
        }
    }

    void DevConsole::Tick()
    {
        m_Runner.Tick();
    }

    void DevConsole::Submit(const OpaaxString& InLine)
    {
        const OpaaxString lLine(InLine.View().Trim());
        m_Scroll = 0;
        if (lLine.IsEmpty())
        {
            return;
        }

        Print(EDevConsoleRow::Input, "> " + lLine);
        m_History.Add(lLine);
        m_bHistoryChanged = true;

        const CommandTextResult lRead = ParseCommandText(lLine, m_Commands);
        if (!lRead.bOk)
        {
            Print(EDevConsoleRow::Error, lRead.Error);
            return;
        }

        AutomationRequest lRequest;
        lRequest.Id      = OpaaxString::Format("console-{}", m_NextRequest++);
        lRequest.Command = lRead.Command;
        lRequest.Params  = lRead.Params;
        m_Runner.Enqueue(Move(lRequest));
    }

    void DevConsole::Complete()
    {
        const OpaaxString          lBefore = m_Line.GetTextBeforeCaret();
        const DevConsoleCompletion lDone   = CompleteCommandText(lBefore, m_Commands.GetSpecs());

        if (lDone.Line != lBefore)
        {
            // The completed part replaces the text before the caret; the caret stays after it.
            const OpaaxString lAfter = m_Line.GetText().SubString(lBefore.GetLength());
            m_Line.SetText(lDone.Line);
            const Uint32 lCaret = m_Line.GetCaret();
            m_Line.Insert(lAfter);
            m_Line.SetCaret(lCaret);
        }

        if (lDone.Matches.empty())
        {
            return;
        }

        // Listed several to a row.
        OpaaxString lRow;
        for (const OpaaxString& lMatch : lDone.Matches)
        {
            if (!lRow.IsEmpty() && lRow.GetLength() + 2 + lMatch.GetLength() > MATCHES_ROW_WIDTH)
            {
                Print(EDevConsoleRow::Info, lRow);
                lRow = OpaaxString();
            }
            lRow += lRow.IsEmpty() ? lMatch : "  " + lMatch;
        }
        Print(EDevConsoleRow::Info, lRow);
    }

    void DevConsole::AddLog(const ELogLevel InLevel, const OpaaxString& InText)
    {
        const EDevConsoleRow lKind = InLevel >= ELogLevel::Error ? EDevConsoleRow::Error
                                   : InLevel == ELogLevel::Warn  ? EDevConsoleRow::Warning
                                                                 : EDevConsoleRow::Info;
        m_Output.Add(lKind, InText);
    }

    // =========================================================================
    // State
    // =========================================================================
    bool DevConsole::ConsumeHistoryChanged() noexcept
    {
        const bool bChanged = m_bHistoryChanged;
        m_bHistoryChanged   = false;
        return bChanged;
    }

    nlohmann::json DevConsole::Info(const Uint32 InRows) const
    {
        nlohmann::json lRows = nlohmann::json::array();

        const auto&  lAll   = m_Output.GetRows();
        const Uint64 lFirst = lAll.size() > InRows ? lAll.size() - InRows : 0;
        for (Uint64 lIndex = lFirst; lIndex < lAll.size(); ++lIndex)
        {
            lRows.push_back({ { "kind", ToString(lAll[lIndex].Kind) }, { "text", lAll[lIndex].Text } });
        }

        return {
            { "open",    m_bOpen },
            { "line",    m_Line.GetText() },
            { "caret",   m_Line.GetCaret() },
            { "scroll",  m_Scroll },
            { "busy",    IsBusy() },
            { "history", m_History.GetLines().size() },
            { "rows",    Move(lRows) }
        };
    }

    // =========================================================================
    // Internal
    // =========================================================================
    void DevConsole::Respond(const AutomationResponse& InResponse)
    {
        if (!InResponse.bOk)
        {
            Print(EDevConsoleRow::Error, InResponse.Error);
            return;
        }

        const OpaaxString lText = FormatCommandAnswer(InResponse.Result, DevConsoleOutput::JSON_INDENT);
        if (!lText.IsEmpty() || InResponse.Result.is_string())
        {
            Print(EDevConsoleRow::Answer, lText);
        }
    }

    void DevConsole::Print(const EDevConsoleRow InKind, const OpaaxString& InText)
    {
        m_Output.Add(InKind, InText);
        if (m_Echo)
        {
            m_Echo(InKind, InText);
        }
    }

    void DevConsole::ScrollBy(const Int64 InRows)
    {
        const Int64 lMost = Maths::Max<Int64>(static_cast<Int64>(m_Output.GetRows().size()) - 1, 0);
        m_Scroll = static_cast<Uint32>(Maths::Clamp<Int64>(static_cast<Int64>(m_Scroll) + InRows, 0, lMost));
    }
}
