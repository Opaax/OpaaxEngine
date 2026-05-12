#include "World.h"

#include "Scene/Scene.h"
#include "ECS/Components/SceneIDComponent.h"

namespace Opaax
{
    // =============================================================================
    // CTOR - DTOR (out-of-line: m_Scenes holds UniquePtr<Scene> forward-decl in header)
    // =============================================================================
    World::World()  = default;
    World::~World() = default;

    // =============================================================================
    // Entity API
    // =============================================================================
    void World::Clear() noexcept
    {
        m_Registry.clear();
        m_EntityCount.store(0, std::memory_order_relaxed);

        OPAAX_CORE_TRACE("World::Clear() — all entities destroyed.");
    }

    Uint32 World::GetEntityCount() const noexcept
    {
        return m_EntityCount;
    }

    void World::DestroyEntity(EntityID InEntity, bool bDestroyChildren)
    {
        if (!IsValid(InEntity))
        {
            OPAAX_CORE_WARN("World::DestroyEntity — invalid entity, ignored.");
            return;
        }

        // Snapshot direct children — we cannot mutate the registry while iterating its view.
        TDynArray<EntityID> lChildren;
        {
            auto lView = m_Registry.view<ECS::ParentComponent>();
            for (auto lEnt : lView)
            {
                if (lView.get<ECS::ParentComponent>(lEnt).Parent == InEntity)
                {
                    lChildren.push_back(lEnt);
                }
            }
        }

        if (bDestroyChildren)
        {
            for (EntityID lChild : lChildren)
            {
                DestroyEntity(lChild, true);
            }
        }
        else
        {
            // Promote children to root — preserves their data.
            for (EntityID lChild : lChildren)
            {
                if (m_Registry.valid(lChild) && m_Registry.all_of<ECS::ParentComponent>(lChild))
                {
                    m_Registry.remove<ECS::ParentComponent>(lChild);
                }
            }
        }

        m_Registry.destroy(InEntity);
        m_EntityCount.fetch_sub(1, std::memory_order_relaxed);
    }

    EntityID World::FindByUuid(Uint64 InUuid) const noexcept
    {
        if (InUuid == 0) { return ENTITY_NONE; }

        auto lView = m_Registry.view<const ECS::UuidComponent>();
        for (auto lEnt : lView)
        {
            if (lView.get<const ECS::UuidComponent>(lEnt).Id == InUuid)
            {
                return lEnt;
            }
        }
        return ENTITY_NONE;
    }

    // =============================================================================
    // Scene stack
    // =============================================================================
    Uint32 World::AllocateSceneID() noexcept
    {
        // Monotonic, skip 0 (reserved for PersistentSceneID). Overflow at 2^32
        // pushes is implausible; if reached, wraps back through 0 — accept and log.
        const Uint32 lID = m_NextSceneID++;
        if (m_NextSceneID == PersistentSceneID)
        {
            OPAAX_CORE_WARN("World::AllocateSceneID — counter wrapped, skipping sentinel.");
            m_NextSceneID = 1;
        }
        return lID;
    }

    Scene* World::PushScene(UniquePtr<Scene> InScene)
    {
        OPAAX_CORE_ASSERT(InScene != nullptr)

        if (!m_Scenes.empty())
        {
            m_Scenes.back()->OnExit();
            OPAAX_CORE_TRACE("World::PushScene — '{}' exited.", m_Scenes.back()->GetName());
        }

        InScene->SetSceneID(AllocateSceneID());
        OPAAX_CORE_TRACE("World::PushScene — loading '{}' (SceneID={}).",
            InScene->GetName(), InScene->GetSceneID());

        InScene->OnLoad();
        InScene->OnEnter();

        m_Scenes.push_back(std::move(InScene));
        return m_Scenes.back().get();
    }

    void World::PopScene()
    {
        if (m_Scenes.empty())
        {
            OPAAX_CORE_WARN("World::PopScene — scene stack is empty, ignored.");
            return;
        }

        OPAAX_CORE_TRACE("World::PopScene — unloading '{}' (SceneID={}).",
            m_Scenes.back()->GetName(), m_Scenes.back()->GetSceneID());
        m_Scenes.back()->OnExit();
        m_Scenes.back()->OnUnload();
        m_Scenes.pop_back();

        if (!m_Scenes.empty())
        {
            OPAAX_CORE_TRACE("World::PopScene — '{}' entered.", m_Scenes.back()->GetName());
            m_Scenes.back()->OnEnter();
        }
    }

    Scene* World::ReplaceScene(UniquePtr<Scene> InScene)
    {
        OPAAX_CORE_ASSERT(InScene != nullptr)

        if (!m_Scenes.empty())
        {
            OPAAX_CORE_TRACE("World::ReplaceScene — unloading '{}'.", m_Scenes.back()->GetName());
            m_Scenes.back()->OnExit();
            m_Scenes.back()->OnUnload();
            m_Scenes.pop_back();
        }

        InScene->SetSceneID(AllocateSceneID());
        OPAAX_CORE_TRACE("World::ReplaceScene — loading '{}' (SceneID={}).",
            InScene->GetName(), InScene->GetSceneID());

        InScene->OnLoad();
        InScene->OnEnter();

        m_Scenes.push_back(std::move(InScene));
        return m_Scenes.back().get();
    }

    Scene* World::GetActiveScene() noexcept
    {
        return m_Scenes.empty() ? nullptr : m_Scenes.back().get();
    }

    const Scene* World::GetActiveScene() const noexcept
    {
        return m_Scenes.empty() ? nullptr : m_Scenes.back().get();
    }

    Uint32 World::GetSceneCount() const noexcept
    {
        return static_cast<Uint32>(m_Scenes.size());
    }

    void World::DestroyEntitiesWithSceneID(Uint32 InSceneID)
    {
        // Snapshot before destroying — mutating during a view iteration is UB.
        TDynArray<EntityID> lDoomed;
        auto lView = m_Registry.view<const ECS::SceneIDComponent>();
        for (auto lEnt : lView)
        {
            if (lView.get<const ECS::SceneIDComponent>(lEnt).SceneID == InSceneID)
            {
                lDoomed.push_back(lEnt);
            }
        }

        for (EntityID lEnt : lDoomed)
        {
            DestroyEntity(lEnt, true);
        }

        OPAAX_CORE_TRACE("World::DestroyEntitiesWithSceneID — destroyed {} entity(ies) for SceneID={}.",
            lDoomed.size(), InSceneID);
    }
} // namespace Opaax
