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
    struct ColliderComponent;
    struct TransformComponent;

    // =============================================================================
    // ColliderDebugSubsystem — draws every collider's outline, every frame, paused or not (Present),
    //   at the pose of the world's last step.
    //   Runs in Edit and Play worlds (colliders must be visible while editing) and reads the components.
    //   Dev builds only: a ship build's worlds never get one. Drawn on DebugChannels::Physics,
    //   which can be toggled.
    // =============================================================================
    class ColliderDebugSubsystem final : public WorldSubsystem
    {
        OPAAX_OBJECT(ColliderDebugSubsystem, WorldSubsystem)

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin WorldSubsystem interface
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::Debug; }
        bool  ShouldCreate(const World& InWorld) const override;

        bool Startup() override;
        void Present(double InRealDeltaTime) override;
        void Shutdown() override;
        //~End WorldSubsystem interface

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** The creation rule, for any build: true in a dev build only. */
        static constexpr bool ShouldCreateIn(bool bInDevBuild) noexcept { return bInDevBuild; }

        /** Number of colliders drawn on the last Present. */
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
        Uint64 m_LastDrawn = 0;
    };
}
