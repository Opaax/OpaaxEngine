#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"

namespace Sandbox
{
    // =============================================================================
    // QuadOscillatorSubsystem — a sample gameplay world subsystem. Play-only (ShouldCreate is asked on
    //   the class default, so none is made for an Edit world). Gets what it needs through the
    //   WorldContext.
    // =============================================================================
    class QuadOscillatorSubsystem final : public Opaax::WorldSubsystem
    {
        OPAAX_OBJECT(QuadOscillatorSubsystem, Opaax::WorldSubsystem)

        // =========================================================================
        // Override
        // =========================================================================
    public:
        /** Play worlds only. */
        bool ShouldCreate(const Opaax::World& InWorld) const override;

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

        Opaax::TDynArray<Baseline>  m_Baselines;
        double                      m_Elapsed   = 0.0;
        bool                        m_bCaptured = false;
    };
}
