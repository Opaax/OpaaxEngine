#pragma once

#include "Core/Systems/GameSubsystem.h"

namespace Opaax
{
    class OpaaxEvent;
}

/**
 * @class PhysicsEventSystem
 *
 * Reacts to engine physics events (PIE-gated). Logs collision Enter/Exit, overlap
 * Start/Stop, and world-bounds kills so the sandbox is verifiable from the console, and
 * implements the pickup: when a sensor tagged "Pickup" reports OnOverlapStart it is
 * destroyed in the handler (score++). That destroy-in-handler is the forcing case for the
 * engine's live body-removal reconcile — the pickup's sensor body must vanish with it.
 */
class PhysicsEventSystem final : public Opaax::GameSubsystemBase
{
public:
    OPAAX_SUBSYSTEM_TYPE(PhysicsEventSystem)

    PhysicsEventSystem() = default;
    explicit PhysicsEventSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    //~Begin IGameSubsystem Interface
    bool          OnEvent(Opaax::OpaaxEvent& InEvent)     override;
    Opaax::Uint32 GetEventCategoryFilter() const noexcept override;
    //~End IGameSubsystem Interface

private:
    int m_Score = 0;
};
