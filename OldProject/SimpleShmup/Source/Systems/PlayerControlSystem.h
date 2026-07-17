#pragma once

#include "Core/Systems/GameSubsystem.h"
#include "Core/Input/OpaaxInputTypes.hpp"

namespace Opaax
{
    class KeyPressedEvent;
    class KeyReleasedEvent;
}

/**
 * @class PlayerControlSystem
 *
 * Event-driven player controller. WASD + arrow keys drive an 8-bool input state
 * (per physical key — releasing W while still holding Up keeps the up direction
 * active). Space spawns bullets at FireRateHz while held. PIE-gated by default;
 * window-lost-focus zeroes all input to prevent stuck keys after Alt+Tab.
 */
class PlayerControlSystem final : public Opaax::GameSubsystemBase
{
    // =============================================================================
    // Identification
    // =============================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(PlayerControlSystem)

    // =============================================================================
    // CTORs
    // =============================================================================
public:
    PlayerControlSystem() = default;
    explicit PlayerControlSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    // =============================================================================
    // Override
    // =============================================================================
    //~Begin IGameSubsystem interface
public:
    void   Update(double DeltaTime) override;
    bool   OnEvent(Opaax::OpaaxEvent& Event) override;
    Opaax::Uint32 GetEventCategoryFilter() const noexcept override;
    //~End IGameSubsystem interface

    // =============================================================================
    // Public config (tweakable from game-app OnInitialize)
    // =============================================================================
public:
    float PlayerSpeed    = 400.f;
    float FireRateHz     = 8.f;
    float BulletSpeed    = 800.f;
    float BulletLifetime = 3.f;

    // =============================================================================
    // Helpers
    // =============================================================================
private:
    void HandleKeyDown(Opaax::EOpaaxKeyCode InKey);
    void HandleKeyUp  (Opaax::EOpaaxKeyCode InKey);
    void ResetAllInput();

    // =============================================================================
    // Members
    // =============================================================================
private:
    bool m_bW     = false;
    bool m_bA     = false;
    bool m_bS     = false;
    bool m_bD     = false;
    bool m_bUp    = false;
    bool m_bDown  = false;
    bool m_bLeft  = false;
    bool m_bRight = false;

    bool  m_bSpaceHeld = false;
    float m_FireTimer  = 0.f;
};
