#pragma once

#include "Core/Systems/EngineSubsystem.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"

#include "ECS/OpaaxEntity.hpp"
#include "Physics/IPhysicsWorld.h"
#include "Physics/PhysicsTypes.h"

namespace Opaax
{
    class World;

    // =============================================================================
    // PhysicsSubsystem
    // =============================================================================
    /**
     * @class PhysicsSubsystem
     *
     * Owns the backend-neutral IPhysicsWorld and advances it at the engine's fixed
     * timestep. Play-only: FixedUpdate is skipped while the editor is in Editing or
     * Paused, and runs every frame in release builds (gameplay always active).
     *
     * The world is process-lifetime (built in Startup, released in Shutdown). Bodies
     * churn per play session: OnPlayBegin builds them from the scene's
     * Rigidbody/Collider/Transform components, FixedUpdate steps the sim and writes
     * dynamic bodies back to their Transforms, OnPlayEnd destroys them — so no Box2D
     * state leaks across PIE Start/Stop (ECS Transforms roll back via the snapshot system).
     */
    class OPAAX_API PhysicsSubsystem final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(PhysicsSubsystem)

        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        PhysicsSubsystem() = default;
        explicit PhysicsSubsystem(CoreEngineApp* InEngineApp) : EngineSubsystemBase(InEngineApp) {}
        ~PhysicsSubsystem() override = default;

        PhysicsSubsystem(const PhysicsSubsystem&)            = delete;
        PhysicsSubsystem& operator=(const PhysicsSubsystem&) = delete;
        PhysicsSubsystem(PhysicsSubsystem&&)                 = default;
        PhysicsSubsystem& operator=(PhysicsSubsystem&&)      = default;

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        IPhysicsWorld* GetWorld() noexcept { return m_World.get(); }

        // =============================================================================
        // Queries (resolve the seam's raw user-data to EntityID)
        // =============================================================================
    public:
        // Closest-hit result of a ray cast. Entity is ENTITY_NONE when bHit is false.
        struct RaycastResult
        {
            bool     bHit     = false;
            EntityID Entity   = ENTITY_NONE;
            Vector2F Point    = { 0.f, 0.f };
            Vector2F Normal   = { 0.f, 0.f };
            float    Fraction = 0.f;
        };

        // Cast a ray from Origin along Direction for Distance world units; ChannelMask selects which
        // channels are hittable (~0 = all). Empty result if no world (not playing).
        RaycastResult RayCast(Vector2F Origin, Vector2F Direction, float Distance, Uint64 ChannelMask = ~0ull);

        // Collect every entity whose collider overlaps the world-space AABB [Min..Max] into Out
        // (cleared first), filtered by ChannelMask. No-op if no world (not playing).
        void OverlapAABB(Vector2F Min, Vector2F Max, TDynArray<EntityID>& Out, Uint64 ChannelMask = ~0ull);

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup()                       override;
        void FixedUpdate(double FixedDeltaTime) override;
        void Shutdown()                      override;

        void OnPlayBegin()                   override;
        void OnPlayEnd()                     override;

        bool IsPlayOnly() const noexcept override { return true; }
        //~End EngineSubsystemBase Interface

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        // Build one body (+ its shape) per entity carrying a ColliderComponent, reading
        // an optional RigidbodyComponent (absent => implicit static) and the Transform.
        void BuildBodies(World& InWorld);

        // Destroy every body built for the current play session and clear the map.
        void ClearBodies();

        // Write each dynamic body's post-step transform back to its ECS TransformComponent.
        void SyncDynamicTransforms(World& InWorld);

        // Drain the world's sensor + contact events after a step and fan them out as engine
        // events: overlap Start/Tick/Stop (Tick synthesized from the live overlap set) and
        // collision Enter/Exit. Called once per FixedUpdate after Step.
        void DispatchPhysicsEvents();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // One live body per entity. bSyncToTransform marks dynamic bodies (the only ones
        // that move on their own and need their Transform written back each step).
        struct BodyRecord
        {
            BodyHandle Handle;
            bool       bSyncToTransform = false;
        };

        UniquePtr<IPhysicsWorld>                m_World;
        PhysicsWorldDesc                        m_WorldDesc;

        // Keyed by entity bits (static_cast<Uint32>(EntityID)); reconstructed on sync.
        UnorderedMap<Uint32, BodyRecord>  m_Bodies;

        // Reusable scratch buffers drained from the world each step (no per-step allocation).
        TDynArray<PhysicsContactPair>         m_SensorBegan;
        TDynArray<PhysicsContactPair>         m_SensorEnded;
        TDynArray<PhysicsContactPair>         m_ContactBegan;
        TDynArray<PhysicsContactPair>         m_ContactEnded;

        // Currently-overlapping sensor pairs, keyed by the normalized pair (min<<32 | max of the
        // two entity bits) so (A,B) and (B,A) collapse to one entry. Value keeps the ordered
        // (sensor, visitor) pair so Tick re-fires with sensor-first semantics. Drives OnOverlapTick.
        UnorderedMap<Uint64, PhysicsContactPair> m_LiveOverlaps;
    };

} // namespace Opaax
