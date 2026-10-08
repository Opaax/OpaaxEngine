#pragma once

#include <string>

#include "Automation/AutomationRunner.h"
#include "Automation/EngineAutomationCommands.h"
#include "Automation/AutomationTransports.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class IEngine;

    // =============================================================================
    // AutomationSession — the app driven from outside, as its command line asks:
    //   --exec <script.json> [--exec-out <answers.json>] runs a script's requests, then closes the
    //     app; its exit code is 1 when a request failed, or never ran because the app closed first.
    //   --automation <folder> keeps an inbox open for an agent until it sends app.quit.
    //   The app calls BeginFrame before the frame ticks and EndFrame once it is drawn.
    // =============================================================================
    class AutomationSession final : public IAutomationHost
    {
    public:
        /**
         * A session when InScript or InInbox is given, else null (also when the script cannot be
         * read: logged).
         * @param InScriptOutput Where the script's answers go; null: beside the script
         */
        static TUniquePtr<AutomationSession> Create(const char* InScript, const char* InScriptOutput, const char* InInbox);

        AutomationRunner& GetRunner() noexcept { return m_Runner; }

        /** Registers the engine's commands. Called once the engine is up. */
        void Start(IEngine& InEngine);

        /** Runs the requests due this frame. */
        void BeginFrame();

        /**
         * After the frame is drawn, before it is shown: takes the screenshot asked for.
         * @return True when the app should close (asked to, or the script is done)
         */
        bool EndFrame(IEngine& InEngine);

        /** The app is closing: what is left unanswered gets an answer (AutomationRunner::Close). */
        void Close();

        /** 1 when a request of the script failed (or never ran: the app closed first), else 0. */
        int GetExitCode() const noexcept;

        //~Begin IAutomationHost interface
    public:
        void   RequestScreenshot(const std::string& InPath) override;
        void   RequestQuit() override { m_bQuitRequested = true; }
        Uint64 GetFrameIndex() const override { return m_Frame; }
        //~End IAutomationHost interface

    private:
        AutomationRunner             m_Runner;
        TUniquePtr<AutomationInbox>  m_Inbox;
        TUniquePtr<AutomationScript> m_Script;

        std::string m_PendingScreenshot;
        bool        m_bQuitRequested = false;
        Uint64      m_Frame          = 0;
    };
}
