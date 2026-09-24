#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "World/Entity/EntityTypes.h"
#include "World/Systems/WorldContext.h"
#include "World/Systems/WorldSubsystem.h"

namespace Sandbox
{
    // =============================================================================
    // QuadOscillatorSubsystem — the game's first real world subsystem (M4).
    //
    //   PLAY-ONLY: it is gameplay, so it must not run while you are authoring. That is the
    //   whole of `ShouldCreate` — one static function, and in an Edit world this type is never
    //   even CONSTRUCTED (the predicate is static precisely so no instance is needed to decide).
    //
    //   Everything it needs arrives through the WorldContext by constructor: the world whose
    //   entities it moves. It never touches the AppServiceLocator (D3) and the engine has never
    //   heard of this type — a game subsystem costs one `Register<T>()` and nothing else.
    // =============================================================================
    class QuadOscillatorSubsystem final : public Opaax::WorldSubsystemBase
    {
        // =========================================================================
        // Base implementation
        // =========================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(QuadOscillatorSubsystem)

        /** Gameplay: Play worlds only. Omitting this would create it everywhere. */
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
         * Record each quad's starting position, so the oscillation has something to swing around
         * instead of drifting.
         *
         * Deferred to the FIRST Update on purpose, not done in Startup: the world's entities are
         * spawned by the host in PostEngineStartup, which runs AFTER the startup world — and
         * therefore after this subsystem's Startup (BO4). At Startup the world is legitimately
         * empty. This is the normal shape for a subsystem that reads world content.
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
