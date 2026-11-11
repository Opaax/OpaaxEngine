#pragma once

#include "Automation/AutomationRunner.h"
#include "Automation/AutomationTransports.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // AutomationSession — the app driven from outside, as its command line asks:
    //   --exec <script.json> [--exec-out <answers.json>] runs a script's requests, then closes the
    //     app; its exit code is 1 when a request failed, or never ran because the app closed first.
    //   --automation <folder> keeps an inbox open for an agent until it sends app.quit.
    //   Its requests run from the app's registry (ICommandRegistry), in a queue of its own. The app
    //   calls BeginFrame before the frame ticks and EndFrame once it is drawn.
    // =============================================================================
    class AutomationSession final
    {
    public:
        /**
         * A session when InScript or InInbox is given (not empty), else null (also when the script
         * cannot be read: logged). Paths are in UTF-8.
         * @param InScriptOutput Where the script's answers go; empty: beside the script
         */
        static TUniquePtr<AutomationSession> Create(const OpaaxString& InScript, const OpaaxString& InScriptOutput,
                                                    const OpaaxString& InInbox, ICommandRegistry& InCommands);

        explicit AutomationSession(ICommandRegistry& InCommands) : m_Runner(InCommands) {}

        AutomationRunner& GetRunner() noexcept { return m_Runner; }

        /** Runs the requests due this frame. */
        void BeginFrame();

        /**
         * After the frame is drawn.
         * @return True when the app should close: the script is done (and no inbox is open)
         */
        bool EndFrame();

        /** The app is closing: what is left unanswered gets an answer (AutomationRunner::Close). */
        void Close();

        /** 1 when a request of the script failed (or never ran: the app closed first), else 0. */
        int GetExitCode() const noexcept;

        /**
         * True while a script drives the app: its input is the only input. The app then ignores the
         * window's keys, mouse and focus changes, so a pointer resting on the window cannot move the
         * scripted one. An inbox session (--automation) shares the input with whoever uses the window.
         */
        bool OwnsInput() const noexcept { return m_Script != nullptr; }

    private:
        AutomationRunner             m_Runner;
        TUniquePtr<AutomationInbox>  m_Inbox;
        TUniquePtr<AutomationScript> m_Script;
    };
}
