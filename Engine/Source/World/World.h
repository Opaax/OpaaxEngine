#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"
#include "Application/WorldSpec.h"   // EWorldMode
#include "World/WorldGuidRegistry.h"

#include "Core/GUID/Guid.h"
#include "Renderer/CameraView.h"   // held BY VALUE — full type, not a forward decl
#include "Systems/WorldSubsystem.h"

#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldContext.h"

namespace Opaax
{
    class Entity;
    class Level;

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

        /**
         * Whether this is the world WorldManager currently considers active.
         *
         * Worlds COEXIST — a PIE clone beside the edit world, and briefly two Play worlds during
         * a level swap, since OpenLevel creates the new one before destroying the old. Only the
         * active one ticks, so anything that reacts OUTSIDE the tick has to ask.
         *
         * The concrete reader is a world subsystem bound to input: bindings live on the
         * GameInstance and outlive any single world, so a callback reaches every listener
         * regardless of which world is active (§IM).
         */
        bool IsActive() const noexcept { return m_bActive; }
        
        /***/
        void Clear() noexcept;
        
        // End World Lifetime
        // =========================================================================

        // =========================================================================
        // Revision
    public:
        /**
         * Bumped whenever this world's content MIGHT have changed. Monotonic, never reset.
         *
         * A GATE, not a dirty flag: it says when an expensive derived answer is worth
         * recomputing, never what that answer is (**MP5**). The editor's dirty check compares it
         * against the value it last checked at, and skips a whole capture + serialize when it has
         * not moved. Over-reporting is harmless — it costs one extra check.
         */
        Uint64 GetRevision() const noexcept { return m_Revision; }

        /**
         * Say that content changed through a path the world cannot see.
         *
         * Entity create/destroy bumps on its own. This is for a component MUTATED in place: entt
         * stores components by value and the Inspector's drawers write straight through a
         * TComponent&, so no chokepoint here observes the write.
         */
        void MarkChanged() noexcept { ++m_Revision; }

        // End Revision
        // =========================================================================

        // =========================================================================
        // Level
    public:
        /**
         * Install this world's Level — WHICH maps are in it, and the only thing that mounts or
         * unmounts one (**WM1**: this World owns the registry, its Level composes the Maps).
         *
         * Set once by WorldManager::CreateWorld, and NULL for a bare world in a test, exactly as
         * the context is: a Level needs the ComponentRegistry and IPaths that only a real engine
         * has, and a test that just wants entities should not have to supply them.
         */
        void SetLevel(TUniquePtr<Level> InLevel);

        /** This world's Level, or null if none was installed (a bare world in a test). */
        Level* GetLevel() const noexcept { return m_Level.get(); }

        // End Level
        // =========================================================================

        // =========================================================================
        // Camera
    public:
        /**
         * How this world is being looked at. RendererManager reads it every frame and turns it
         * into the pass's RenderView; whoever PRODUCES it writes it here.
         *
         * The slot is what keeps the two producers apart without the engine naming either: an
         * engine subsystem resolves it from a CameraComponent in a Play world, and the editor's
         * own camera writes it in an Edit world. It is per-world because PIE keeps two alive and
         * each is framed differently (WM6).
         *
         * A world nobody writes to keeps the DEFAULT view — centred, 600 units tall, which is the
         * frame the engine drew before cameras existed. That is the fallback, and it costs no
         * branch anywhere (BO4c one level down: never a black frame).
         */
        const CameraView& GetCameraView() const noexcept { return m_CameraView; }
        void              SetCameraView(const CameraView& InView) noexcept { m_CameraView = InView; }

        // End Camera
        // =========================================================================

        // =========================================================================
        // Subsystems
    public:
        /**
         * Install this world's context — the references its subsystems are constructed from.
         *
         * Called ONCE by WorldManager::CreateWorld, before any subsystem is created. It is not a
         * ctor argument on purpose: World would then need engine references at every construction
         * site, including the many tests that only want a bare world.
         */
        void SetContext(const WorldContext& InContext);

        /** This world's context, or null if none was installed (a bare world in a test). */
        WorldContext* GetContext() const noexcept { return m_Context.get(); }

        /**
         * This world's subsystem list. WorldManager drives its lifecycle and tick; game code uses
         * it to look one up (`GetSubsystems().GetSubsystem<WaveSpawnSubsystem>()`).
         */
        WorldSubsystemMgr&       GetSubsystems()       noexcept { return m_Subsystems; }
        const WorldSubsystemMgr& GetSubsystems() const noexcept { return m_Subsystems; }

        /**
         * Shut every subsystem down, in reverse registration order. IDEMPOTENT (LC3), because it
         * is reached two ways: WorldManager::DestroyWorld calls it while every engine sibling is
         * still alive (the LC-correct moment), and ~World repeats it as the safety net for
         * WorldManager::Shutdown, which clears its worlds without going through DestroyWorld.
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

        // Heap-held so it has a STABLE address: a subsystem stores WorldContext& and must keep
        // working for the world's whole life. Holds a reference back to this World, which is safe
        // precisely because the World owns it.
        TUniquePtr<WorldContext> m_Context;

        // WHICH maps are in this world. Heap-held because a Level holds a World& back to this one
        // and cannot exist before it does. Null for a bare world (see SetLevel).
        TUniquePtr<Level>        m_Level;

        // How this world is framed. Default-constructed = the pre-camera centred view, which is
        // what makes the no-producer case a fallback rather than a special case (see GetCameraView).
        CameraView               m_CameraView;

        bool                    m_bSubsystemsShutdown = false;

        EntityRegistry m_Registry;
        WorldGuidRegistry   m_Guids;
        Guid           m_Id;
        OpaaxString    m_Name;
        EWorldMode     m_Mode = EWorldMode::Play; // const-by-convention: set in the ctor, never after
        Uint64         m_EntityCount = 0;
        Uint64         m_Revision    = 0;   // see GetRevision — a gate, not a dirty flag

        // Driven by OnActive/OnDesactive, which WorldManager calls at every transition. A world
        // that has been created but never activated is correctly false.
        bool           m_bActive     = false;
    };
}
