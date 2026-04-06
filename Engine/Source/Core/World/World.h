#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Log/OpaaxLog.h"
#include "ECS/ComponentStorage.hpp"
#include "ECS/ComponentTypeID.h"

namespace Opaax::ECS
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
        /**
         * Creates a new entity with a TagComponent.
         * @param InTag 
         * @return ENTITY_NONE if the entity pool is exhausted (should never happen in practice).
         */
        EntityID CreateEntity(const char* InTag = "Entity");

        /**
         * Destroys an entity and removes all its components.
         * After this call, InEntity is invalid — any stored copy will fail IsValid() checks.
         * @param InEntity 
         */
        void DestroyEntity(EntityID InEntity);
        
        /**
         * 
         * @param InEntity 
         * @return True if the entity was created by this world and has not been destroyed.
         */
        bool IsValid(EntityID InEntity) const noexcept;

        //------------------------------------------------------------------------------

        //------------------------------------------------------------------------------
        // API
    public:
        template<typename T, typename... Args>
        T& AddComponent(EntityID InEntity, Args&&... InArgs)
        {
            OPAAX_CORE_ASSERT(IsValid(InEntity))
            return GetStorage<T>().Add(InEntity, std::forward<Args>(InArgs)...);
        }

        template<typename T>
        void RemoveComponent(EntityID InEntity)
        {
            OPAAX_CORE_ASSERT(IsValid(InEntity))
            GetStorage<T>().Remove(InEntity);
        }

        template<typename T>
        T* GetComponent(EntityID InEntity) noexcept
        {
            return GetStorage<T>().Get(InEntity);
        }

        template<typename T>
        const T* GetComponent(EntityID InEntity) const noexcept
        {
            return GetStorage<T>().Get(InEntity);
        }

        template<typename T>
        bool HasComponent(EntityID InEntity) const noexcept
        {
            return GetStorage<T>().Has(InEntity);
        }

        //------------------------------------------------------------------------------
        
        //------------------------------------------------------------------------------
        // Storage access — for systems that iterate over all components of a type
        // Usage:
        //   auto& lStorage = World.GetStorage<TransformComponent>();
        //   for (auto& lTr : lStorage.GetComponents()) { ... }

        /**
         * 
         * @tparam T 
         * @return 
         */
        template<typename T>
        ComponentStorage<T>& GetStorage()
        {
            const auto lKey = GetStorageKey<T>();
            auto lIt = m_Storages.find(lKey);
            OPAAX_CORE_ASSERT(lIt != m_Storages.end())
            if (lIt == m_Storages.end())
            {
                m_Storages.emplace(lKey, MakeUnique<StorageWrapper<T>>());
                lIt = m_Storages.find(lKey);
            }
            return static_cast<StorageWrapper<T>*>(lIt->second.get())->Storage;
        }

        /**
         * 
         * @tparam T 
         * @return 
         */
        template<typename T>
        const ComponentStorage<T>& GetStorage() const
        {
            const auto lKey = GetStorageKey<T>();
            auto lIt = m_Storages.find(lKey);
            OPAAX_CORE_ASSERT(lIt != m_Storages.end())
            if (lIt == m_Storages.end())
            {
                m_Storages.emplace(lKey, MakeUnique<StorageWrapper<T>>());
                lIt = m_Storages.find(lKey);
            }
            return static_cast<const StorageWrapper<T>*>(lIt->second.get())->Storage;
        }

        //------------------------------------------------------------------------------
        // Queries — convenience iteration helpers

    public:
        /**
         * 
         * @tparam A 
         * @tparam B 
         * @tparam TFunc 
         * @param InFunc 
         */
        template<typename A, typename B, typename TFunc>
        void Each(TFunc&& InFunc)
        {
            // Ensure both storages exist before taking any references.
            // Any rehash happens here — before refs are taken.
            EnsureStorage<A>();
            EnsureStorage<B>();

            // Map is stable now — refs are safe.
            auto& lStorageA = GetStorage<A>();
            auto& lStorageB = GetStorage<B>();

            if (lStorageA.Count() <= lStorageB.Count())
            {
                const auto& lEntities = lStorageA.GetEntities();
                auto&       lCompsA   = lStorageA.GetComponents();

                for (Uint32 i = 0; i < static_cast<Uint32>(lCompsA.size()); ++i)
                {
                    B* lB = lStorageB.Get(lEntities[i]);
                    if (lB) { InFunc(lEntities[i], lCompsA[i], *lB); }
                }
            }
            else
            {
                const auto& lEntities = lStorageB.GetEntities();
                auto&       lCompsB   = lStorageB.GetComponents();

                for (Uint32 i = 0; i < static_cast<Uint32>(lCompsB.size()); ++i)
                {
                    A* lA = lStorageA.Get(lEntities[i]);
                    if (lA) { InFunc(lEntities[i], *lA, lCompsB[i]); }
                }
            }
        }
        
        /**
         * 
         * @tparam T 
         * @tparam TFunc 
         * @param InFunc 
         */
        template<typename T, typename TFunc>
        void Each(TFunc&& InFunc)
        {
            EnsureStorage<T>();
            auto& lStorage = GetStorage<T>();

            const auto& lEntities   = lStorage.GetEntities();
            auto&       lComponents = lStorage.GetComponents();

            for (Uint32 i = 0; i < static_cast<Uint32>(lComponents.size()); ++i)
            {
                InFunc(lEntities[i], lComponents[i]);
            }
        }

        //------------------------------------------------------------------------------
        // Get - Set
    public:
        Uint32 GetEntityCount() const noexcept { return m_EntityCount; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // -------------------------------------------------------------------------
        // Entity pool
        struct EntitySlot
        {
            Uint32 Generation = 0;
            bool   bAlive     = false;
        };

        TDynArray<EntitySlot>  m_EntitySlots;   // indexed by entity index
        TDynArray<Uint32>      m_FreeList;       // recycled indices
        Uint32                 m_EntityCount = 0;

        // -------------------------------------------------------------------------
        // Storage map — type-erased
        //
        // We avoid RTTI by using a function pointer address as the type key.
        //   GetStorageKey<T>() returns the address of a static local — unique per T,
        //   stable across TUs, zero RTTI. Same pattern as OPAAX_SUBSYSTEM_TYPE.

    private:
        struct IStorageBase
        {
            virtual ~IStorageBase() = default;
        };

        template<typename T>
        struct StorageWrapper final : IStorageBase
        {
            ComponentStorage<T> Storage;
        };

        using StorageKey = Uint32;

        template<typename T>
        static StorageKey GetStorageKey() noexcept
        {
            return GetComponentTypeID<T>();
        }

        UnorderedMap<StorageKey, UniquePtr<IStorageBase>> m_Storages;

    private:
        // Creates the storage slot for T if it doesn't exist yet.
        // Does NOT return a reference — call GetStorage<T>() after.
        // Separated from GetStorage to avoid taking refs across a potential rehash.
        template<typename T>
        void EnsureStorage()
        {
            const StorageKey lKey = GetStorageKey<T>();
            if (m_Storages.find(lKey) == m_Storages.end())
            {
                m_Storages.emplace(lKey, MakeUnique<StorageWrapper<T>>());
            }
        }
    };

} // namespace Opaax::ECS