#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Resources/Asset/AssetHandle.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldSubsystem.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(Mover);

    class World;
    class IPhysicsWorld;
    struct MoverComponent;
    struct TransformComponent;
    class Mover;
    class MoveMode;

    // =============================================================================
    // MoverSubsystem — advances every MoverComponent with the mode its Mover asset names.
    //   Play worlds only. Runs in FixedUpdate after physics (its tick order), so it sees
    //   this step's poses. Loads the mode assets through its own caches.
    // =============================================================================
    class MoverSubsystem final : public WorldSubsystem
    {
        OPAAX_OBJECT(MoverSubsystem, WorldSubsystem)

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin WorldSubsystem interface
        /** Play worlds only. */
        bool  ShouldCreate(const World& InWorld) const override;
        Int32 GetTickOrder() const override { return WorldSubsystemOrder::PostPhysics; }

        void FixedUpdate(double InFixedDeltaTime) override;
        void Shutdown() override;
        //~End WorldSubsystem interface

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
         * The mode InModeName names in InMover, or nullptr. A mode asset that does not load walks
         * (GroundMoveMode's defaults). Each failure logs once.
         */
        const MoveMode* ResolveMode(const MoverComponent& InMover, OpaaxStringID InModeName);

        /** True the first time InKey is seen (log once). */
        bool ShouldWarnOnce(Uint32 InKey);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /** Loaded asset caches, by path. */
        TUnorderedMap<Uint32, TAssetHandle<Mover>>    m_MoverCache;
        TUnorderedMap<Uint32, TAssetHandle<MoveMode>> m_ModeCache;

        /** Keys already warned about. */
        TUnorderedSet<Uint32> m_Warned;

        Uint64 m_LastAdvanced = 0;
    };
}
