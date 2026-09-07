#pragma once

#include "Application/Services/ILogger.h"
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
    //
    //   IT HAS NO ShouldCreate, and that is the point: a collider must be visible while you are
    //   AUTHORING it, which is exactly when PhysicsSubsystem does not exist (PH6). So this one
    //   runs in Edit and Play alike and reads the COMPONENTS rather than the simulation — the
    //   outline is where the shape is authored to be, which in Play is also where it is, because
    //   the transform is synced back from the body each step.
    //
    //   VISIBILITY IS A CHANNEL, NOT A MODE. Everything it submits goes on DebugChannels::Physics,
    //   so one toggle outside the producer silences it — including in a dev build of Game.exe,
    //   which is the case "just do not register it in the editor" could not have served (F4b).
    //
    //   It draws through WorldContext::Debug, which is the ONLY way a world subsystem draws:
    //   immediate mode, re-submitted every frame, into a frame RendererManager already owns (WS5).
    // =============================================================================
    class OPAAX_API ColliderDebugSubsystem final : public WorldSubsystemBase
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
        /** Colliders outlined on the last tick — the number the logs assert on. */
        Uint64 GetLastDrawnCount() const noexcept { return m_LastDrawn; }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** One collider's outline, in whichever primitive its shape calls for. */
        void DrawCollider(const ColliderComponent& InCollider, const TransformComponent& InTransform);

        /** Sensors read differently from solids at a glance — the one thing colour must encode. */
        static Vector4F ColorFor(const ColliderComponent& InCollider);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        Uint64 m_LastDrawn         = 0;
        bool   m_bLoggedFirstTick  = false;
    };
}
