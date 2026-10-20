#include "Engine/GameInstance/GameInstance.h"

#include "Object/ObjectClassRegistry.h"

namespace Opaax
{
    GameInstance::GameInstance(const GameInstanceContext& InContext)
        : m_Context(MakeUnique<GameInstanceContext>(InContext))
    {
        m_Subsystems.SetContext(m_Context.get());
    }

    GameInstance::~GameInstance()
    {
        // In case the host never reached TearDown. Both calls are safe to repeat.
        ShutdownSubsystems();
    }

    Uint64 GameInstance::StartSubsystems(const ObjectClassRegistry& InClasses)
    {
        Uint64 lCreated = 0;

        for (const ObjectClass* lClass : CollectSubsystemClasses(InClasses, GameInstanceSubsystem::StaticClass()))
        {
            if (m_Subsystems.Add(*lClass))
            {
                ++lCreated;
            }
        }

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
