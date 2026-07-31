#include "WorldManager.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
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
        // Resolve the engine-side half of every future WorldContext, ONCE. Safe during our own
        // Startup because Engine's accessors resolve-from-manager first and the create pass has
        // already built every subsystem — never a lazy self-Startup (F3 / L6). Same pattern
        // RendererManager::Startup uses.
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();

        m_Resources = &lEngine.GetResources();
        m_Events    = &lEngine.GetEngineEventBus();
        m_Debug     = &lEngine.GetDebugDraw();

        OPAAX_LOG(LogWorldManager, Info, "WorldManager started (no world yet — the host creates it)")
        return true;
    }

    // =========================================================================
    // Tick — only the ACTIVE world simulates (a PIE clone and the edit world coexist).
    // =========================================================================
    void WorldManager::Update(double InDeltaTime)
    {
        if (m_ActiveWorld == nullptr)
        {
            return;
        }

        m_ActiveWorld->GetSubsystems().UpdateAll(InDeltaTime);
    }

    void WorldManager::FixedUpdate(double InFixedDeltaTime)
    {
        if (m_ActiveWorld == nullptr)
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

        OPAAX_LOG(LogWorldManager, Info, "WorldManager torn down")
    }

    void WorldManager::Shutdown()
    {
        // Normally a no-op: TearDown already destroyed and announced every world. This stays
        // as a safety net for paths that Shutdown WITHOUT a TearDown (e.g. ~Engine()), where
        // worlds necessarily die silently — Engine has unbound by now, so a broadcast here
        // would reach nobody anyway.
        m_ActiveWorld = nullptr; // clear the non-owning slot BEFORE releasing the owners
        m_Worlds.clear();

        OPAAX_LOG(LogWorldManager, Info, "WorldManager shutdown")
    }

    // =========================================================================
    // World lifetime
    // =========================================================================
    World* WorldManager::CreateWorld(OpaaxString InName, EWorldMode InMode)
    {
        // Sealing belongs here, not at the engine level: the rule is "no registration once a
        // world exists", and THIS is the line that makes one exist. Hoisting it into
        // Engine::FinishStartup would only cover the startup world and silently leave every
        // later CreateWorld (PIE clones, S4) unsealed.
        if (m_Registries != nullptr)
        {
            m_Registries->SealAll();
        }

        m_Worlds.push_back(MakeUnique<World>(Move(InName), InMode));
        World* lWorld = m_Worlds.back().get();

        CreateSubsystemsFor(*lWorld);

        // AFTER the subsystems exist and started: a WorldCreated subscriber may reasonably ask
        // the new world for one of them.
        OnWorldCreated.Broadcast(lWorld);

        return lWorld;
    }

    World* WorldManager::CloneWorld(const World& InSource, EWorldMode InMode)
    {
        if (m_Registries == nullptr)
        {
            // Refuse rather than hand back an empty world that LOOKS like a clone: with no
            // ComponentRegistry the capture below is empty by construction, so every entity would
            // be silently missing (L22 — no lazy safety net).
            OPAAX_LOG(LogWorldManager, Error,
                      "CloneWorld '{}' refused — no registries, so there is nothing to capture through.",
                      InSource.GetName().CStr())
            return nullptr;
        }

        // UNFILTERED on purpose: no MapId means "the whole world, runtime spawns included" (WM2).
        // Captured BEFORE the clone exists, so nothing the creation path does can perturb it.
        const MapData lSnapshot = MapSerializer::Capture(InSource, m_Registries->Components());

        // The ordinary creation path — that is the point. The clone seals, gets its own context,
        // and takes the subsystems ITS mode qualifies for, exactly as any other world does.
        World* lClone = CreateWorld(InSource.GetName(), InMode);

        // NOTE: the clone's subsystems have already started, on a world that is still EMPTY — the
        // entities land below. Same shape as the startup world, whose entities the host spawns after
        // FinishStartup (BO4), so the rule holds uniformly: a subsystem reads world content from its
        // first Update, never from Startup (WS7).
        const Uint64 lInstantiated = MapFactory::Instantiate(lSnapshot, *lClone, m_Registries->Components());

        if (lInstantiated < lSnapshot.EntityCount())
        {
            OPAAX_LOG(LogWorldManager, Warn,
                      "Clone of '{}' is INCOMPLETE — {} of {} entities; the map factory logged which were refused.",
                      InSource.GetName().CStr(), lInstantiated, lSnapshot.EntityCount())
        }

        OPAAX_LOG(LogWorldManager, Info, "Cloned world '{}' ({}) -> '{}' ({}) — {} of {} entities",
                  InSource.GetName().CStr(), ToString(InSource.GetMode()),
                  lClone->GetName().CStr(), ToString(lClone->GetMode()),
                  lInstantiated, lSnapshot.EntityCount())

        return lClone;
    }

    void WorldManager::CreateSubsystemsFor(World& InWorld)
    {
        if (m_Registries == nullptr)
        {
            // A bare manager in a test. No candidates exist, so a world with no subsystems is
            // the correct outcome, not an error.
            return;
        }

        // The context must exist BEFORE any subsystem is constructed — it IS the ctor argument.
        // A null sibling here means Startup never ran; the world then gets no subsystems rather
        // than a context full of dangling references.
        if (m_Resources == nullptr || m_Events == nullptr || m_Debug == nullptr)
        {
            OPAAX_LOG(LogWorldManager, Error,
                      "CreateWorld '{}' — WorldManager was never started, so there is no engine context. World created with NO subsystems.",
                      InWorld.GetName().CStr())
            return;
        }

        InWorld.SetContext(WorldContext{InWorld, *m_Resources, *m_Events, *m_Debug});

        WorldContext* lContext = InWorld.GetContext();
        OPAAX_ASSERT(lContext != nullptr);

        // Walk every candidate in registration order and take the ones this world qualifies for.
        // ShouldCreate is STATIC, so a rejected candidate is never constructed — an Edit-only
        // overlay does not EXIST in a Play world rather than sitting there inert.
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

        // One StartupAll for the whole set, so the create pass finishes before any Startup runs
        // and a subsystem can reach a sibling during its own (F3, one scope down).
        InWorld.GetSubsystems().StartupAll();

        OPAAX_LOG(LogWorldManager, Info, "World '{}' ({}) — {} of {} subsystem candidate(s) created",
                  InWorld.GetName().CStr(), ToString(InWorld.GetMode()),
                  lCreated, m_Registries->WorldSubsystems().Count())
    }

    void WorldManager::DestroyWorld(World* InWorld)
    {
        if (InWorld == nullptr)
        {
            OPAAX_LOG(LogWorldManager, Error, "Trying to destroy a null world!")
            return;
        }

        if (m_ActiveWorld == InWorld)
        {
            m_ActiveWorld->OnDesactive();
            m_ActiveWorld = nullptr;
            
            OnActiveWorldChanged.Broadcast(InWorld, nullptr);
        }
        
        OnWorldDestroyed.Broadcast(InWorld);

        // HERE, not in ~World: this is the LC-correct moment — every engine sibling a subsystem
        // might reach through its context (Resources, the bus, DebugDraw) is still alive. By the
        // time the UniquePtr below releases, we are inside destruction. ~World repeats the call
        // as an idempotent safety net for the Shutdown path, which never comes through here.
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
            OPAAX_LOG(LogWorldManager, Error, "Trying to set active a null world!")

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

        OPAAX_LOG(LogWorldManager, Info, "New Active world -> '{}'", m_ActiveWorld->GetName().CStr())

        OnActiveWorldChanged.Broadcast(lOldWorld, m_ActiveWorld);

        return true;
    }
}
