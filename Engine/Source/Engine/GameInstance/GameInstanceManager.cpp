#include "Engine/GameInstance/GameInstanceManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Engine/Config/Config_Engine.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceSubsystemRegistry.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "World/WorldManager.h"

namespace Opaax
{
    GameInstanceManager::GameInstanceManager(EngineRegistries* InRegistries)
        : m_Registries(InRegistries)
    {
    }

    GameInstanceManager::~GameInstanceManager() = default;

    bool GameInstanceManager::Startup()
    {
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();

        m_Worlds    = &lEngine.GetWorldManager();
        m_Resources = &lEngine.GetResources();
        m_Events    = &lEngine.GetEngineEventBus();
        m_Input     = &lEngine.GetInput();
        m_Paths     = &OpaaxApplication::GetAppService<IPaths>();
        m_Config    = &OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().GetData();

        return true;
    }

    bool GameInstanceManager::StartGame()
    {
        if (m_GameInstance != nullptr)
        {
            OPAAX_LOG(LogGameInstanceManager, Error,
                      "StartGame refused — a game is already running ({} subsystem(s)). EndGame first.",
                      m_GameInstance->GetSubsystemCount());
            return false;
        }

        if (m_Registries == nullptr)
        {
            // No registry (e.g. in a test): refuse.
            OPAAX_LOG(LogGameInstanceManager, Error, "StartGame refused — no registries to create candidates from.");
            return false;
        }

        if (m_Worlds == nullptr || m_Resources == nullptr || m_Events == nullptr
            || m_Input == nullptr || m_Paths == nullptr || m_Config == nullptr)
        {
            OPAAX_LOG(LogGameInstanceManager, Error,
                      "StartGame refused — this manager was never started, so there is no engine context.");
            return false;
        }

        m_GameInstance = MakeUnique<GameInstance>(GameInstanceContext{*m_Worlds, *m_Resources, *m_Paths,
                                                                      *m_Events, *m_Input, *m_Config});
        ++m_SessionsStarted;

        const Uint64 lCreated = m_GameInstance->StartSubsystems(m_Registries->GameInstanceSubsystems());

        OPAAX_LOG(LogGameInstanceManager, Info,
                  "GAME STARTED (session #{}) — {} of {} session subsystem(s) created",
                  m_SessionsStarted, lCreated, m_Registries->GameInstanceSubsystems().Count());
        return true;
    }

    bool GameInstanceManager::EndGame()
    {
        if (m_GameInstance == nullptr)
        {
            return false;
        }

        const Uint64 lCount = m_GameInstance->GetSubsystemCount();

        // TearDown while everything is still alive, then Shutdown. Both are safe to repeat.
        m_GameInstance->TearDownSubsystems();
        m_GameInstance->ShutdownSubsystems();
        m_GameInstance.reset();

        OPAAX_LOG(LogGameInstanceManager, Info, "GAME ENDED (session #{}) — {} session subsystem(s) destroyed",
                  m_SessionsStarted, lCount);
        return true;
    }

    void GameInstanceManager::Update(double InDeltaTime)
    {
        if (m_GameInstance != nullptr)
        {
            m_GameInstance->Update(InDeltaTime);
        }
    }

    void GameInstanceManager::TearDown()
    {
        // In case the host never called EndGame; the context's references are still alive here.
        EndGame();
    }

    void GameInstanceManager::Shutdown()
    {
    }
}
