#pragma once

#include "Application/Services/ILogger.h"
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
    // PhysicsSubsystem — owns the world's IPhysicsWorld and advances it at the fixed timestep.
    //
    //   PLAY WORLDS ONLY, and that single line replaces the whole PIE apparatus M9 needed. As a
    //   WORLD subsystem its physics world's lifetime IS the world's: a PIE clone gets a fresh
    //   one on Play and takes it with it on Stop, so there is nothing to tear down between
    //   sessions and no state can leak across one. M9's OnPlayBegin / OnPlayEnd / ClearBodies
    //   existed only because physics lived one tier too high; none of them came back.
    //   WorldManager already forwards FixedUpdate to the ACTIVE world only (WS5) and already
    //   resolves pause/step once per frame (WS8), so this holds no gate of its own.
    //
    //   BODIES ARE RECONCILED, NEVER BUILT ONCE. Every fixed step it reaps bodies whose entity
    //   is gone, then builds one for every collider that lacks one. That is what satisfies WS7
    //   without the post-instantiate hook WS7 named as physics' likely need: entities spawned by
    //   the host after Startup, by a clone's Instantiate, or by gameplay mid-play all arrive the
    //   same way, and add-order stops mattering (a Rigidbody added AFTER a Collider rebuilds the
    //   body rather than leaving it static forever).
    //
    //   ONE BODY PER COLLIDER — a rigidbody without one is skipped, since a body with no shape
    //   is something nothing can touch and gravity moves invisibly.
    // =============================================================================
    class OPAAX_API PhysicsSubsystem final : public WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(PhysicsSubsystem)

        /** Gameplay: Play worlds only. In an Edit world it must not exist — it MOVES authored data. */
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
        // Queries — the seam answers in USER-DATA; these answer in entities
        // =============================================================================
    public:
        /**
         * @struct RaycastHit
         *
         * A closest-hit result resolved to the engine's own vocabulary. The seam's PhysicsRayHit
         * carries raw body user-data, which is meaningless to a caller; this is the same answer
         * with the entity decoded. Entity is ENTITY_NONE whenever bHit is false.
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
         * Closest hit from InOrigin along InDirection for InDistance world units.
         *
         * @param InChannelMask which channels are hittable — combine `CategoryBit(channel)` values.
         *   The default hits everything.
         * @return an empty result when there is no physics world (nothing is playing), which is a
         *   state rather than an error: a query before Play is a legitimate thing to ask.
         */
        RaycastHit RayCast(Vector2F InOrigin, Vector2F InDirection, float InDistance,
                           Uint64 InChannelMask = ~0ull);

        /**
         * Every entity whose collider overlaps the world-space box [InMin..InMax], filtered by
         * InChannelMask. OutEntities is cleared first, and unresolvable hits are dropped rather
         * than reported as ENTITY_NONE — a caller iterating the result never has to check.
         */
        void OverlapAABB(Vector2F InMin, Vector2F InMax, TDynArray<EntityID>& OutEntities,
                         Uint64 InChannelMask = ~0ull);

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** The live world, or null when the backend refused to build one. */
        IPhysicsWorld* GetPhysicsWorld() const noexcept { return m_World.get(); }

        /** How many bodies exist right now — the number the logs and the tests both assert on. */
        Uint64 GetBodyCount() const noexcept { return static_cast<Uint64>(m_Bodies.size()); }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** Reap bodies whose entity no longer exists, so a dead one cannot emit contacts. */
        void ReconcileDeadBodies(World& InWorld);

        /**
         * Build a body for every collider that lacks one, and REBUILD any whose desired type no
         * longer matches what was built — which is what makes component add-order irrelevant.
         */
        void ReconcileLiveBodies(World& InWorld);

        /** Build one entity's body + shape. No-op if it already has one or has no Transform. */
        void BuildBodyForEntity(World& InWorld, EntityID InEntity, const ColliderComponent& InCollider,
                                const TransformComponent& InTransform);

        /** Destroy one entity's body through the seam and forget it. */
        void RemoveBodyForEntity(EntityID InEntity);

        /** Write each dynamic body's post-step pose back into its TransformComponent. */
        void SyncDynamicTransforms(World& InWorld);

        /**
         * Drain the backend's touch edges and publish them, once per step, after Step.
         *
         * Began then Ended then Stayed, in that order and for a reason: the live-overlap set has
         * to reflect this step's edges BEFORE the survivors are ticked, or a pair that ended this
         * very step would get one more Stayed after its Ended.
         */
        void DispatchPhysicsEvents(World& InWorld);

        /** A normalized key for an unordered pair, so (A,B) and (B,A) are one entry. */
        static Uint64 PairKey(Uint64 InEntityBitsA, Uint64 InEntityBitsB) noexcept;

        /**
         * The kill volume, run LAST in the step so this step's contacts and overlaps have already
         * fired before anything is reaped. Latched, so a body that keeps falling reports once.
         */
        void EnforceWorldBounds(World& InWorld);

        /** The body type an entity implies: its Rigidbody's, or Static when it has none. */
        static EBodyType ResolveBodyType(const RigidbodyComponent* InRigidbody) noexcept;

        /** ColliderComponent -> the neutral shape the seam takes. */
        static ShapeDesc MakeShapeDesc(const ColliderComponent& InCollider);

        /**
         * Say ONCE that the simulation actually MOVED something.
         *
         * "Simulating N body/bodies" is a count, and it prints the same for N frozen bodies as for
         * N falling ones — the [[L76]] trap, in the shape SpriteAnimationSubsystem::NoteStepApplied
         * already exists to avoid. So this watches ONE body against the pose it was built at and
         * reports the distance it has travelled, which nothing but a live solver can produce.
         */
        void NoteBodyMoved(EntityID InEntity, const Vector2F& InPosition);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        TUniquePtr<IPhysicsWorld> m_World;
        int                       m_SubStepCount = 4;

        /**
         * One live body per entity. BuiltType is what it was CREATED as: when the entity's
         * components later imply a different one, the reconcile rebuilds instead of leaving a
         * stale static body that gravity will never touch.
         */
        struct BodyRecord
        {
            BodyHandle Handle;
            EBodyType  BuiltType        = EBodyType::Static;
            bool       bSyncToTransform = false;
        };

        /** Keyed by entity bits, so a record survives nothing but its own entity. */
        TUnorderedMap<Uint32, BodyRecord> m_Bodies;

        /** Scratch, reused every step: never mutate m_Bodies mid-iteration. */
        TDynArray<Uint32> m_DeadBodyVictims;

        /** Drained from the backend each step. Members so a step allocates nothing. */
        TDynArray<PhysicsContactPair> m_SensorBegan;
        TDynArray<PhysicsContactPair> m_SensorEnded;
        TDynArray<PhysicsContactPair> m_ContactBegan;
        TDynArray<PhysicsContactPair> m_ContactEnded;

        /**
         * Pairs currently overlapping, keyed by the NORMALIZED pair so (A,B) and (B,A) collapse
         * to one entry. The value keeps the ordered (sensor, visitor) pair, so a Stayed re-fires
         * with the same sensor-first meaning its Began had.
         *
         * This map is the only reason a Stayed event can exist: the backend reports edges, and a
         * state has to be remembered between them.
         */
        TUnorderedMap<Uint64, PhysicsContactPair> m_LiveOverlaps;

        /** Scratch for pairs whose entity died — same never-mutate-mid-iteration rule. */
        TDynArray<Uint64> m_StaleOverlaps;

        /** Counted for the one-shot log, so "events are flowing" is a NUMBER, not a claim. */
        Uint64 m_OverlapEventCount   = 0;
        Uint64 m_CollisionEventCount = 0;
        bool   m_bLoggedFirstTouch   = false;

        // ---- world bounds, read from config at Startup --------------------------------------
        bool                 m_bWorldBoundsEnabled = false;
        Vector2F             m_WorldBoundsMin      = { 0.f, 0.f };
        Vector2F             m_WorldBoundsMax      = { 0.f, 0.f };
        EWorldBoundsResponse m_WorldBoundsResponse = EWorldBoundsResponse::EventAndDestroy;

        /**
         * Entity bits currently OUTSIDE the bounds, so the event fires on the transition rather
         * than every step a body keeps falling. An entity that comes back in is removed, so
         * re-entering and leaving again reports twice — which is the honest answer.
         */
        TUnorderedSet<Uint32> m_OutOfBounds;

        /** Scratch for entities to reap this step — collected during the walk, destroyed after. */
        TDynArray<Uint32> m_BoundsVictims;

        /** One-shot: the feature being CONFIGURED and anything having left are separate claims. */
        bool m_bLoggedFirstExit = false;

        /** Reused by OverlapAABB so a per-frame query allocates nothing. */
        TDynArray<Uint64> m_QueryScratch;

        /** One-shot log flags — a fixed step must not print sixty lines a second ([[L15]]). */
        bool   m_bLoggedFirstStep = false;
        Uint64 m_LastBuiltCount   = 0;

        /** The one body NoteBodyMoved watches, and the pose it was BUILT at. */
        EntityID m_ProbeEntity   = ENTITY_NONE;
        Vector2F m_ProbeOrigin   = { 0.f, 0.f };
        bool     m_bLoggedMotion = false;
    };
}
