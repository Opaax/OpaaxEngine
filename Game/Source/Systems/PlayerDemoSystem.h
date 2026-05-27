#pragma once

#include "Core/Systems/GameSubsystem.h"
#include "ECS/OpaaxEntity.hpp"

namespace Opaax
{
    class OpaaxEvent;
}

/**
 * @class PlayerDemoSystem
 *
 * Sandbox subsystem that drives the Player entity on a Lissajous curve so the follow
 * camera has motion to track. PIE-gated by default (GameSubsystem IsPlayOnly = true)
 * so the player stays still in editor edit mode.
 *
 * Re-attaches the follow controller when the Player's EntityID changes (first frame,
 * or after a PIE restart that rebuilds the world). Press K during PIE to trigger a
 * decaying camera shake that composes over the active follow.
 */
class PlayerDemoSystem final : public Opaax::GameSubsystemBase
{
    // =============================================================================
    // Identification
    // =============================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(PlayerDemoSystem)

    // =============================================================================
    // CTORs
    // =============================================================================
public:
    PlayerDemoSystem() = default;
    explicit PlayerDemoSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    // =============================================================================
    // Override
    // =============================================================================
    //~Begin IGameSubsystem Interface
public:
    void          Update(double DeltaTime)               override;
    bool          OnEvent(Opaax::OpaaxEvent& InEvent)    override;
    Opaax::Uint32 GetEventCategoryFilter() const noexcept override;
    //~End IGameSubsystem Interface

    // =============================================================================
    // Members
    // =============================================================================
private:
    Opaax::EntityID m_PlayerEntity = Opaax::ENTITY_NONE;
    float           m_DemoTime     = 0.f;
};
