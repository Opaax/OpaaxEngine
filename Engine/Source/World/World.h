#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Log/Logger.h"
#include "World/WorldSpec.h"   // EWorldMode
#include "World/WorldGuidRegistry.h"

#include "Core/GUID/Guid.h"
#include "Renderer/CameraView.h"
#include "Systems/WorldSubsystem.h"

#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldContext.h"

namespace Opaax
{
    class Entity;
    class Level;

    inline constexpr LogCategory LogWorld{"World"};

    // =============================================================================
    // World — a runtime simulation container (the ECS boundary). Owns its EntityRegistry,
    //   a Guid -> entity lookup, its subsystems, its Level and its camera view.
    //   Game code reaches entities through Entity handles.
    // =============================================================================
    class World
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        //Todo: World should have OpaaxStringID to get ID and Name in one place
        /**
         * @param InMode What the world is for. Defaults to Play (tests, game hosts); the editor
         *               passes Edit. Cannot change afterwards.
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

        // =========================================================================
        // Entity
    public:
        /**
         * Creates an entity with a new Guid and an EntityMeta.
         * @param InOwnerMap The map this entity belongs to. Invalid (default) = runtime-spawned (not saved).
         * @return The created entity
         */
        Entity CreateEntity(OpaaxString InName = "Entity", MapId InOwnerMap = {});

        /**
         * Creates an entity with an existing Guid (loading a map; keeps references valid).
         * Refused if InGuid is invalid or already used in this World.
         * @return The created entity, or an invalid Entity if refused
         */
        Entity CreateEntityWithGuid(const Guid& InGuid, OpaaxString InName = "Entity", MapId InOwnerMap = {});

        /**
         * Destroys the entity and its children.
         */
        void   DestroyEntity(Entity InEntity);

        /**
         * Destroys the entity and its children.
         */
        void   DestroyEntity(EntityID InEntity);

        /**
         * @return True if the entity is valid
         */
        bool   IsValid(EntityID InEntity) const noexcept { return m_Registry.valid(InEntity); }
        
        /**
         * @return The entity with InGuid, or an invalid Entity if unknown
         */
        Entity FindByGuid(const Guid& InGuid);
        
        // End Entity
        // =========================================================================

        // =========================================================================
        // World Lifetime
        
        void OnActive();

        void OnDesactive();

        /**
         * Whether this is WorldManager's active world. Several worlds can exist (Play copy beside the
         * edit world, two Play worlds during a level change); only the active one ticks.
         */
        bool IsActive() const noexcept { return m_bActive; }
        
        void Clear() noexcept;
        
        // End World Lifetime
        // =========================================================================

        // =========================================================================
        // Revision
    public:
        /**
         * Incremented whenever the content may have changed. Never reset.
         * Used by the editor's dirty check to skip work when nothing moved.
         */
        Uint64 GetRevision() const noexcept { return m_Revision; }

        /**
         * Signals a change the world cannot see (a component edited in place).
         * Creating and destroying entities already bump the revision.
         */
        void MarkChanged() noexcept { ++m_Revision; }

        // End Revision
        // =========================================================================

        // =========================================================================
        // Level
    public:
        /**
         * Sets this world's Level (which maps it contains). Set once by WorldManager::CreateWorld;
         * null for a bare test world.
         */
        void SetLevel(TUniquePtr<Level> InLevel);

        /** This world's Level, or null (bare test world). */
        Level* GetLevel() const noexcept { return m_Level.get(); }

        // End Level
        // =========================================================================

        // =========================================================================
        // Camera
    public:
        /**
         * How this world is viewed; RendererManager reads it every frame.
         * Set by CameraManager in Play worlds and by the editor camera in Edit worlds.
         * Default: centred, 600 units tall.
         */
        const CameraView& GetCameraView() const noexcept { return m_CameraView; }
        void              SetCameraView(const CameraView& InView) noexcept { m_CameraView = InView; }

        // End Camera
        // =========================================================================

        // =========================================================================
        // Subsystems
    public:
        /**
         * Sets the context the subsystems are built with. Called once by WorldManager::CreateWorld,
         * before any subsystem is created.
         */
        void SetContext(const WorldContext& InContext);

        /** This world's context, or null (bare test world). */
        WorldContext* GetContext() const noexcept { return m_Context.get(); }

        /**
         * This world's subsystems (GetSubsystems().GetSubsystem<WaveSpawnSubsystem>()).
         */
        WorldSubsystemMgr&       GetSubsystems()       noexcept { return m_Subsystems; }
        const WorldSubsystemMgr& GetSubsystems() const noexcept { return m_Subsystems; }

        /**
         * Shuts every subsystem down, in reverse order. Safe to call twice (DestroyWorld, then ~World).
         */
        void ShutdownSubsystems();

        // End Subsystems
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

        /** What this world is for. Cannot change. */
        EWorldMode         GetMode() const noexcept        { return m_Mode; }

        // Raw registry — for Entity and World-layer code only.
        EntityRegistry&       GetRegistry() noexcept       { return m_Registry; }
        const EntityRegistry& GetRegistry() const noexcept { return m_Registry; }
        // End Get - Set
        // =========================================================================

        // =========================================================================
        // Members
        // =========================================================================
    private:
        WorldSubsystemMgr m_Subsystems;

        // On the heap, so subsystems can keep a WorldContext& for the world's lifetime.
        TUniquePtr<WorldContext> m_Context;

        // Which maps are in this world. Null for a bare world (see SetLevel).
        TUniquePtr<Level>        m_Level;

        // Default: the centred view (see GetCameraView).
        CameraView               m_CameraView;

        bool                    m_bSubsystemsShutdown = false;

        EntityRegistry m_Registry;
        WorldGuidRegistry   m_Guids;
        Guid           m_Id;
        OpaaxString    m_Name;
        EWorldMode     m_Mode = EWorldMode::Play; // set in the ctor only
        Uint64         m_EntityCount = 0;
        Uint64         m_Revision    = 0;   // see GetRevision

        // Set by OnActive/OnDesactive, called by WorldManager.
        bool           m_bActive     = false;
    };
}
