#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"
#include "Application/WorldSpec.h"   // EWorldMode
#include "World/WorldGuidRegistry.h"

#include "Core/GUID/Guid.h"
#include "Systems/WorldSubsystem.h"

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
        //Todo: World should have OpaaxStringID to get ID and Name in one place
        /**
         * @param InName
         * @param InMode What this world is FOR. Defaults to Play so a bare world (a test, a
         *               game host) is runnable; the editor passes Edit explicitly. Fixed for
         *               the world's whole life — there is deliberately no setter, see EWorldMode.
         */
        explicit World(OpaaxString InName = "World", EWorldMode InMode = EWorldMode::Play);
        ~World();

        // =========================================================================
        // Copy - Move Delete
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
         * @param InOwnerMap The Map authoring this entity. Left invalid (the default) the
         *                   entity is RUNTIME-SPAWNED and filtered capture will skip it.
         * @return The created entity
         */
        Entity CreateEntity(OpaaxString InName = "Entity", MapId InOwnerMap = {});

        /**
         * Create an entity carrying a Guid that ALREADY EXISTS — the instantiate half of the
         * snapshot core. CreateEntity cannot serve this: it mints a fresh Guid, which would
         * break every inter-entity reference in the map being loaded.
         *
         * Refused (invalid Entity returned) when InGuid is invalid or already live in this
         * World — a duplicate identity would make FindByGuid answer arbitrarily.
         *
         * @param InGuid The stable identity to restore.
         * @param InName
         * @param InOwnerMap
         * @return The created entity, or an invalid Entity on refusal.
         */
        Entity CreateEntityWithGuid(const Guid& InGuid, OpaaxString InName = "Entity", MapId InOwnerMap = {});

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

        /** What this world is for. Read-only BY DESIGN — see EWorldMode for why there is no setter. */
        EWorldMode         GetMode() const noexcept        { return m_Mode; }

        // Raw registry — for the Entity wrapper + advanced World-layer use only.
        EntityRegistry&       GetRegistry() noexcept       { return m_Registry; }
        const EntityRegistry& GetRegistry() const noexcept { return m_Registry; }
        // End Get - Set
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        /**
         * Handle Subsystem lifetime
         */
        WorldSubsystemMgr m_Subsystems;
        
        EntityRegistry m_Registry;
        WorldGuidRegistry   m_Guids;
        Guid           m_Id;
        OpaaxString    m_Name;
        EWorldMode     m_Mode = EWorldMode::Play; // const-by-convention: set in the ctor, never after
        Uint64         m_EntityCount = 0;
    };
}
