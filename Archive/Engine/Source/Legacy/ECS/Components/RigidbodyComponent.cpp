#include "RigidbodyComponent.h"

Opaax::ECS::json Opaax::ECS::RigidbodyComponent::Serialize() const
{
    return {
            { "type",            ToString(Type) },
            { "gravity_scale",   GravityScale },
            { "fixed_rotation",  FixedRotation },
            { "linear_damping",  LinearDamping },
            { "angular_damping", AngularDamping }
    };
}

void Opaax::ECS::RigidbodyComponent::DeserializeImplementation(const json& Json)
{
    if (Json.contains("type"))
    {
        Type = BodyTypeFromString(Json["type"].get<std::string>().c_str());
    }
    if (Json.contains("gravity_scale"))   { GravityScale   = Json["gravity_scale"].get<float>(); }
    if (Json.contains("fixed_rotation"))  { FixedRotation  = Json["fixed_rotation"].get<bool>(); }
    if (Json.contains("linear_damping"))  { LinearDamping  = Json["linear_damping"].get<float>(); }
    if (Json.contains("angular_damping")) { AngularDamping = Json["angular_damping"].get<float>(); }
}
