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
        m_Worlds.push_back(MakeUnique<World>(Move(InName)));
        return m_Worlds.back().get();
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
    
    bool WorldManager::SetActiveWorld(World* InWorld) noexcept
    {
        if (InWorld == nullptr)
        {
            OPAAX_LOG(LogWorldManager, Error, "Trying to set active a null world!")
            
            return false;
        }
        
        if (m_ActiveWorld != nullptr)
        {
            m_ActiveWorld->OnDesactive();
        }
        
        m_ActiveWorld = InWorld;
        m_ActiveWorld->OnActive();
        
        OPAAX_LOG(LogWorldManager, Info, "New Active world -> '{}'", m_ActiveWorld->GetName().CStr())
        
        return true;
    }
}
