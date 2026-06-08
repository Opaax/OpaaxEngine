#pragma once

#include <unordered_map>
#include <vector>

#include "Core/Systems/EngineSubsystem.h"
#include "Core/OpaaxTypes.h"

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
        std::unordered_map<Uint32, BodyRecord>  m_Bodies;

        // Reusable scratch buffers drained from the world each step (no per-step allocation).
        std::vector<PhysicsContactPair>         m_SensorBegan;
        std::vector<PhysicsContactPair>         m_SensorEnded;
        std::vector<PhysicsContactPair>         m_ContactBegan;
        std::vector<PhysicsContactPair>         m_ContactEnded;

        // Currently-overlapping sensor pairs, keyed by the normalized pair (min<<32 | max of the
        // two entity bits) so (A,B) and (B,A) collapse to one entry. Value keeps the ordered
        // (sensor, visitor) pair so Tick re-fires with sensor-first semantics. Drives OnOverlapTick.
        std::unordered_map<Uint64, PhysicsContactPair> m_LiveOverlaps;
    };

} // namespace Opaax
