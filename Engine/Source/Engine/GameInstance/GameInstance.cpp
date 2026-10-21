#include "Engine/GameInstance/GameInstance.h"

#include "Engine/Registries/AutoRegistration.h"
#include "Object/ObjectClassRegistry.h"

namespace Opaax
{
    // The class a project without its own gameInstanceClass runs.
    OPAAX_REGISTER_OBJECT(GameInstance);

    GameInstance::GameInstance() = default;

    GameInstance::~GameInstance()
    {
        // In case the owner never called End: Shutdown only, since TearDown may use services already gone.
        if (!m_bEnded)
        {
            m_Subsystems.ShutdownAll();
        }
    }

    TStrongObjectPtr<GameInstance> GameInstance::Create(const OpaaxStringID InClassName,
                                                        const ObjectClassRegistry& InClasses)
    {
        if (InClassName.IsValid())
        {
            const ObjectClass* lClass = InClasses.FindByName(InClassName);

            if (lClass != nullptr && !lClass->IsAbstract() && lClass->IsChildOf(StaticClass()))
            {
                return Cast<GameInstance>(lClass->CreateInstance());
            }

            OPAAX_LOG(LogGameInstance, Error,
                      "'{}' is not a GameInstance class of this game, so it runs a plain GameInstance. Declare it "
                      "with OPAAX_OBJECT({}, Opaax::GameInstance) and register it with OPAAX_REGISTER_OBJECT({}).",
                      InClassName.CStr(), InClassName.CStr(), InClassName.CStr());
        }

        return NewObject<GameInstance>();
    }

    // =========================================================================
    // Lifetime
    // =========================================================================
    void GameInstance::Init(const GameInstanceContext& InContext)
    {
        if (m_Context != nullptr)
        {
            OPAAX_LOG(LogGameInstance, Error, "Init refused for '{}': it already has its services.", GetName().CStr());
            return;
        }

        m_Context = MakeUnique<GameInstanceContext>(InContext);
        m_Subsystems.SetContext(m_Context.get());
    }

    Uint64 GameInstance::Start(const ObjectClassRegistry& InClasses)
    {
        if (m_Context == nullptr || m_bStarted)
        {
            OPAAX_LOG(LogGameInstance, Error, "Start refused for '{}': {}", GetName().CStr(),
                      m_bStarted ? "it has started already." : "Init first, with the game's services.");
            return 0;
        }

        m_bStarted = true;

        Uint64 lCreated = 0;
        for (const ObjectClass* lClass : CollectSubsystemClasses(InClasses, GameInstanceSubsystem::StaticClass()))
        {
            if (m_Subsystems.Add(*lClass))
            {
                ++lCreated;
            }
        }

        m_Subsystems.StartupAll();

        OnStart();

        return lCreated;
    }

    void GameInstance::End()
    {
        if (m_bEnded)
        {
            return;
        }

        m_bEnded = true;

        // OnEnd answers OnStart: a game that never started has nothing to end.
        if (m_bStarted)
        {
            OnEnd();
        }

        // TearDown while everything is still alive, then Shutdown.
        m_Subsystems.TearDownAll();
        m_Subsystems.ShutdownAll();
    }

    void GameInstance::Update(double InDeltaTime)
    {
        m_Subsystems.UpdateAll(InDeltaTime);
    }
}
