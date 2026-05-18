#pragma once

#include <vector>

#include "Core/Systems/GameSubsystem.h"
#include "ECS/OpaaxEntity.hpp"

// =============================================================================
// CollisionSystem
//
// Pair-agnostic AABB overlap detector. Snapshots all entities with
// <TransformComponent, AABB2DComponent> each frame, runs the O(N^2) unordered
// pair test, and stores results in m_Hits. Holds no gameplay knowledge — the
// reaction (destroy / score / etc.) lives in ShmupGameRulesSystem, which reads
// GetHits() one tick downstream in the same UpdateAll pass.
// =============================================================================
class CollisionSystem final : public Opaax::GameSubsystemBase
{
    // =========================================================================
    // Identification
    // =========================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(CollisionSystem)

    // =========================================================================
    // Public types
    // =========================================================================
public:
    struct HitPair
    {
        Opaax::EntityID A;
        Opaax::EntityID B;
    };

    // =========================================================================
    // CTORs
    // =========================================================================
public:
    CollisionSystem() = default;
    explicit CollisionSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    // =========================================================================
    // Override
    // =========================================================================
    //~Begin IGameSubsystem interface
public:
    void Update(double DeltaTime) override;
    //~End IGameSubsystem interface

    // =========================================================================
    // Public API
    // =========================================================================
public:
    const std::vector<HitPair>& GetHits() const noexcept { return m_Hits; }

    // =========================================================================
    // Members
    // =========================================================================
private:
    std::vector<HitPair> m_Hits;
};
