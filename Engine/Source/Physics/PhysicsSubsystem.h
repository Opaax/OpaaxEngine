#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Physics/IPhysicsWorld.h"
#include "Physics/PhysicsTypes.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Physics);

    class World;
    struct WorldContext;
    struct ColliderComponent;
    struct RigidbodyComponent;
    struct TransformComponent;

    // =============================================================================
    // PhysicsSubsystem — owns the world's IPhysicsWorld and steps it at the fixed timestep.
    //   Play worlds only: a Play copy gets its own physics world, destroyed with it.
    //   Bodies are reconciled every step: dead ones removed, missing ones created (so entities
    //   spawned at any time work, and component add order does not matter).
    //   One body per collider; a rigidbody without a collider is skipped.
    // =============================================================================
    class PhysicsSubsystem final : public WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(PhysicsSubsystem)

        /** Play worlds only (it moves authored data). */
        static bool ShouldCreate(const World& InWorld);

        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        explicit PhysicsSubsystem(WorldContext& InContext) : m_Context(&InContext) {}

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin ISubsystem interface
        bool Startup() override;
        void FixedUpdate(double InFixedDeltaTime) override;
        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Queries (results as entities)
        // =============================================================================
    public:
        /**
         * Closest hit, with the entity. Entity is ENTITY_NONE when bHit is false.
         */
        struct RaycastHit
        {
            bool     bHit     = false;
            EntityID Entity   = ENTITY_NONE;
            Vector2F Point    = { 0.f, 0.f };
            Vector2F Normal   = { 0.f, 0.f };
            float    Fraction = 0.f;
        };

        /**
         * Closest hit from InOrigin along InDirection, up to InDistance world units.
         * @param InChannelMask Hittable channels (combine CategoryBit(channel)). Default: all.
         * @return An empty result when no physics world exists (not playing)
         */
        RaycastHit RayCast(Vector2F InOrigin, Vector2F InDirection, float InDistance,
                           Uint64 InChannelMask = ~0ull);

        /**
         * Every entity whose collider overlaps the box [InMin..InMax], filtered by InChannelMask.
         * OutEntities is cleared first; hits without an entity are dropped.
         */
        void OverlapAABB(Vector2F InMin, Vector2F InMax, TDynArray<EntityID>& OutEntities,
                         Uint64 InChannelMask = ~0ull);

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** The physics world, or null if the backend failed to create one. */
        IPhysicsWorld* GetPhysicsWorld() const noexcept { return m_World.get(); }

        /** Number of bodies. */
        Uint64 GetBodyCount() const noexcept { return static_cast<Uint64>(m_Bodies.size()); }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Removes bodies whose entity is gone. */
        void ReconcileDeadBodies(World& InWorld);

        /**
         * Creates a body for every collider without one, and rebuilds those whose type changed.
         */
        void ReconcileLiveBodies(World& InWorld);

        /** Creates one entity's body and shape. Does nothing if it already has one or has no Transform. */
        void BuildBodyForEntity(World& InWorld, EntityID InEntity, const ColliderComponent& InCollider,
                                const TransformComponent& InTransform);

        /** Destroys one entity's body. */
        void RemoveBodyForEntity(EntityID InEntity);

        /** Writes each dynamic body's pose back into its TransformComponent. */
        void SyncDynamicTransforms(World& InWorld);

        /**
         * Publishes this step's contact and overlap events: Began, then Ended, then Stayed
         * (so a pair that ended this step gets no Stayed).
         */
        void DispatchPhysicsEvents(World& InWorld);

        /** Order-independent key for a pair: (A,B) and (B,A) are the same. */
        static Uint64 PairKey(Uint64 InEntityBitsA, Uint64 InEntityBitsB) noexcept;

        /**
         * World bounds check, last in the step. Reports once per exit.
         */
        void EnforceWorldBounds(World& InWorld);

        /** The body type for an entity: its Rigidbody's, or Static. */
        static EBodyType ResolveBodyType(const RigidbodyComponent* InRigidbody) noexcept;

        /** ColliderComponent -> backend shape description. */
        static ShapeDesc MakeShapeDesc(const ColliderComponent& InCollider);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // owned by the World

        TUniquePtr<IPhysicsWorld> m_World;
        int                       m_SubStepCount = 4;

        /**
         * One body per entity. BuiltType is the type it was created with (rebuilt if it changes).
         */
        struct BodyRecord
        {
            BodyHandle Handle;
            EBodyType  BuiltType        = EBodyType::Static;
            bool       bSyncToTransform = false;
        };

        /** Keyed by entity bits. */
        TUnorderedMap<Uint32, BodyRecord> m_Bodies;

        /** Reused every step. */
        TDynArray<Uint32> m_DeadBodyVictims;

        /** Backend events, reused every step. */
        TDynArray<PhysicsContactPair> m_SensorBegan;
        TDynArray<PhysicsContactPair> m_SensorEnded;
        TDynArray<PhysicsContactPair> m_ContactBegan;
        TDynArray<PhysicsContactPair> m_ContactEnded;

        /**
         * Pairs currently overlapping, keyed by the order-independent pair. The value keeps the
         * (sensor, visitor) order, so Stayed matches Began. Needed because the backend only reports
         * begin/end.
         */
        TUnorderedMap<Uint64, PhysicsContactPair> m_LiveOverlaps;

        /** Reused: pairs whose entity died. */
        TDynArray<Uint64> m_StaleOverlaps;

        // ---- world bounds, read from config at Startup -------------------------------------------
        bool                 m_bWorldBoundsEnabled = false;
        Vector2F             m_WorldBoundsMin      = { 0.f, 0.f };
        Vector2F             m_WorldBoundsMax      = { 0.f, 0.f };
        EWorldBoundsResponse m_WorldBoundsResponse = EWorldBoundsResponse::EventAndDestroy;

        /**
         * Entities currently outside the bounds, so the event fires once per exit.
         */
        TUnorderedSet<Uint32> m_OutOfBounds;

        /** Reused: entities to remove this step. */
        TDynArray<Uint32> m_BoundsVictims;

        /** Reused by OverlapAABB. */
        TDynArray<Uint64> m_QueryScratch;
    };
}
