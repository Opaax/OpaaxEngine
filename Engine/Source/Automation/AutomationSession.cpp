#include "Automation/AutomationSession.h"

#include "Application/Services/IEngine.h"

namespace Opaax
{
    TUniquePtr<AutomationSession> AutomationSession::Create(const char* InScript, const char* InScriptOutput,
                                                            const char* InInbox)
    {
        if (InScript == nullptr && InInbox == nullptr)
        {
            return nullptr;
        }

        TUniquePtr<AutomationSession> lSession = MakeUnique<AutomationSession>();
        AutomationSession&            lSelf    = *lSession;

        // Each answer goes back the way its request came.
        lSelf.m_Runner.SetResponseSink([&lSelf](const AutomationResponse& InResponse)
        {
            if (lSelf.m_Script != nullptr) { lSelf.m_Script->Answer(InResponse); }
            if (lSelf.m_Inbox != nullptr)  { lSelf.m_Inbox->Answer(InResponse); }
        });

        if (InScript != nullptr)
        {
            const std::filesystem::path lScript(InScript);
            const std::filesystem::path lOutput = (InScriptOutput != nullptr) ? std::filesystem::path(InScriptOutput)
                                                                              : AutomationScript::DefaultOutput(lScript);

            lSelf.m_Script = MakeUnique<AutomationScript>();
            if (!lSelf.m_Script->Load(lScript, lOutput, lSelf.m_Runner))
            {
                return nullptr;
            }
        }

        if (InInbox != nullptr)
        {
            lSelf.m_Inbox = MakeUnique<AutomationInbox>(std::filesystem::path(InInbox));
            if (!lSelf.m_Inbox->Open())
            {
                lSelf.m_Inbox.reset();
            }
        }

        return lSession;
    }

    void AutomationSession::Start(IEngine& InEngine)
    {
        EngineAutomation::Register(m_Runner, InEngine, *this);
    }

    void AutomationSession::BeginFrame()
    {
        ++m_Frame;

        if (m_Inbox != nullptr)
        {
            m_Inbox->Poll(m_Runner);
        }
        m_Runner.Tick();
    }

    bool AutomationSession::EndFrame(IEngine& InEngine)
    {
        if (!m_PendingScreenshot.empty())
        {
            InEngine.CaptureFrame(OpaaxString(m_PendingScreenshot.c_str()));
            m_PendingScreenshot.clear();
        }

        // A script with no inbox beside it ends the session once answered.
        const bool bScriptDone = m_Script != nullptr && m_Script->IsDone() && m_Runner.IsIdle() && m_Inbox == nullptr;
        if (bScriptDone && !m_bQuitRequested)
        {
            OPAAX_LOG(LogAutomation, Info, "Automation script done: {} request(s), {} failed",
                      m_Runner.GetAnswered(), m_Script->GetFailed());
        }

        return m_bQuitRequested || bScriptDone;
    }

    void AutomationSession::Close()
    {
        m_Runner.Close();
    }

    int AutomationSession::GetExitCode() const noexcept
    {
        return (m_Script != nullptr && (m_Script->GetFailed() > 0 || !m_Script->IsDone())) ? 1 : 0;
    }

    void AutomationSession::RequestScreenshot(const std::string& InPath)
    {
        m_PendingScreenshot = InPath;
    }
}
