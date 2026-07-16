#pragma once

#include <entt/entt.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/Application/Services/ILogger.h"
#include "Core/World/Guid.h"
#include "Core/World/EntityTypes.h"
#include "Core/World/GuidRegistry.h"

namespace Opaax
{
    class Entity;

    inline constexpr LogCategory LogWorld{"World"};

    // =============================================================================
    // World — a runtime simulation container and the ECS boundary. Owns its
    //   entt::registry (the source of truth), a per-world GuidRegistry mapping stable
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
        // Entity lifecycle
        // =========================================================================
    public:
        // Create an entity stamped with a fresh Guid + EntityMeta, registered in the
        // GuidRegistry. Out-of-line (returns a complete Entity — see Entity.h).
        Entity CreateEntity(OpaaxString InName = "Entity");

        void   DestroyEntity(EntityID InEntity);

        bool   IsValid(EntityID InEntity) const noexcept { return m_Registry.valid(InEntity); }

        // Resolve a stable Guid to its Entity, or an invalid Entity if unknown.
        Entity FindByGuid(const Guid& InGuid);

        void   Clear() noexcept;

        // =========================================================================
        // Iteration — typed views (raw entt stays World-internal)
        // =========================================================================
    public:
        template<typename T, typename TFunc>
        void Each(TFunc&& InFunc) { m_Registry.view<T>().each(std::forward<TFunc>(InFunc)); }

        template<typename A, typename B, typename TFunc>
        void Each(TFunc&& InFunc) { m_Registry.view<A, B>().each(std::forward<TFunc>(InFunc)); }

        // =========================================================================
        // Get - Set
        // =========================================================================
    public:
        const Guid&        GetId() const noexcept          { return m_Id; }
        const OpaaxString& GetName() const noexcept        { return m_Name; }
        Uint64             GetEntityCount() const noexcept { return m_EntityCount; }

        // Raw registry — for the Entity wrapper + advanced World-layer use only.
        entt::registry&       GetRegistry() noexcept       { return m_Registry; }
        const entt::registry& GetRegistry() const noexcept { return m_Registry; }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        entt::registry m_Registry;
        GuidRegistry   m_Guids;
        Guid           m_Id;
        OpaaxString    m_Name;
        Uint64         m_EntityCount = 0;
    };
}
