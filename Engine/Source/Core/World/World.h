#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/OpaaxLog.h"
#include "ECS/BaseComponents.hpp"
#include "ECS/OpaaxEntity.hpp"
#include "ECS/Components/TagComponent.h"

namespace Opaax
{
    // =============================================================================
    // Usage:
    //   EntityID lEnt = World.CreateEntity("Player");
    //   World.AddComponent<TransformComponent>(lEnt, {.Position = {100, 200}});
    //   auto* lTr = World.GetComponent<TransformComponent>(lEnt);
    //   World.DestroyEntity(lEnt);
    // =============================================================================

    /**
     * @class World
     *
     * Owner of all entities and components.
     * One World = one game scene (or editor scene).
     *
     * World does not tick systems — that is the responsibility of the caller (CoreEngineApp or a future SceneManager).
     * World is pure data.
     */
    class OPAAX_API World
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        World()  = default;
        ~World() = default;

        // =============================================================================
        // Copy - Delete
        // =============================================================================
        World(const World&)            = delete;
        World& operator=(const World&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        World(World&&)                 = default;
        World& operator=(World&&)      = default;

        // =============================================================================
        // Function 
        // =============================================================================

        //------------------------------------------------------------------------------
        //  Entity Life cycle
    public:
        EntityID CreateEntity(const char* InTag = "Entity")
        {
            const EntityID lID = m_Registry.create();
            m_Registry.emplace<ECS::TagComponent>(lID, InTag);
            OPAAX_CORE_TRACE("World::CreateEntity '{}' — id={}", InTag, static_cast<Uint32>(lID));
            m_EntityCount.fetch_add(1, std::memory_order_relaxed);
            return lID;
        }

        void DestroyEntity(EntityID InEntity)
        {
            if (!IsValid(InEntity))
            {
                OPAAX_CORE_WARN("World::DestroyEntity — invalid entity, ignored.");
                return;
            }
            m_Registry.destroy(InEntity);
            m_EntityCount.fetch_sub(1, std::memory_order_relaxed);
        }

        bool IsValid(EntityID InEntity) const noexcept
        {
            return m_Registry.valid(InEntity);
        }

        //------------------------------------------------------------------------------
        //  API
    public:
        template<typename T, typename... Args>
        T& AddComponent(EntityID InEntity, Args&&... InArgs)
        {
            OPAAX_CORE_ASSERT(IsValid(InEntity))
            return m_Registry.emplace<T>(InEntity, std::forward<Args>(InArgs)...);
        }

        template<typename T>
        void RemoveComponent(EntityID InEntity)
        {
            OPAAX_CORE_ASSERT(IsValid(InEntity))
            m_Registry.remove<T>(InEntity);
        }

        template<typename T>
        T* GetComponent(EntityID InEntity) noexcept
        {
            return m_Registry.try_get<T>(InEntity);
        }

        template<typename T>
        const T* GetComponent(EntityID InEntity) const noexcept
        {
            return m_Registry.try_get<T>(InEntity);
        }

        template<typename T>
        bool HasComponent(EntityID InEntity) const noexcept
        {
            return m_Registry.all_of<T>(InEntity);
        }
        
        //------------------------------------------------------------------------------

        // Single component — calls InFunc(EntityID, T&)
        template<typename T, typename TFunc>
        void Each(TFunc&& InFunc)
        {
            m_Registry.view<T>().each(std::forward<TFunc>(InFunc));
        }

        // Two components — calls InFunc(EntityID, A&, B&)
        template<typename A, typename B, typename TFunc>
        void Each(TFunc&& InFunc)
        {
            m_Registry.view<A, B>().each(std::forward<TFunc>(InFunc));
        }

        // Three components — calls InFunc(EntityID, A&, B&, C&)
        template<typename A, typename B, typename C, typename TFunc>
        void Each(TFunc&& InFunc)
        {
            m_Registry.view<A, B, C>().each(std::forward<TFunc>(InFunc));
        }

        // Raw registry access — for advanced use (editor, serializer).
        // NOTE: Prefer the typed API above for game code.
        entt::registry& GetRegistry() noexcept { return m_Registry; }
        const entt::registry& GetRegistry() const noexcept { return m_Registry; }
        
        //------------------------------------------------------------------------------
        // Get - Set
    public:
        Uint32 GetEntityCount() const noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        entt::registry m_Registry;
        Atomic<Uint32> m_EntityCount{ 0 };
    };

} // namespace Opaax::ECS