#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"

namespace Sandbox
{
    // =============================================================================
    // QuadOscillatorSubsystem — a sample gameplay world subsystem. Play-only (ShouldCreate is static,
    //   so it is never even constructed in an Edit world). Gets what it needs through the
    //   WorldContext.
    // =============================================================================
    class QuadOscillatorSubsystem final : public Opaax::WorldSubsystemBase
    {
        // =========================================================================
        // Base implementation
        // =========================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(QuadOscillatorSubsystem)

        /** Play worlds only. */
        static bool ShouldCreate(const Opaax::World& InWorld);

        // =========================================================================
        // CTORS
        // =========================================================================
    public:
        explicit QuadOscillatorSubsystem(Opaax::WorldContext& InContext) : m_Context(&InContext) {}

        // =========================================================================
        // Override
        // =========================================================================
    public:
        bool Startup() override;
        void Update(double InDeltaTime) override;
        void Shutdown() override;

        // =========================================================================
        // Functions
        // =========================================================================
    private:
        /**
         * Records each quad's starting position, so the oscillation swings around it. Done on the first
         * Update, not in Startup: the world's entities are loaded after this subsystem starts.
         */
        void CaptureBaselines();

        // =========================================================================
        // Members
        // =========================================================================
    private:
        struct Baseline
        {
            Opaax::EntityID  Entity;
            Opaax::Vector2F  Position;
        };

        Opaax::WorldContext*        m_Context = nullptr; // borrowed; the World owns it
        Opaax::TDynArray<Baseline>  m_Baselines;
        double                      m_Elapsed   = 0.0;
        bool                        m_bCaptured = false;
    };
}
