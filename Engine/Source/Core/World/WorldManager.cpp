#include "WorldManager.h"

namespace Opaax
{
    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool WorldManager::Startup()
    {
        // Always have a render target: spin up a default world and make it active.
        World* lDefault = CreateWorld("Main");
        SetActiveWorld(lDefault);

        OPAAX_LOG(LogWorldManager, Info, "WorldManager started ({} world(s))", GetWorldCount())
        return true;
    }

    void WorldManager::Shutdown()
    {
        m_ActiveWorld = nullptr; // clear the non-owning slot BEFORE releasing the owners
        m_Worlds.clear();

        OPAAX_LOG(LogWorldManager, Info, "WorldManager shutdown")
    }

    // =========================================================================
    // World lifetime
    // =========================================================================
    World* WorldManager::CreateWorld(OpaaxString InName)
    {
        m_Worlds.push_back(MakeUnique<World>(std::move(InName)));
        return m_Worlds.back().get();
    }

    void WorldManager::DestroyWorld(World* InWorld)
    {
        if (InWorld == nullptr)
        {
            return;
        }

        if (m_ActiveWorld == InWorld)
        {
            m_ActiveWorld = nullptr;
        }

        for (auto lIt = m_Worlds.begin(); lIt != m_Worlds.end(); ++lIt)
        {
            if (lIt->get() == InWorld)
            {
                m_Worlds.erase(lIt);
                break;
            }
        }
    }

    // =========================================================================
    // Active (render) world
    // =========================================================================
    void WorldManager::SetActiveWorld(World* InWorld) noexcept
    {
        m_ActiveWorld = InWorld;
        OPAAX_LOG(LogWorldManager, Info, "Active world -> '{}'", InWorld ? InWorld->GetName().CStr() : "none")
    }
}
