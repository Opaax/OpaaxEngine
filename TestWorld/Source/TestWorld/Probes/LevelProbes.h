#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "Engine/GameInstance/GameInstanceContext.h"
#include "Engine/GameInstance/IGameInstanceSubsystem.h"
#include "Engine/Subsystems/EngineEventBus.h"
#include "Probes/LifecycleProbes.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // ProbeLedger — the game's record, kept across levels: how many LedgerProbes started (one per
    //   level that has one), and the EndProbes that ended (and how many because their world ended).
    // =============================================================================
    class ProbeLedger final : public Opaax::GameInstanceSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(ProbeLedger)

        explicit ProbeLedger(Opaax::GameInstanceContext& InContext) : m_Context(&InContext) {}

        bool Startup() override
        {
            m_Context->Events.GetEventBus().Subscribe(this, &ProbeLedger::OnEnded);
            return true;
        }

        void Shutdown() override { m_Context->Events.GetEventBus().UnsubscribeAll(this); }

        Opaax::Int32 LedgersStarted = 0;
        Opaax::Int32 Ended          = 0;
        Opaax::Int32 WorldEndings   = 0;

    private:
        void OnEnded(const ProbeEnded& InEvent)
        {
            ++Ended;
            if (InEvent.bWorldEnding)
            {
                ++WorldEndings;
            }
        }

        Opaax::GameInstanceContext* m_Context = nullptr;
    };

    // =============================================================================
    // LedgerProbe — signs the game's ProbeLedger when it starts, and copies the ledger's counts into
    //   its own fields every frame, where the test scripts read them.
    // =============================================================================
    class LedgerProbe : public Opaax::Behaviour
    {
    public:
        bool         bFound         = false;
        Opaax::Int32 LedgersStarted = 0;
        Opaax::Int32 Ended          = 0;
        Opaax::Int32 WorldEndings   = 0;

        OPAAX_PROPERTIES(LedgerProbe, OPAAX_PROP(bFound), OPAAX_PROP(LedgersStarted), OPAAX_PROP(Ended),
                         OPAAX_PROP(WorldEndings))

        void OnStart() override
        {
            if (ProbeLedger* lLedger = GetGameSubsystem<ProbeLedger>())
            {
                bFound = true;
                ++lLedger->LedgersStarted;
            }
            Read();
        }

        void OnUpdate(float) override { Read(); }

    private:
        void Read()
        {
            if (const ProbeLedger* lLedger = GetGameSubsystem<ProbeLedger>())
            {
                LedgersStarted = lLedger->LedgersStarted;
                Ended          = lLedger->Ended;
                WorldEndings   = lLedger->WorldEndings;
            }
        }
    };

    // =============================================================================
    // TravelProbe — opens the level Target Delay seconds after it starts; with bQuit it quits the
    //   game instead.
    // =============================================================================
    class TravelProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString Target;
        float              Delay = 0.2f;
        bool               bQuit = false;

        OPAAX_PROPERTIES(TravelProbe, OPAAX_PROP(Target), OPAAX_PROP(Delay), OPAAX_PROP(bQuit))

        void OnStart() override { SetTimer<&TravelProbe::Travel>(Delay); }

        void Travel() const
        {
            if (bQuit) { QuitGame(); }
            else       { OpenLevel(Target); }
        }
    };
}
