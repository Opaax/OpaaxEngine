#pragma once

#include <nlohmann/json.hpp>

#include "Automation/AutomationRunner.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "DevConsole/DevConsoleHistory.h"
#include "DevConsole/DevConsoleLineEdit.h"
#include "DevConsole/DevConsoleOutput.h"
#include "DevConsole/DevConsoleSettings.h"
#include "Input/InputCodes.h"

namespace Opaax
{
    class ICommandRegistry;
    class InputManager;

    OPAAX_LOG_CATEGORY(DevConsole);

    // =============================================================================
    // DevConsole — the console without its drawing: open or closed, the line being typed, the
    //   history, the output, and a runner of its own over the app's commands, so a line that waits
    //   answers when its wait ends without holding a script's queue. Fed a frame of input at a time
    //   (HandleInput): the toggle key, the text typed, the edit keys (Left, Right, Home, End,
    //   Backspace, Delete, Up, Down, Tab, Enter, Escape, Page Up, Page Down). DevConsoleSubsystem
    //   draws it and holds the game's input while it is open.
    // =============================================================================
    class DevConsole
    {
    public:
        /** Characters a row of Tab's matches holds, at most. */
        static constexpr Uint32 MATCHES_ROW_WIDTH = 96;

        DevConsole(ICommandRegistry& InCommands, const ConsoleSettings& InSettings);

        DevConsole(const DevConsole&)            = delete;
        DevConsole& operator=(const DevConsole&) = delete;

        // =============================================================================
        // Open
        // =============================================================================
    public:
        bool IsOpen() const noexcept { return m_bOpen; }
        void Open();
        void Close();

        // =============================================================================
        // Frame
        // =============================================================================
    public:
        /**
         * One frame of input. The toggle key opens or closes, and the text typed in that frame is
         * dropped (the key types its own character). Open, the text typed goes into the line, then
         * the edit keys apply, held ones repeating.
         */
        void HandleInput(const InputManager& InInput);

        /** Runs the lines entered: one that waits holds the console's queue until its wait ends. */
        void Tick();

        /** Runs InLine as Enter does: echoed, kept in the history, read, then run. */
        void Submit(const OpaaxString& InLine);

        /** Completes the word before the caret, as Tab does; several matches are listed. */
        void Complete();

        /** Adds a log line, coloured by its level. Not echoed: it is in the log already. */
        void AddLog(ELogLevel InLevel, const OpaaxString& InText);

        /**
         * Told each text the console prints, whole (an answer's JSON in one text): the echo of a line,
         * an answer, an error, the matches of Tab. The editor's Log panel logs them.
         */
        void SetEcho(TFunction<void(EDevConsoleRow, const OpaaxString&)> InEcho) { m_Echo = Move(InEcho); }

        // =============================================================================
        // State
        // =============================================================================
    public:
        const DevConsoleLineEdit& GetLine() const noexcept { return m_Line; }
        DevConsoleLineEdit&       GetLine()       noexcept { return m_Line; }

        const DevConsoleOutput& GetOutput() const noexcept { return m_Output; }
        DevConsoleOutput&       GetOutput()       noexcept { return m_Output; }

        const DevConsoleHistory& GetHistory() const noexcept { return m_History; }
        DevConsoleHistory&       GetHistory()       noexcept { return m_History; }

        /** Rows scrolled up from the newest (Page Up); 0 shows the newest. */
        Uint32 GetScroll() const noexcept { return m_Scroll; }

        /** Rows a Page Up moves: the rows the view shows. */
        void SetPageRows(Uint32 InRows) noexcept { m_PageRows = InRows > 0 ? InRows : 1; }

        /** True once after a line was added to the history: time to save it. */
        bool ConsumeHistoryChanged() noexcept;

        /** Whether a line entered is still running (a wait). */
        bool IsBusy() const noexcept { return !m_Runner.IsIdle(); }

        /** console.info's answer: open, the line and its caret, the scroll, and the last InRows rows. */
        nlohmann::json Info(Uint32 InRows) const;

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        void Toggle();
        void Respond(const AutomationResponse& InResponse);

        /** Adds InText to the output and tells the echo. */
        void Print(EDevConsoleRow InKind, const OpaaxString& InText);
        void ScrollBy(Int64 InRows);

        ICommandRegistry&  m_Commands;
        AutomationRunner   m_Runner;
        EKeyCode           m_ToggleKey;
        DevConsoleLineEdit m_Line;
        DevConsoleHistory  m_History;
        DevConsoleOutput   m_Output;

        TFunction<void(EDevConsoleRow, const OpaaxString&)> m_Echo;

        bool   m_bOpen           = false;
        bool   m_bHistoryChanged = false;
        Uint32 m_Scroll          = 0;
        Uint32 m_PageRows        = 10;
        Uint64 m_NextRequest     = 1;
    };
}
