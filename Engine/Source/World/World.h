#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Application/Services/ILogger.h"
#include "World/WorldGuidRegistry.h"

#include "GUID/Guid.h"

#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    class Entity;

    inline constexpr LogCategory LogWorld{"World"};

    // =============================================================================
    // World — a runtime simulation container and the ECS boundary. Owns its
    //   EntityRegistry (the source of truth), a per-world GuidRegistry mapping stable
    //   Guids to runtime entities, and its own identity. Raw entt access stays inside
    //   the World layer; game code reaches entities through Entity handles.
    // =============================================================================
    class OPAAX_API World
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        explicit World(OpaaxString InName = "World");
        ~World();

        // =========================================================================
        // Copy - Move Delete  (owned via UniquePtr in WorldManager; never copied/moved)
        // =========================================================================
        World(const World&)            = delete;
        World& operator=(const World&) = delete;
        World(World&&)                 = delete;
        World& operator=(World&&)      = delete;
        
        // =========================================================================
        // Functions
        // =========================================================================
        
    private:
        void AddEntityCount();
        void RemoveEntityCount();
        void LogEntityCount();

        // =========================================================================
        // Entity
    public:
        /**
         * Create an entity stamped with a fresh Guid + EntityMeta.
         * Registered in this GuidRegistry.
         * 
         * @param InName 
         * @return The created entity
         */
        Entity CreateEntity(OpaaxString InName = "Entity");
        
        /**
         * @param InEntity The ID of the Entity to destroy
         */
        void   DestroyEntity(Entity InEntity);

        /**
         * @param InEntity The ID of the Entity to destroy
         */
        void   DestroyEntity(EntityID InEntity);

        /**
         * @param InEntity The entity to check
         * @return True if the entity is valid
         */
        bool   IsValid(EntityID InEntity) const noexcept { return m_Registry.valid(InEntity); }
        
        /**
         * Resolve a stable Guid to its Entity
         * @param InGuid The Guid to find
         * @return The entity if found or an invalid Entity if unknown.
         */
        Entity FindByGuid(const Guid& InGuid);
        
        // End Entity
        // =========================================================================

        // =========================================================================
        // World Lifetime
        
        /***/
        void OnActive();
        
        /***/
        void OnDesactive();
        
        /***/
        void Clear() noexcept;
        
        // End World Lifetime
        // =========================================================================

        // =========================================================================
        // Iteration
    public:
        template<typename T, typename TFunc>
        void Each(TFunc&& InFunc) { m_Registry.view<T>().each(std::forward<TFunc>(InFunc)); }

        template<typename A, typename B, typename TFunc>
        void Each(TFunc&& InFunc) { m_Registry.view<A, B>().each(std::forward<TFunc>(InFunc)); }
        
        // End Iteration
        // =========================================================================

        // =========================================================================
        // Get - Set
    public:
        const Guid&        GetId() const noexcept          { return m_Id; }
        const OpaaxString& GetName() const noexcept        { return m_Name; }
        Uint64             GetEntityCount() const noexcept { return m_EntityCount; }

        // Raw registry — for the Entity wrapper + advanced World-layer use only.
        EntityRegistry&       GetRegistry() noexcept       { return m_Registry; }
        const EntityRegistry& GetRegistry() const noexcept { return m_Registry; }
        // End Get - Set
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        EntityRegistry m_Registry;
        WorldGuidRegistry   m_Guids;
        Guid           m_Id;
        OpaaxString    m_Name;
        Uint64         m_EntityCount = 0;
    };
}
