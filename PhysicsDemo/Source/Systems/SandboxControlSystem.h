#pragma once

#include "Core/Systems/GameSubsystem.h"
#include "Core/OpaaxMathTypes.h"
#include "ECS/OpaaxEntity.hpp"

namespace Opaax
{
    class OpaaxEvent;
    class World;
}

/**
 * @class SandboxControlSystem
 *
 * Drives the player Mover and the on-demand query probes from keyboard input (PIE-gated):
 *   A/D  — walk (Ground) / horizontal (Fly)      W/S  — vertical (Fly only)
 *   Space — jump (Ground)                         Tab  — toggle Ground <-> Fly
 *   Q     — ray cast straight down from the player (logs the hit)
 *   E     — overlap query of an AABB around the player (logs the entities)
 *
 * Input is latched event-side (key down/up) and applied in Update against the single
 * MoverComponent in the world; the entity is re-found each frame so a PIE restart that
 * rebuilds the world is handled transparently.
 */
class SandboxControlSystem final : public Opaax::GameSubsystemBase
{
public:
    OPAAX_SUBSYSTEM_TYPE(SandboxControlSystem)

    SandboxControlSystem() = default;
    explicit SandboxControlSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    //~Begin IGameSubsystem Interface
    void          Update(double DeltaTime)                override;
    bool          OnEvent(Opaax::OpaaxEvent& InEvent)     override;
    Opaax::Uint32 GetEventCategoryFilter() const noexcept override;
    //~End IGameSubsystem Interface

private:
    void DoRaycastDown(Opaax::Vector2F InOrigin);
    void DoOverlapHere(Opaax::Vector2F InCenter);

    // Create a dynamic box entity at InAt at RUNTIME (during play). The PhysicsSubsystem's
    // per-step reconcile builds its body next step, so it falls without any play-begin rebuild —
    // the proof that physics can be added to an entity dynamically.
    void SpawnDynamicBox(Opaax::Vector2F InAt);

    // Re-point the follow camera at InTarget (re-attaches on a fresh EntityID after a PIE restart).
    void AttachFollowCamera(Opaax::EntityID InTarget);

    // Held movement keys (cleared on release / focus loss).
    bool m_Left  = false;   // A
    bool m_Right = false;   // D
    bool m_Up    = false;   // W
    bool m_Down  = false;   // S

    // One-shot edges consumed in Update (need the resolved Mover, so they wait for the tick).
    bool m_JumpQueued       = false;
    bool m_ToggleModeQueued = false;
    bool m_RaycastQueued    = false;
    bool m_OverlapQueued    = false;
    bool m_SpawnQueued      = false;

    // The entity the follow camera currently tracks; re-attaches when it changes (PIE restart
    // rebuilds the world with a fresh player id).
    Opaax::EntityID m_CameraTarget = Opaax::ENTITY_NONE;
};
