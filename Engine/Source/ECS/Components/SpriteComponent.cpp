#include "SpriteComponent.h"

#include "Assets/AssetRegistry.h"

Opaax::ECS::json Opaax::ECS::SpriteComponent::Serialize() const
{
    OpaaxString lPath;
    if (Texture.IsValid()) {
        lPath = Texture.GetID().ToString();
        OPAAX_CORE_WARN("Sprite texture path: {}", lPath);
    } else {
        lPath = "";
        OPAAX_CORE_WARN("Sprite has no valid texture!");
    }
        
    return {
                { "texture", lPath.CStr() },
                { "color",   { Color.r, Color.g, Color.b, Color.a } },
                { "uv_min",  { UVMin.x, UVMin.y } },
                { "uv_max",  { UVMax.x, UVMax.y } },
                { "visible", Visible }
    };
}

void Opaax::ECS::SpriteComponent::DeserializeImplementation(const json& Json)
{
    const OpaaxString lPath = Json["texture"].get<std::string>().c_str();
    if (!lPath.IsEmpty())
    {
        // NOTE: Path stored in JSON is already absolute (written by serializer).
        //   We construct the StringID directly — no second Resolve() call.
        Texture = AssetRegistry::Load<OpenGLTexture2D>(OpaaxStringID(lPath));
    }

    Color.r = Json["color"][0].get<float>();
    Color.g = Json["color"][1].get<float>();
    Color.b = Json["color"][2].get<float>();
    Color.a = Json["color"][3].get<float>();
    
    UVMin.x = Json["uv_min"][0].get<float>();
    UVMin.y = Json["uv_min"][1].get<float>();
    
    UVMax.x = Json["uv_max"][0].get<float>();
    UVMax.y = Json["uv_max"][1].get<float>();
    
    Visible = Json["visible"].get<bool>();
}
