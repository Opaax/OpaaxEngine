#pragma once
#include "Core/Component/OpaaxComponent.h"

/**
 * @struct LifetimeComponent
 *
 * Countdown in seconds. LifetimeSystem decrements every frame and destroys
 * the entity once SecondsRemaining <= 0.
 */
struct LifetimeComponent : public Opaax::OpaaxComponentBase
{
    float SecondsRemaining = 1.f;

    Opaax::json Serialize() const override
    {
        return { { "seconds_remaining", SecondsRemaining } };
    }

protected:
    void DeserializeImplementation(const Opaax::json& Json) override
    {
        if (Json.contains("seconds_remaining"))
        {
            SecondsRemaining = Json["seconds_remaining"].get<float>();
        }
    }
};
