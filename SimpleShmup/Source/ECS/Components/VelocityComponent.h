#pragma once
#include "Core/OpaaxMathTypes.h"
#include "Core/Component/OpaaxComponent.h"

/**
 * @struct VelocityComponent
 *
 * Per-frame linear velocity in world units / second.
 * MovementSystem integrates this into TransformComponent::Position.
 */
struct VelocityComponent : public Opaax::OpaaxComponentBase
{
    Opaax::Vector2F Velocity = { 0.f, 0.f };

    Opaax::json Serialize() const override
    {
        return {
            { "velocity", { Velocity.x, Velocity.y } }
        };
    }

protected:
    void DeserializeImplementation(const Opaax::json& Json) override
    {
        if (Json.contains("velocity") && Json["velocity"].is_array() && Json["velocity"].size() == 2)
        {
            Velocity.x = Json["velocity"][0].get<float>();
            Velocity.y = Json["velocity"][1].get<float>();
        }
    }
};
