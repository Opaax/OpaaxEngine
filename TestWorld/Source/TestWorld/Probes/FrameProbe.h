#pragma once

#include "Core/Maths/Maths.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // FrameProbe — counts the frames and the fixed steps its world gets, and keeps its shortest and
    //   longest frame: what a paced run (--frame-pacing, time.pace) is checked with.
    // =============================================================================
    class FrameProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(FrameProbe, Opaax::Behaviour)

    public:
        Opaax::Int32 Frames = 0;
        Opaax::Int32 Steps  = 0;

        /** Seconds. Reset them to 1 and 0 to measure from a point. */
        float MinDelta = 1.f;
        float MaxDelta = 0.f;

        OPAAX_PROPERTIES(FrameProbe, OPAAX_PROP(Frames), OPAAX_PROP(Steps), OPAAX_PROP(MinDelta), OPAAX_PROP(MaxDelta))

        void OnUpdate(const float InDeltaTime) override
        {
            ++Frames;
            MinDelta = Opaax::Maths::Min(MinDelta, InDeltaTime);
            MaxDelta = Opaax::Maths::Max(MaxDelta, InDeltaTime);
        }

        void OnFixedUpdate(float) override { ++Steps; }
    };
}
