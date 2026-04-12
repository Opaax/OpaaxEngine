#include "TransformComponent.h"

using json = nlohmann::json;

Opaax::json Opaax::ECS::TransformComponent::Serialize() const
{
    return {
                { "position", { Position.x, Position.y } },
                { "scale",    { Scale.x,    Scale.y    } },
                { "rotation", Rotation },
                { "z_order",  ZOrder   }
    };
}

void Opaax::ECS::TransformComponent::DeserializeImplementation(const json& Json)
{
    Position.x = Json["position"][0].get<float>();
    Position.y = Json["position"][1].get<float>();
    
    Scale.x    = Json["scale"][0].get<float>();
    Scale.y    = Json["scale"][1].get<float>();
    
    Rotation   = Json["rotation"].get<float>();
    
    ZOrder     = Json["z_order"].get<float>();
}
