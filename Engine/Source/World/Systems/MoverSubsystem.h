#pragma once

#include "Application/Services/ILogger.h"
#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"   // Vector2F — the motion probe
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourceRef.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Mover);

    class World;
    class IPhysicsWorld;
    class MoverModeRegistry;
    struct WorldContext;
    struct MoverComponent;
    struct TransformComponent;
    struct MoveModeData;

    // Only NAMED by the held claims.
    struct MoverResource;
    struct MoveModeResource;

    // =============================================================================
    // MoverSubsystem — advances every MoverComponent through the mode its Mover asset names.
    //
    //   PLAY WORLDS ONLY, for PhysicsSubsystem's reason: it MOVES authored transforms.
    //
    //   IT RUNS IN FixedUpdate, AFTER PHYSICS, and the order is the design: a mover sweeps against
    //   the world's shapes, so it must see the poses this step produced rather than last step's.
    //   Registration order is what gives it that — the subsystem manager ticks in the order
    //   candidates were registered.
    //
    //   THE TUNING COMES FROM AN ASSET, resolved through ref caches this subsystem owns —
    //   SpriteAnimationSubsystem's shape, and for its reason: ResourceManager::Load takes an
    //   absolute path while a TResourcePath is deliberately relative (**MP8**), and the resource
    //   layer cannot reach IPaths.
    // =============================================================================
    class OPAAX_API MoverSubsystem final : public WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(MoverSubsystem)

        /** Gameplay: Play worlds only. */
        static bool ShouldCreate(const World& InWorld);

        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        explicit MoverSubsystem(WorldContext& InContext) : m_Context(&InContext) {}

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
        // Get
        // =============================================================================
    public:
        /** How many movers were advanced on the last step — the number the logs assert on. */
        Uint64 GetLastAdvanced() const noexcept { return m_LastAdvanced; }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** One entity's step: resolve the mode, apply any pending switch, then tick it. */
        void Advance(EntityID InEntity, MoverComponent& InMover, TransformComponent& InTransform,
                     IPhysicsWorld& InWorld, float InDelta);

        /**
         * The tuning InMover's bag resolves for InModeName, or nullptr when the bag, the name or
         * the tuning cannot be resolved. Each failure logs ONCE per key.
         */
        const MoveModeData* ResolveMode(const MoverComponent& InMover, OpaaxStringID InModeName);

        /** Asset-relative -> absolute, through the context's IPaths. */
        OpaaxString ToAbsolute(const OpaaxString& InAssetPath) const;

        /** True the FIRST time InKey is passed, so a per-step path logs once and never again. */
        bool ShouldWarnOnce(Uint32 InKey);

        /** Say ONCE that a mover actually MOVED — a count of movers is not a claim about motion. */
        void NoteMoverMoved(EntityID InEntity, const Vector2F& InPosition);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // borrowed; the World owns it

        /** Resolved once in Startup — the modes a `.opaaxmovemode` may name. */
        const MoverModeRegistry* m_Modes = nullptr;

        /** The ref caches, RendererManager::ResolveTexture's shape. */
        TUnorderedMap<Uint32, ResourceRef<MoverResource>>    m_MoverCache;
        TUnorderedMap<Uint32, ResourceRef<MoveModeResource>> m_ModeCache;

        /** Keys already warned about, so a missing tuning does not print sixty lines a second. */
        TUnorderedSet<Uint32> m_Warned;

        Uint64 m_LastAdvanced     = 0;
        bool   m_bLoggedFirstStep = false;

        /** The one entity NoteMoverMoved watches, and where it started. */
        EntityID m_ProbeEntity   = ENTITY_NONE;
        Vector2F m_ProbeOrigin   = { 0.f, 0.f };
        bool     m_bLoggedMotion = false;
    };
}
