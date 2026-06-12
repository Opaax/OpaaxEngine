#include "MoverComponent.h"

#include <algorithm>
#include <cmath>

#include "Assets/AssetRegistry.h"
#include "Physics/Collision/CollisionProfile.h"

#include "Core/Systems/Movement/MoverModeRegistry.h"
#include "Core/Systems/Movement/IMoverMode.h"
#include "Core/Systems/Movement/IMoverModeParams.h"

namespace Opaax::ECS
{
    namespace
    {
        // Mint a mode's default params via the registry (the mode is the factory for its type).
        // Null if the mode isn't registered yet (the subsystem reconciles on play-begin).
        UniquePtr<IMoverModeParams> MintModeParams(OpaaxStringID InMode)
        {
            if (IMoverMode* lMode = MoverModeRegistry::Find(InMode)) { return lMode->CreateDefaultParams(); }
            return nullptr;
        }
    }
}

// Out-of-line special members (the ModeParams UniquePtr map needs the complete IMoverModeParams type).
Opaax::ECS::MoverComponent::MoverComponent() = default;
Opaax::ECS::MoverComponent::MoverComponent(const Opaax::ECS::json& Json) : OpaaxComponentBase(Json) { Deserialize(Json); }
Opaax::ECS::MoverComponent::~MoverComponent() = default;
Opaax::ECS::MoverComponent::MoverComponent(MoverComponent&&) noexcept = default;
Opaax::ECS::MoverComponent& Opaax::ECS::MoverComponent::operator=(MoverComponent&&) noexcept = default;

void Opaax::ECS::MoverComponent::AddMode(OpaaxStringID InMode)
{
    if (SupportsMode(InMode)) { return; }
    Modes.push_back(InMode);
    if (ModeParams.find(InMode.GetId()) == ModeParams.end())
    {
        if (UniquePtr<IMoverModeParams> lParams = MintModeParams(InMode)) { ModeParams[InMode.GetId()] = Move(lParams); }
    }
}

void Opaax::ECS::MoverComponent::RemoveMode(OpaaxStringID InMode)
{
    for (size_t i = 0; i < Modes.size(); ++i)
    {
        if (Modes[i] == InMode) { Modes.erase(Modes.begin() + i); break; }
    }
    ModeParams.erase(InMode.GetId());
}

void Opaax::ECS::MoverComponent::EnsureModeParams()
{
    for (const OpaaxStringID& lMode : Modes)
    {
        if (ModeParams.find(lMode.GetId()) == ModeParams.end())
        {
            if (UniquePtr<IMoverModeParams> lParams = MintModeParams(lMode)) { ModeParams[lMode.GetId()] = Move(lParams); }
        }
    }
}

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

Opaax::Uint64 Opaax::ECS::MoverComponent::EffectiveMovementMask() const noexcept
{
    if (const CollisionProfile* lProfile = Profile.Get()) { return lProfile->ComputeBlockMaskBits(); }
    return CollisionMask;
}

Opaax::Uint64 Opaax::ECS::MoverComponent::EffectiveEventMask() const noexcept
{
    if (const CollisionProfile* lProfile = Profile.Get()) { return lProfile->ComputeMaskBits(); }
    return CollisionMask;
}

Opaax::ECS::json Opaax::ECS::MoverComponent::Serialize() const
{
    const OpaaxString lModeName = ModeId.ToString();

    json lModes = json::array();
    for (const OpaaxStringID& lId : Modes)
    {
        lModes.push_back(lId.ToString().CStr());
    }

    const OpaaxString lProfileRef = Profile.IsValid() ? Profile.GetID().ToString() : OpaaxString();

    // Per-mode params, keyed by mode name. Each mode serializes only its own knobs.
    json lModeParams = json::object();
    for (const auto& [lBits, lParams] : ModeParams)
    {
        if (lParams)
        {
            const OpaaxString lParamModeName = OpaaxStringID(lBits).ToString();
            lModeParams[lParamModeName.CStr()] = lParams->ToJson();
        }
    }

    return {
            { "shape",            ToString(Shape) },
            { "height",           Height },
            { "radius",           Radius },
            { "collision_mask",   CollisionMask },
            { "profile",          lProfileRef.CStr() },
            { "max_slope_deg",    MaxSlopeAngleDeg },
            { "modes",            lModes },
            { "mode",             lModeName.CStr() },
            { "modeParams",       lModeParams }
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
    if (Json.contains("profile"))
    {
        const OpaaxString lRef = Json["profile"].get<std::string>().c_str();
        if (!lRef.IsEmpty())
        {
            Profile = AssetRegistry::Load<CollisionProfile>(OpaaxStringID(lRef));
        }
    }
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

    // Per-mode params: mint each supported mode's default type, then override from "modeParams" if
    // present. (Modes registered at MoverSubsystem::Startup, before scenes load; the subsystem also
    // reconciles any still-missing params on play-begin.)
    ModeParams.clear();
    const json* lModeParams = (Json.contains("modeParams") && Json["modeParams"].is_object())
                            ? &Json["modeParams"] : nullptr;
    for (const OpaaxStringID& lMode : Modes)
    {
        UniquePtr<IMoverModeParams> lParams = MintModeParams(lMode);
        if (lParams == nullptr) { continue; }

        if (lModeParams != nullptr)
        {
            const OpaaxString lParamModeName = lMode.ToString();
            if (lModeParams->contains(lParamModeName.CStr()))
            {
                lParams->FromJson((*lModeParams)[lParamModeName.CStr()]);
            }
        }
        ModeParams[lMode.GetId()] = Move(lParams);
    }
}
