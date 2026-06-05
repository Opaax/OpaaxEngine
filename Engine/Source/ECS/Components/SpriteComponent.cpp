#include "SpriteComponent.h"

#include "Assets/AssetRegistry.h"
#include "Renderer/Texture2D.h"

Opaax::ECS::json Opaax::ECS::SpriteComponent::Serialize() const
{
    const OpaaxString lSerializedRef = Texture.IsValid()
                                     ? Texture.GetID().ToString()
                                     : OpaaxString();

    const OpaaxString lLayerName = ToStringID(Layer).ToString();

    return {
            { "texture", lSerializedRef.CStr() },
            { "size",    { Size.x, Size.y } },
            { "color",   { Color.r, Color.g, Color.b, Color.a } },
            { "uv_min",  { UVMin.x, UVMin.y } },
            { "uv_max",  { UVMax.x, UVMax.y } },
            { "visible", Visible },
            { "layer",   lLayerName.CStr() },
            { "order",   OrderInLayer }
    };
}

void Opaax::ECS::SpriteComponent::DeserializeImplementation(const json& Json)
{
    // Pass the raw stored reference to AssetRegistry::Load — its ResolveToAbsPath
    // already handles all three cases: absolute path, manifest logical ID, raw
    // relative path. Pre-resolving here would skip the manifest lookup.
    const OpaaxString lRef = Json["texture"].get<std::string>().c_str();
    if (!lRef.IsEmpty())
    {
        Texture = AssetRegistry::Load<Texture2D>(OpaaxStringID(lRef));
    }

    // Backward compat: pre-M3 scenes don't carry "size" — default to (1,1) so
    // existing scenes render unchanged (Transform.Scale used to be the de facto size).
    if (Json.contains("size"))
    {
        Size.x = Json["size"][0].get<float>();
        Size.y = Json["size"][1].get<float>();
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

    // Backward compat: pre-M7 scenes carry no layer/order — keep the defaults so they
    // render unchanged (Default band, order 0).
    if (Json.contains("layer"))
    {
        Layer = RenderLayerFromStringID(OpaaxStringID(Json["layer"].get<std::string>()));
    }
    if (Json.contains("order"))
    {
        OrderInLayer = Json["order"].get<Int16>();
    }
}
