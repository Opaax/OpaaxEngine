#pragma once

#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // LifecycleProbe — counts its lifecycle calls: a behaviour starts once, then updates every
    //   frame and every fixed step.
    // =============================================================================
    class LifecycleProbe : public Opaax::Behaviour
    {
    public:
        Opaax::Int32 StartCount       = 0;
        Opaax::Int32 UpdateCount      = 0;
        Opaax::Int32 FixedUpdateCount = 0;

        OPAAX_PROPERTIES(LifecycleProbe, OPAAX_PROP(StartCount), OPAAX_PROP(UpdateCount), OPAAX_PROP(FixedUpdateCount))

        void OnStart() override { ++StartCount; }
        void OnUpdate(float) override { ++UpdateCount; }
        void OnFixedUpdate(float) override { ++FixedUpdateCount; }
    };

    // =============================================================================
    // TimerProbe — a one-shot timer sets bFired after Delay; a repeating one counts Ticks every
    //   Period and is cleared once it reached RepeatLimit.
    // =============================================================================
    class TimerProbe : public Opaax::Behaviour
    {
    public:
        float        Delay       = 0.25f;
        float        Period      = 0.1f;
        Opaax::Int32 RepeatLimit = 3;

        bool         bFired = false;
        Opaax::Int32 Ticks  = 0;

        OPAAX_PROPERTIES(TimerProbe, OPAAX_PROP(Delay), OPAAX_PROP(Period), OPAAX_PROP(RepeatLimit),
                         OPAAX_PROP(bFired), OPAAX_PROP(Ticks))

        void OnStart() override
        {
            SetTimer<&TimerProbe::OnFired>(Delay);
            m_Repeating = SetTimer<&TimerProbe::OnTick>(Period, /*bInRepeat*/ true);
        }

        void OnFired() { bFired = true; }

        void OnTick()
        {
            if (++Ticks >= RepeatLimit)
            {
                ClearTimer(m_Repeating);
            }
        }

    private:
        Opaax::TimerHandle m_Repeating;
    };

    // =============================================================================
    // SelfDestructProbe — destroys its entity Seconds after it starts.
    // =============================================================================
    class SelfDestructProbe : public Opaax::Behaviour
    {
    public:
        float Seconds = 0.3f;

        OPAAX_PROPERTIES(SelfDestructProbe, OPAAX_PROP(Seconds))

        void OnStart() override { DestroyAfter(Seconds); }
    };

    /** Published by an EndProbe as it ends. */
    struct ProbeEnded
    {
        bool bWorldEnding = false;
    };

    // =============================================================================
    // EndProbe — publishes ProbeEnded as it ends, saying whether the whole world is ending.
    // =============================================================================
    class EndProbe : public Opaax::Behaviour
    {
    public:
        void OnDestroy() override { Broadcast(ProbeEnded{ IsWorldEnding() }); }
    };

    // =============================================================================
    // WitnessProbe — counts the EndProbes that ended, and how many of them because the world was
    //   ending.
    // =============================================================================
    class WitnessProbe : public Opaax::Behaviour
    {
    public:
        Opaax::Int32 Ended        = 0;
        Opaax::Int32 WorldEndings = 0;

        OPAAX_PROPERTIES(WitnessProbe, OPAAX_PROP(Ended), OPAAX_PROP(WorldEndings))

        void OnStart() override { Subscribe<&WitnessProbe::OnEnded>(); }

        void OnEnded(const ProbeEnded& InEvent)
        {
            ++Ended;
            if (InEvent.bWorldEnding)
            {
                ++WorldEndings;
            }
        }
    };
}
