#pragma once

#include <atomic>
#include <string>

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Editor/Export/GameExportPlan.hpp"

namespace Opaax::Editor
{
    OPAAX_LOG_CATEGORY(GameExport);

    // =============================================================================
    // GameExport — runs a GameExportPlan in the background, one step after the other, their output
    //   added to a log file. The editor stays usable meanwhile; one export at a time. Needs the
    //   source tree (an editor build edits it) and CMake on the PATH.
    // =============================================================================
    class GameExport
    {
    public:
        enum class EState : Uint8
        {
            Idle,
            Running,
            Succeeded,
            Failed
        };

        GameExport() = default;

        /** Waits for an export still running: its CMake processes must not outlive the editor. */
        ~GameExport();

        GameExport(const GameExport&)            = delete;
        GameExport& operator=(const GameExport&) = delete;

        /**
         * Exports InGame from the workspace InWorkspace into InDestination; the commands' output goes
         * to InLog (emptied first).
         * @return False (logged) when an export is running, or the name or a path is refused
         */
        bool Start(const std::string& InWorkspace, const std::string& InGame, const std::string& InDestination,
                   const std::string& InLog);

        EState GetState() const noexcept { return m_State.load(); }
        bool   IsRunning() const noexcept { return GetState() == EState::Running; }

        /** The last export's settings. Stable while it runs. */
        const std::string& GetDestination() const noexcept { return m_Destination; }
        const std::string& GetLog() const noexcept { return m_Log; }

        /** The step a failed export stopped at, or empty. */
        std::string GetFailedStep() const;

    private:
        /** The worker: the steps in order, stopping at the first that fails. */
        void Run();

    private:
        Thread                 m_Worker;
        std::atomic<EState>    m_State{ EState::Idle };
        std::atomic<Int32>     m_FailedStep{ -1 };

        // Written by Start before the worker starts, read-only while it runs.
        TDynArray<ExportStep> m_Steps;
        std::string           m_Workspace;
        std::string           m_Destination;
        std::string           m_Log;
    };
}
