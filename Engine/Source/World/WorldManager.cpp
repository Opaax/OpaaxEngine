#include "World/WorldManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IConfigSystem.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Core/Profiling/Profiler.h"   // OPAAX_STAT_SCOPE
#include "Engine/Config/Config_Engine.h"
#include "Engine/GameInstance/GameInstance.h"
#include "Engine/GameInstance/GameInstanceManager.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "UI/UISubsystem.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Level.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapSerializer.h"
#include "World/Systems/WorldContext.h"

namespace Opaax
{
    // =========================================================================
    // CTOR
    // =========================================================================
    WorldManager::WorldManager(EngineRegistries* InRegistries)
        : m_Registries(InRegistries)
    {
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool WorldManager::Startup()
    {
        // Resolve the engine parts of every WorldContext once (from the manager, all created already).
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();

        m_Resources = &lEngine.GetResources();
        m_Events    = &lEngine.GetEngineEventBus();
        m_Debug     = &lEngine.GetDebugDraw();
        m_Paths     = &OpaaxApplication::GetAppService<IPaths>();
        m_Config    = &OpaaxApplication::GetAppService<IConfigSystem>().Get<Config_Engine>().GetData();
        m_Input     = &lEngine.GetInput();
        m_GameInstances = &lEngine.GetGameInstances();

        return true;
    }

    // =========================================================================
    // Tick — only the active world runs.
    // =========================================================================
    void WorldManager::Update(double InDeltaTime)
    {
        // Decided in Update (once per frame); FixedUpdate uses the same answer.
        m_bTickThisFrame = !m_bPaused || m_bStepRequested;
        m_bStepRequested = false;

        if (!m_bTickThisFrame || m_ActiveWorld == nullptr)
        {
            return;
        }

        // Scope for everything the world's subsystems do.
        OPAAX_STAT_SCOPE("World");

        m_ActiveWorld->GetSubsystems().UpdateAll(InDeltaTime);
    }

    void WorldManager::FixedUpdate(double InFixedDeltaTime)
    {
        if (!m_bTickThisFrame || m_ActiveWorld == nullptr)
        {
            return;
        }

        m_ActiveWorld->GetSubsystems().FixedUpdateAll(InFixedDeltaTime);
    }

    void WorldManager::TearDown()
    {
        while (!m_Worlds.empty())
        {
            DestroyWorld(m_Worlds.back().get());
        }
    }

    void WorldManager::Shutdown()
    {
        // Usually nothing left (TearDown destroyed the worlds). Safety net for a Shutdown without
        // TearDown, where worlds are destroyed silently.
        m_ActiveWorld = nullptr; // clear before releasing the worlds
        m_Worlds.clear();
    }

    // =========================================================================
    // World lifetime
    // =========================================================================
    World* WorldManager::CreateWorld(OpaaxString InName, EWorldMode InMode)
    {
        // Seal here: this is what makes a world exist (covers every CreateWorld, not only startup).
        if (m_Registries != nullptr)
        {
            m_Registries->SealAll();
        }

        m_Worlds.emplace_back(MakeUnique<World>(Move(InName), InMode));
        World* lWorld = m_Worlds.back().get();

        // Level before subsystems: subsystems receive the world. No Level for a bare test manager.
        if (m_Registries != nullptr && m_Paths != nullptr && m_Resources != nullptr)
        {
            lWorld->SetLevel(MakeUnique<Level>(*lWorld, m_Registries->Components(),
                                               *m_Paths, *m_Resources, m_Registries->Resources()));
        }

        CreateSubsystemsFor(*lWorld);

        // After the subsystems started: a WorldCreated subscriber may look one up.
        OnWorldCreated.Broadcast(lWorld);

        return lWorld;
    }

    World* WorldManager::CloneWorld(const World& InSource, EWorldMode InMode)
    {
        if (m_Registries == nullptr)
        {
            // Refuse: without a ComponentRegistry the capture would be empty.
            OPAAX_LOG(LogWorldManager, Error,
                      "CloneWorld '{}' refused — no registries, so there is nothing to capture through.",
                      InSource.GetName().CStr());
            return nullptr;
        }

        // Whole world, runtime entities included. Captured before the clone exists.
        const MapData lSnapshot = MapSerializer::CaptureWorld(InSource, m_Registries->Components());

        // Created like any world: sealed, own context, subsystems for its mode.
        World* lClone = CreateWorld(InSource.GetName(), InMode);

        // The clone's subsystems start on an empty world; entities are added below
        // (subsystems read content from their first Update, never from Startup).
        const Uint64 lInstantiated = MapFactory::Instantiate(lSnapshot, *lClone, m_Registries->Components());

        // Copy the source's mount state without mounting: the entities are already here
        // (mounting again would duplicate them).
        if (lClone->GetLevel() != nullptr && InSource.GetLevel() != nullptr)
        {
            lClone->GetLevel()->AdoptMountedFrom(*InSource.GetLevel());
        }

        if (lInstantiated < lSnapshot.EntityCount())
        {
            OPAAX_LOG(LogWorldManager, Warn,
                      "Clone of '{}' is INCOMPLETE — {} of {} entities; the map factory logged which were refused.",
                      InSource.GetName().CStr(), lInstantiated, lSnapshot.EntityCount());
        }

        OPAAX_LOG(LogWorldManager, Trace, "Cloned world '{}' ({}) -> '{}' ({}) — {} of {} entities",
                  InSource.GetName().CStr(), ToString(InSource.GetMode()),
                  lClone->GetName().CStr(), ToString(lClone->GetMode()),
                  lInstantiated, lSnapshot.EntityCount());

        return lClone;
    }

    Uint64 WorldManager::DestroyWorldsOfMode(EWorldMode InMode)
    {
        // Collected first: DestroyWorld changes m_Worlds.
        TDynArray<World*> lDoomed;

        for (const TUniquePtr<World>& lWorld : m_Worlds)
        {
            if (lWorld != nullptr && lWorld->GetMode() == InMode)
            {
                lDoomed.emplace_back(lWorld.get());
            }
        }

        for (World* lWorld : lDoomed)
        {
            DestroyWorld(lWorld);
        }

        return static_cast<Uint64>(lDoomed.size());
    }

    Uint64 WorldManager::CountWorldsOfMode(EWorldMode InMode) const noexcept
    {
        Uint64 lCount = 0;

        for (const TUniquePtr<World>& lWorld : m_Worlds)
        {
            if (lWorld != nullptr && lWorld->GetMode() == InMode)
            {
                ++lCount;
            }
        }

        return lCount;
    }

    void WorldManager::CreateSubsystemsFor(World& InWorld)
    {
        if (m_Registries == nullptr)
        {
            // No registry (test): a world without subsystems.
            return;
        }

        // A null reference means Startup never ran: no subsystems.
        if (m_Resources == nullptr || m_Events == nullptr || m_Debug == nullptr || m_Paths == nullptr
            || m_Config == nullptr || m_Input == nullptr)
        {
            OPAAX_LOG(LogWorldManager, Error,
                      "CreateWorld '{}' — WorldManager was never started, so there is no engine context. World created with NO subsystems.",
                      InWorld.GetName().CStr());
            return;
        }

        // Per world, and allowed to be null (Edit worlds have no game).
        InputMappingSubsystem* lActions = nullptr;
        UISubsystem*           lUI      = nullptr;

        if (m_GameInstances != nullptr)
        {
            if (GameInstance* lGame = m_GameInstances->GetGameInstance())
            {
                lActions = lGame->GetSubsystems().GetSubsystem<InputMappingSubsystem>();
                lUI      = lGame->GetSubsystems().GetSubsystem<UISubsystem>();
            }
        }

        InWorld.SetContext(WorldContext{InWorld, *m_Resources, *m_Paths, *m_Events, *m_Input, *m_Config,
                                        lActions, lUI, *m_Debug, &m_Registries->Components()});

        WorldContext* lContext = InWorld.GetContext();
        OPAAX_ASSERT(lContext != nullptr);

        // Take the types this world qualifies for (ShouldCreate); rejected ones are never built.
        Uint64 lCreated = 0;

        m_Registries->WorldSubsystems().ForEach([&](const IWorldSubsystemEntry& InEntry)
        {
            if (!InEntry.ShouldCreate(InWorld))
            {
                return;
            }

            InEntry.CreateInto(InWorld.GetSubsystems(), *lContext);
            ++lCreated;
        });

        // Create all, then start all, so a subsystem can find another during its Startup.
        InWorld.GetSubsystems().StartupAll();

        OPAAX_LOG(LogWorldManager, Trace, "World '{}' ({}) — {} of {} subsystem candidate(s) created",
                  InWorld.GetName().CStr(), ToString(InWorld.GetMode()),
                  lCreated, m_Registries->WorldSubsystems().Count());
    }

    void WorldManager::DestroyWorld(World* InWorld)
    {
        if (InWorld == nullptr)
        {
            OPAAX_LOG(LogWorldManager, Error, "Trying to destroy a null world!");
            return;
        }

        if (m_ActiveWorld == InWorld)
        {
            m_ActiveWorld->OnDesactive();
            m_ActiveWorld = nullptr;
            
            OnActiveWorldChanged.Broadcast(InWorld, nullptr);
        }
        
        OnWorldDestroyed.Broadcast(InWorld);

        // Here, while everything a subsystem may reach is still alive. ~World repeats it as a
        // safety net (safe to call twice).
        InWorld->ShutdownSubsystems();

        for (auto lIt = m_Worlds.begin(); lIt != m_Worlds.end(); ++lIt)
        {
            if (lIt->get() == InWorld)
            {
                m_Worlds.erase(lIt);
                break;
            }
        }
    }
    
    bool WorldManager::SetActiveWorld(World* InWorld) noexcept
    {
        if (InWorld == nullptr)
        {
            OPAAX_LOG(LogWorldManager, Error, "Trying to set active a null world!");

            return false;
        }
        
        if (m_ActiveWorld == InWorld)
        {
            return true;
        }

        World* lOldWorld = m_ActiveWorld;

        if (lOldWorld != nullptr)
        {
            lOldWorld->OnDesactive();
        }

        m_ActiveWorld = InWorld;
        m_ActiveWorld->OnActive();

        OnActiveWorldChanged.Broadcast(lOldWorld, m_ActiveWorld);

        return true;
    }
}
