#include "SpriteComponent.h"

#include "Assets/AssetRegistry.h"

Opaax::ECS::json Opaax::ECS::SpriteComponent::Serialize() const
{
    OpaaxString lSerializedRef;

    if (Texture.IsValid())
    {
        const OpaaxStringID lID = Texture.GetID();

        const AssetDescriptor* lDesc = AssetManifest::Find(lID);
        
        if (lDesc != nullptr)
        {
            lSerializedRef = lDesc->ID.ToString();
            OPAAX_CORE_TRACE("SpriteComponent::Serialize — using logical ID '{}'", lSerializedRef);
        }
        else
        {
            const OpaaxString lAbsPath = lID.ToString();
            const OpaaxString lRelPath = OpaaxPath::MakeRelative(lAbsPath);
            
            lSerializedRef = lRelPath;
            OPAAX_CORE_TRACE("SpriteComponent::Serialize — using relative path '{}'", lSerializedRef);
        }
    }

    return {
            { "texture", lSerializedRef.CStr() },
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
        const OpaaxString lAbsPath = OpaaxPath::Resolve(lPath);
        Texture = AssetRegistry::Load<OpenGLTexture2D>(OpaaxStringID(lAbsPath));
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
