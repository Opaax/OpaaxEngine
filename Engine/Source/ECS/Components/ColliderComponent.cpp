#include "ColliderComponent.h"

#include "Assets/AssetRegistry.h"
#include "Physics/Collision/CollisionProfile.h"

Opaax::ECS::json Opaax::ECS::ColliderComponent::Serialize() const
{
    const OpaaxString lChannelName = ToStringID(Channel).ToString();
    const OpaaxString lProfileRef  = Profile.IsValid() ? Profile.GetID().ToString() : OpaaxString();

    return {
            { "shape",       ToString(Shape) },
            { "mode",        ToString(Mode) },
            { "channel",     lChannelName.CStr() },
            { "profile",     lProfileRef.CStr() },
            { "offset",      { Offset.x, Offset.y } },
            { "size",        { Size.x, Size.y } },
            { "radius",      Radius },
            { "density",     Density },
            { "friction",    Friction },
            { "restitution", Restitution }
    };
}

void Opaax::ECS::ColliderComponent::DeserializeImplementation(const json& Json)
{
    if (Json.contains("shape"))
    {
        Shape = ColliderShapeFromString(Json["shape"].get<std::string>().c_str());
    }
    if (Json.contains("mode"))
    {
        Mode = ColliderModeFromString(Json["mode"].get<std::string>().c_str());
    }
    if (Json.contains("channel"))
    {
        Channel = CollisionChannelFromStringID(OpaaxStringID(Json["channel"].get<std::string>()));
    }
    if (Json.contains("profile"))
    {
        const OpaaxString lRef = Json["profile"].get<std::string>().c_str();
        if (!lRef.IsEmpty())
        {
            Profile = AssetRegistry::Load<CollisionProfile>(OpaaxStringID(lRef));
        }
    }
    if (Json.contains("offset"))
    {
        Offset.x = Json["offset"][0].get<float>();
        Offset.y = Json["offset"][1].get<float>();
    }
    if (Json.contains("size"))
    {
        Size.x = Json["size"][0].get<float>();
        Size.y = Json["size"][1].get<float>();
    }
    if (Json.contains("radius"))      { Radius      = Json["radius"].get<float>(); }
    if (Json.contains("density"))     { Density     = Json["density"].get<float>(); }
    if (Json.contains("friction"))    { Friction    = Json["friction"].get<float>(); }
    if (Json.contains("restitution")) { Restitution = Json["restitution"].get<float>(); }
}
