#pragma once

#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"

// =============================================================================
// QuadBoundsSubsystem — an authoring overlay: draws each quad's bounds.
//
//   EDITOR-MODULE content (moved here in M4 S5, from the game module): an Edit-only overlay is
//   editor content by definition, so it is compiled only into SandboxEditor.exe and registered
//   through EditWorldSystems(). Sandbox.exe never even links it.
//
//   That is the M4 gate in one file. The game module registers QuadOscillatorSubsystem through
//   WorldSubsystems() and the editor module registers this through EditWorldSystems(); the two
//   routes feed ONE WorldSubsystemRegistry, and each World takes the subset its mode qualifies
//   for. In a Play world this type is never constructed, so the overlay does not exist rather
//   than existing and checking a flag every frame — and to the World that creates them, an
//   editor overlay and a gameplay system are indistinguishable.
//
//   It draws through the context's DebugDraw, which is IMMEDIATE MODE by contract (F4): the
//   renderer drains and clears the queue every frame, so this re-submits every frame. There
//   is deliberately no Render hook for world subsystems — this is the drawing path (WS5).
// =============================================================================
class QuadBoundsSubsystem final : public Opaax::WorldSubsystemBase
{
    // =========================================================================
    // Base implementation
    // =========================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(QuadBoundsSubsystem)

    /** Authoring aid: Edit worlds only. */
    static bool ShouldCreate(const Opaax::World& InWorld);

    // =========================================================================
    // CTORS
    // =========================================================================
public:
    explicit QuadBoundsSubsystem(Opaax::WorldContext& InContext) : m_Context(&InContext) {}

    // =========================================================================
    // Override
    // =========================================================================
public:
    bool Startup() override;
    void Update(double InDeltaTime) override;
    void Shutdown() override;

    // =========================================================================
    // Members
    // =========================================================================
private:
    Opaax::WorldContext* m_Context = nullptr; // borrowed; the World owns it
    bool                 m_bLoggedFirstDraw = false;
};
