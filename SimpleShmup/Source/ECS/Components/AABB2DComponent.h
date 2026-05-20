#pragma once
#include "Core/OpaaxMathTypes.h"
#include "Core/Component/OpaaxComponent.h"

/**
 * @struct AABB2DComponent
 *
 * Axis-aligned bounding box. Center = TransformComponent::Position + Offset;
 * box extents = (Position +/- HalfExtents). Computed each frame, no caching.
 */
struct AABB2DComponent : public Opaax::OpaaxComponentBase
{
    Opaax::Vector2F HalfExtents = { 16.f, 16.f };
    Opaax::Vector2F Offset      = { 0.f,  0.f  };

    Opaax::json Serialize() const override
    {
        return {
            { "half_extents", { HalfExtents.x, HalfExtents.y } },
            { "offset",       { Offset.x,      Offset.y      } }
        };
    }

protected:
    void DeserializeImplementation(const Opaax::json& Json) override
    {
        if (Json.contains("half_extents") && Json["half_extents"].is_array() && Json["half_extents"].size() == 2)
        {
            HalfExtents.x = Json["half_extents"][0].get<float>();
            HalfExtents.y = Json["half_extents"][1].get<float>();
        }
        if (Json.contains("offset") && Json["offset"].is_array() && Json["offset"].size() == 2)
        {
            Offset.x = Json["offset"][0].get<float>();
            Offset.y = Json["offset"][1].get<float>();
        }
    }
};
