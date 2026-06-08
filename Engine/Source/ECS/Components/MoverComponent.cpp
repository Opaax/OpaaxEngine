#include "MoverComponent.h"

#include <algorithm>
#include <cmath>

Opaax::MoverCapsule Opaax::ECS::MoverComponent::BuildCapsule() const noexcept
{
    MoverCapsule lCapsule;
    lCapsule.Radius = Radius;

    if (Shape == EMoverShape::Circle)
    {
        // Degenerate capsule — both semicircle centers coincide at the origin.
        lCapsule.Center1 = { 0.f, 0.f };
        lCapsule.Center2 = { 0.f, 0.f };
        return lCapsule;
    }

    // Vertical capsule: semicircle centers at +/-(Height/2 - Radius). Clamp to >= 0 so a
    // too-short capsule collapses to a circle rather than inverting.
    const float lHalfSegment = std::max(0.f, Height * 0.5f - Radius);
    lCapsule.Center1 = { 0.f, -lHalfSegment };
    lCapsule.Center2 = { 0.f,  lHalfSegment };
    return lCapsule;
}

float Opaax::ECS::MoverComponent::GroundNormalY() const noexcept
{
    constexpr float lDegToRad = 3.14159265358979323846f / 180.f;
    return std::cos(MaxSlopeAngleDeg * lDegToRad);
}

Opaax::ECS::json Opaax::ECS::MoverComponent::Serialize() const
{
    const OpaaxString lModeName = ModeId.ToString();

    json lModes = json::array();
    for (const OpaaxStringID& lId : Modes)
    {
        lModes.push_back(lId.ToString().CStr());
    }

    return {
            { "shape",            ToString(Shape) },
            { "height",           Height },
            { "radius",           Radius },
            { "collision_mask",   CollisionMask },
            { "max_slope_deg",    MaxSlopeAngleDeg },
            { "modes",            lModes },
            { "mode",             lModeName.CStr() },
            { "params",           Params }
    };
}

void Opaax::ECS::MoverComponent::DeserializeImplementation(const json& Json)
{
    if (Json.contains("shape"))
    {
        Shape = MoverShapeFromString(Json["shape"].get<std::string>().c_str());
    }
    if (Json.contains("height"))         { Height           = Json["height"].get<float>(); }
    if (Json.contains("radius"))         { Radius           = Json["radius"].get<float>(); }
    if (Json.contains("collision_mask")) { CollisionMask    = Json["collision_mask"].get<Uint64>(); }
    if (Json.contains("max_slope_deg"))  { MaxSlopeAngleDeg = Json["max_slope_deg"].get<float>(); }
    if (Json.contains("modes") && Json["modes"].is_array() && !Json["modes"].empty())
    {
        Modes.clear();
        for (const auto& lEntry : Json["modes"])
        {
            if (lEntry.is_string())
            {
                Modes.push_back(OpaaxStringID(OpaaxString(lEntry.get<std::string>().c_str())));
            }
        }
    }
    // Absent/empty "modes" keeps the default { GroundMove } so an old scene still walks.
    if (Modes.empty()) { Modes = { OPAAX_ID("GroundMove") }; }

    if (Json.contains("mode"))
    {
        ModeId = OpaaxStringID(OpaaxString(Json["mode"].get<std::string>().c_str()));
    }
    if (Json.contains("params"))
    {
        Params = Json["params"].get<MoverParams>();
    }
}
