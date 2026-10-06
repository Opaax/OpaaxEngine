#include "TagComponent.h"

namespace Opaax
{
    OpaaxString ECS::TagComponent::TagComponentName = "Tag";
    
    json ECS::TagComponent::Serialize() const
    {
        json lTagJson;
        lTagJson[ECS::TagComponent::TagComponentName.CStr()] = Tag.ToString().CStr();
        return lTagJson;
    }
    
    void ECS::TagComponent::DeserializeImplementation(const json& InJson)
    {
        Tag = InJson["tag"].get<std::string>();
    }
}


