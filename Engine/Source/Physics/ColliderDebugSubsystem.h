#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ColliderDebug);

    class World;
    struct WorldContext;
    struct ColliderComponent;
    struct TransformComponent;

    // =============================================================================
    // ColliderDebugSubsystem — draws every collider's outline, every frame.
    //   Runs in Edit and Play worlds (colliders must be visible while editing) and reads the components.
    //   Drawn on DebugChannels::Physics, which can be toggled.
    // =============================================================================
    class ColliderDebugSubsystem final : public WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(ColliderDebugSubsystem)

        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        explicit ColliderDebugSubsystem(WorldContext& InContext) : m_Context(&InContext) {}

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin ISubsystem interface
        bool Startup() override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;
        //~End ISubsystem interface

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** Number of colliders drawn on the last tick. */
        Uint64 GetLastDrawnCount() const noexcept { return m_LastDrawn; }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** One collider's outline. */
        void DrawCollider(const ColliderComponent& InCollider, const TransformComponent& InTransform);

        /** Colour: sensors and solids differ. */
        static Vector4F ColorFor(const ColliderComponent& InCollider);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // owned by the World

        Uint64 m_LastDrawn = 0;
    };
}
