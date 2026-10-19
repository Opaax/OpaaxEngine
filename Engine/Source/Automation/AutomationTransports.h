#pragma once

#include "Automation/AutomationRunner.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // AutomationInbox — a folder that drives a running app: each <name>.request.json dropped in
    //   is read (in name order), removed and run, and its answer written to <name>.response.json
    //   (whole: written aside, then renamed). No socket: any tool that writes files can drive it.
    // =============================================================================
    class AutomationInbox
    {
    public:
        explicit AutomationInbox(OpaaxString InFolder);

        /** Creates the folder if needed. False (logged) when it cannot. */
        bool Open();

        /**
         * Moves the requests dropped since the last call into InRunner. A file that is not a
         * request is answered with an error at once.
         * @return Requests queued
         */
        Uint32 Poll(AutomationRunner& InRunner);

        /** Writes the answer of a request that came from here; ignores the others. */
        void Answer(const AutomationResponse& InResponse);

        const OpaaxString& GetFolder() const noexcept { return m_Folder; }

    private:
        void Write(const AutomationResponse& InResponse) const;

    private:
        OpaaxString                m_Folder;
        TUnorderedSet<OpaaxString> m_Pending;   // ids of requests from here, not answered yet
    };

    // =============================================================================
    // AutomationScript — a file of requests run in order (--exec), their answers written to an
    //   output file as they come: {"script", "done", "failed", "responses": [...]}.
    // =============================================================================
    class AutomationScript
    {
    public:
        /**
         * Reads InScript and queues its requests into InRunner. Answers go to InOutput.
         * False (logged) when the file cannot be read or is not a script.
         */
        bool Load(const OpaaxString& InScript, const OpaaxString& InOutput, AutomationRunner& InRunner);

        /** Records the answer of a request from the script and rewrites the output file. */
        void Answer(const AutomationResponse& InResponse);

        /** Every request answered. */
        bool IsDone() const noexcept { return m_bLoaded && m_Pending.empty(); }

        Uint64 GetFailed() const noexcept { return m_Failed; }

        /** The default output file of InScript: next to it, <name>.out.json. */
        static OpaaxString DefaultOutput(const OpaaxString& InScript);

    private:
        void WriteOutput() const;

    private:
        OpaaxString                m_Script;
        OpaaxString                m_Output;
        TUnorderedSet<OpaaxString> m_Pending;
        nlohmann::json             m_Responses = nlohmann::json::array();
        Uint64                     m_Failed    = 0;
        bool                       m_bLoaded   = false;
    };
}
