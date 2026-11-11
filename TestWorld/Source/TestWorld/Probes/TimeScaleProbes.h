#pragma once

#include "Core/Maths/MathTypes.h"
#include "World/Behaviour/Behaviour.h"

namespace TestWorld
{
    // =============================================================================
    // DriftProbe — moves its entity right at Speed units a second of game time, in fixed steps, and
    //   adds up how far it went and the game time its updates were given. Reset Distance and
    //   GameTime to 0 to measure from a point: at a time scale of 0.25 a real second moves it a
    //   quarter as far.
    // =============================================================================
    class DriftProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(DriftProbe, Opaax::Behaviour)

    public:
        float Speed    = 100.f;
        float Distance = 0.f;
        float GameTime = 0.f;

        OPAAX_PROPERTIES(DriftProbe, OPAAX_PROP(Speed), OPAAX_PROP(Distance), OPAAX_PROP(GameTime))

        void OnUpdate(const float InDeltaTime) override { GameTime += InDeltaTime; }

        void OnFixedUpdate(const float InFixedDeltaTime) override
        {
            const float lStep = Speed * InFixedDeltaTime;
            Distance += lStep;
            SetPosition(GetPosition() + Opaax::Vector2F{ lStep, 0.f });
        }
    };
}
