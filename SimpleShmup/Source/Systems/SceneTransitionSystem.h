#pragma once

#include "Core/Systems/GameSubsystem.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class KeyPressedEvent;
    class KeyReleasedEvent;
}

/**
 * @class SceneTransitionSystem
 *
 * Owns Menu↔Game transitions for the G1 vertical slice. Event-driven:
 *   - Space-just-pressed in MenuScene      → Replace(ShmupGameScene)
 *   - Escape held >= EscHoldThreshold in   → Replace(MenuScene)
 *     ShmupGameScene
 *
 * Tap-Esc is intentionally a no-op (locked decision #2 — prevents accidental
 * exit). Active scene is identified by Scene::GetName() since Scene has no
 * type tag and we want zero RTTI assumptions.
 *
 * Registered LAST in tick order so the rest of the frame's gameplay resolves
 * before a transition fires.
 */
class SceneTransitionSystem final : public Opaax::GameSubsystemBase
{
    // =============================================================================
    // Identification
    // =============================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(SceneTransitionSystem)

    // =============================================================================
    // CTORs
    // =============================================================================
public:
    SceneTransitionSystem() = default;
    explicit SceneTransitionSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    // =============================================================================
    // Override
    // =============================================================================
    //~Begin IGameSubsystem interface
public:
    void          Update(double DeltaTime) override;
    bool          OnEvent(Opaax::OpaaxEvent& Event) override;
    Opaax::Uint32 GetEventCategoryFilter() const noexcept override;
    //~End IGameSubsystem interface

    // =============================================================================
    // Public config
    // =============================================================================
public:
    float EscHoldThreshold = 0.6f;

    // =============================================================================
    // Members
    // =============================================================================
private:
    bool  m_bSpaceJustPressed = false;
    bool  m_bEscHeld          = false;
    float m_EscHeldFor        = 0.f;
};
