#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"   // Vector2F
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

    // Forward-declared for the caches.
    struct MoverResource;
    struct MoveModeResource;

    // =============================================================================
    // MoverSubsystem — advances every MoverComponent with the mode its Mover asset names.
    //   Play worlds only. Runs in FixedUpdate after physics (registration order), so it sees
    //   this step's poses. Loads the tuning assets through its own caches.
    // =============================================================================
    class MoverSubsystem final : public WorldSubsystemBase
    {
        // =============================================================================
        // Base implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(MoverSubsystem)

        /** Play worlds only. */
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
        /** Number of movers advanced on the last step. */
        Uint64 GetLastAdvanced() const noexcept { return m_LastAdvanced; }

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** One entity's step: resolve the mode, apply a pending switch, tick. */
        void Advance(EntityID InEntity, MoverComponent& InMover, TransformComponent& InTransform,
                     IPhysicsWorld& InWorld, float InDelta);

        /**
         * The tuning for InModeName in InMover, or nullptr. Each failure logs once.
         */
        const MoveModeData* ResolveMode(const MoverComponent& InMover, OpaaxStringID InModeName);

        /** Asset-relative -> absolute. */
        OpaaxString ToAbsolute(const OpaaxString& InAssetPath) const;

        /** True the first time InKey is seen (log once). */
        bool ShouldWarnOnce(Uint32 InKey);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        WorldContext* m_Context = nullptr;   // owned by the World

        /** Resolved in Startup. */
        const MoverModeRegistry* m_Modes = nullptr;

        /** Loaded resource caches. */
        TUnorderedMap<Uint32, ResourceRef<MoverResource>>    m_MoverCache;
        TUnorderedMap<Uint32, ResourceRef<MoveModeResource>> m_ModeCache;

        /** Keys already warned about. */
        TUnorderedSet<Uint32> m_Warned;

        Uint64 m_LastAdvanced = 0;
    };
}
