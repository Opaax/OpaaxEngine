#include "Engine/GameInstance/GameInstance.h"

#include "Engine/GameInstance/GameInstanceSubsystemRegistry.h"

namespace Opaax
{
    GameInstance::GameInstance(const GameInstanceContext& InContext)
        : m_Context(MakeUnique<GameInstanceContext>(InContext))
    {
    }

    GameInstance::~GameInstance()
    {
        // Safety net for a host that never reached TearDown. Both are idempotent, so the
        // ordinary path (EndGame -> TearDown -> Shutdown -> destroy) does nothing here.
        ShutdownSubsystems();
    }

    Uint64 GameInstance::StartSubsystems(const GameInstanceSubsystemRegistry& InRegistry)
    {
        Uint64 lCreated = 0;

        InRegistry.ForEach([&](const IGameInstanceSubsystemEntry& InEntry)
        {
            InEntry.CreateInto(m_Subsystems, *m_Context);
            ++lCreated;
        });

        m_Subsystems.StartupAll();

        return lCreated;
    }

    void GameInstance::TearDownSubsystems()
    {
        if (m_bTornDown)
        {
            return;
        }

        m_bTornDown = true;
        m_Subsystems.TearDownAll();
    }

    void GameInstance::ShutdownSubsystems()
    {
        if (m_bShutDown)
        {
            return;
        }

        m_bShutDown = true;
        m_Subsystems.ShutdownAll();
    }

    void GameInstance::Update(double InDeltaTime)
    {
        m_Subsystems.UpdateAll(InDeltaTime);
    }
}
