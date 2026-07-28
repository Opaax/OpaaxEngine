#include "WorldManager.h"

#include "World/Components/DummyComponent.h"

namespace Opaax
{
    // =========================================================================
    // CTOR
    // =========================================================================
    WorldManager::WorldManager()
    {
        RegisterNativeComponents();
    }

    void WorldManager::RegisterNativeComponents()
    {
        // NOTE: DummyComponent is the whole native set today — it is the only live component
        // type the engine owns. Not a placeholder for the registry: a real type registered
        // DLL-side is what proves the cross-boundary lookup in ComponentIdentityTests.
        m_Components.Register<DummyComponent>("Dummy");
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool WorldManager::Startup()
    {
        // NO WORLD IS CREATED HERE, on purpose. Starting a subsystem brings up infrastructure;
        // creating a world is CONTENT, and it is the host that decides which one (from the
        // project's startup level). Doing it here also made the boot order unfixable: the first
        // CreateWorld seals ComponentRegistry, so a world born during subsystem startup sealed
        // the registry before any game module had a chance to register into it.
        //
        // Nothing needs a world to exist this early — every consumer already null-checks
        // (RendererManager::Render guards; HierarchyPanel renders "No active world.").
        OPAAX_LOG(LogWorldManager, Info, "WorldManager started (no world yet — the host creates it)")
        return true;
    }

    void WorldManager::TearDown()
    {
        // Destroy through DestroyWorld instead of dropping m_Worlds, so every world still
        // announces OnActiveWorldChanged + OnWorldDestroyed on the way out. This is the LAST
        // moment those reach anyone: the loop has stopped but Engine is still bound and the
        // EventBus (registered first, so torn down last) is still alive to deliver them.
        // Shutdown() cannot do this — Engine unbinds before ShutdownAll.
        //
        // Back-to-front: DestroyWorld's erase then finds its target immediately.
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
    World* WorldManager::CreateWorld(OpaaxString InName)
    {
        // Seal here rather than in Startup: this is the moment the invariant actually needs to
        // hold ("no component type may arrive once a world exists" — Editor.md §3 L1), and
        // enforcing it at the single place worlds are born means no caller has to remember a
        // separate seal step. Idempotent.
        m_Components.Seal();

        m_Worlds.push_back(MakeUnique<World>(Move(InName)));
        World* lWorld = m_Worlds.back().get();

        OnWorldCreated.Broadcast(lWorld);

        return lWorld;
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
