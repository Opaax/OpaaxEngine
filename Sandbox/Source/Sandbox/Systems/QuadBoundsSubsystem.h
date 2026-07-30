#pragma once

#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"

namespace Sandbox
{
    // =============================================================================
    // QuadBoundsSubsystem — an authoring overlay: draws each quad's bounds.
    //
    //   EDIT-ONLY, the exact mirror of QuadOscillatorSubsystem. Together they are the whole
    //   point of the world-subsystem model: ONE registry of candidates, and each world takes the
    //   subset its mode qualifies for. In a Play world this type is never constructed, so the
    //   overlay does not exist rather than existing and checking a flag every frame.
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
}
