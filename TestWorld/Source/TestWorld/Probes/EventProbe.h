#pragma once

#include "Core/String/OpaaxStringJson.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    /** Sent from one probe to another entity. */
    struct ProbePing
    {
        Opaax::Int32 Value = 0;
    };

    /** Published to every subscriber. */
    struct ProbeAnnounce
    {
        Opaax::Int32 Value = 0;
    };

    // =============================================================================
    // EventProbe — sends a ProbePing to the entity named Target and, with bAnnounce, publishes a
    //   ProbeAnnounce a moment later (once every probe has started and subscribed). Counts what it
    //   receives of both.
    // =============================================================================
    class EventProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString Target;
        bool               bAnnounce = false;

        Opaax::Int32 Pings     = 0;
        Opaax::Int32 PingValue = 0;
        Opaax::Int32 Announces = 0;

        OPAAX_PROPERTIES(EventProbe, OPAAX_PROP(Target), OPAAX_PROP(bAnnounce), OPAAX_PROP(Pings),
                         OPAAX_PROP(PingValue), OPAAX_PROP(Announces))

        void OnStart() override
        {
            Listen<&EventProbe::OnPing>();
            Subscribe<&EventProbe::OnAnnounce>();

            if (!Target.IsEmpty())
            {
                const Opaax::Entity lTarget = FindEntity(Target);
                if (lTarget.IsValid())
                {
                    Send(lTarget, ProbePing{ 42 });
                }
            }

            if (bAnnounce)
            {
                SetTimer<&EventProbe::Announce>(0.1f);
            }
        }

        void OnPing(const ProbePing& InPing)
        {
            ++Pings;
            PingValue = InPing.Value;
        }

        void OnAnnounce(const ProbeAnnounce&) { ++Announces; }

        void Announce() const { Broadcast(ProbeAnnounce{ 7 }); }
    };
}
