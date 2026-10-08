#include "Editor/Export/GameExport.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace Opaax::Editor
{
    namespace
    {
#if defined(_WIN32)
        constexpr bool IS_WINDOWS = true;
#else
        constexpr bool IS_WINDOWS = false;
#endif
    }

    GameExport::~GameExport()
    {
        if (m_Worker.joinable())
        {
            if (IsRunning())
            {
                OPAAX_LOG(LogGameExport, Info, "Waiting for the export to '{}' to finish...", m_Destination);
            }
            m_Worker.join();
        }
    }

    bool GameExport::Start(const std::string& InWorkspace, const std::string& InGame, const std::string& InDestination,
                           const std::string& InLog)
    {
        if (IsRunning())
        {
            OPAAX_LOG(LogGameExport, Warn, "An export is already running (to '{}')", m_Destination);
            return false;
        }

        TDynArray<ExportStep> lSteps = GameExportPlan::Make(InGame, InDestination);
        if (lSteps.empty() || !GameExportPlan::IsSafePath(InWorkspace) || !GameExportPlan::IsSafePath(InLog))
        {
            OPAAX_LOG(LogGameExport, Error, "Export refused: '{}' is not a game name, or a path holds a quote", InGame);
            return false;
        }

        // The last one has finished: its thread can be let go.
        if (m_Worker.joinable())
        {
            m_Worker.join();
        }

        std::error_code lError;
        std::filesystem::create_directories(std::filesystem::path(InLog).parent_path(), lError);
        std::ofstream(InLog, std::ios::trunc) << "Export of " << InGame << " into " << InDestination << "\n";

        m_Steps       = Move(lSteps);
        m_Workspace   = InWorkspace;
        m_Destination = InDestination;
        m_Log         = InLog;
        m_FailedStep  = -1;
        m_State       = EState::Running;

        OPAAX_LOG(LogGameExport, Info, "Exporting {} into '{}' ({} steps; output in '{}')", InGame, InDestination,
                  m_Steps.size(), InLog);
        m_Worker = Thread([this]() { Run(); });
        return true;
    }

    std::string GameExport::GetFailedStep() const
    {
        const Int32 lStep = m_FailedStep.load();
        return (lStep >= 0 && GetState() == EState::Failed) ? m_Steps[static_cast<Uint64>(lStep)].Description : std::string();
    }

    void GameExport::Run()
    {
        for (Uint64 lIndex = 0; lIndex < m_Steps.size(); ++lIndex)
        {
            const ExportStep& lStep = m_Steps[lIndex];
            OPAAX_LOG(LogGameExport, Info, "Export step {}/{}: {}", lIndex + 1, m_Steps.size(), lStep.Description);

            const std::string lLine = GameExportPlan::ShellLine(lStep.Command, m_Workspace, m_Log, IS_WINDOWS);
            if (std::system(lLine.c_str()) != 0)
            {
                m_FailedStep = static_cast<Int32>(lIndex);
                m_State      = EState::Failed;
                OPAAX_LOG(LogGameExport, Error, "Export failed at '{}': its output is in '{}'", lStep.Description, m_Log);
                return;
            }
        }

        m_State = EState::Succeeded;
        OPAAX_LOG(LogGameExport, Info, "Exported into '{}'", m_Destination);
    }
}
