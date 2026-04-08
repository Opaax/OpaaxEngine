#include "SceneSerializer.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "ECS/BaseComponents.hpp"
#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"

using json = nlohmann::json;

namespace Opaax
{
    // =============================================================================
    // Helpers — component → json / json → component
    // =============================================================================

    static json SerializeTransform(const ECS::TransformComponent& InT)
    {
        return {
            { "position", { InT.Position.x, InT.Position.y } },
            { "scale",    { InT.Scale.x,    InT.Scale.y    } },
            { "rotation", InT.Rotation },
            { "z_order",  InT.ZOrder   }
        };
    }

    static ECS::TransformComponent DeserializeTransform(const json& InJ)
    {
        ECS::TransformComponent lT;
        lT.Position.x = InJ["position"][0].get<float>();
        lT.Position.y = InJ["position"][1].get<float>();
        lT.Scale.x    = InJ["scale"][0].get<float>();
        lT.Scale.y    = InJ["scale"][1].get<float>();
        lT.Rotation   = InJ["rotation"].get<float>();
        lT.ZOrder     = InJ["z_order"].get<float>();
        return lT;
    }

    static json SerializeSprite(const ECS::SpriteComponent& InS)
    {
        OpaaxString lPath;
        if (InS.Texture.IsValid()) {
            lPath = InS.Texture.GetID().ToString();
            OPAAX_CORE_WARN("Sprite texture path: {}", lPath);
        } else {
            lPath = "";
            OPAAX_CORE_WARN("Sprite has no valid texture!");
        }
        
        return {
            { "texture", lPath.CStr() },
            { "color",   { InS.Color.r, InS.Color.g, InS.Color.b, InS.Color.a } },
            { "uv_min",  { InS.UVMin.x, InS.UVMin.y } },
            { "uv_max",  { InS.UVMax.x, InS.UVMax.y } },
            { "visible", InS.Visible }
        };
    }

    static ECS::SpriteComponent DeserializeSprite(const json& InJ)
    {
        ECS::SpriteComponent lS;

        const OpaaxString lPath = InJ["texture"].get<std::string>().c_str();
        if (!lPath.IsEmpty())
        {
            // NOTE: Path stored in JSON is already absolute (written by serializer).
            //   We construct the StringID directly — no second Resolve() call.
            lS.Texture = AssetRegistry::Load<OpenGLTexture2D>(OpaaxStringID(lPath));
        }

        lS.Color.r = InJ["color"][0].get<float>();
        lS.Color.g = InJ["color"][1].get<float>();
        lS.Color.b = InJ["color"][2].get<float>();
        lS.Color.a = InJ["color"][3].get<float>();
        lS.UVMin.x = InJ["uv_min"][0].get<float>();
        lS.UVMin.y = InJ["uv_min"][1].get<float>();
        lS.UVMax.x = InJ["uv_max"][0].get<float>();
        lS.UVMax.y = InJ["uv_max"][1].get<float>();
        lS.Visible = InJ["visible"].get<bool>();

        return lS;
    }

    // =============================================================================
    // Serialize
    // =============================================================================
    bool SceneSerializer::Serialize(const Scene& InScene, const char* InPath)
    {
        json lRoot;
        lRoot["scene"]    = InScene.GetName().CStr();
        lRoot["entities"] = json::array();

        const World& lWorld     = InScene.GetWorld();
        const auto& lRegistry   = lWorld.GetRegistry();
        

        // Iterate all entities that have a TagComponent — every entity has one.
        auto lView = lRegistry.view<const ECS::TagComponent>();

        OPAAX_CORE_WARN("Number of entities in view: {}", std::distance(lView.begin(), lView.end()));

        for (auto lEntity : lView)
        {
            OPAAX_ASSERT(lRegistry.valid(lEntity))

            const ECS::TagComponent* lTagComp = lWorld.GetComponent<ECS::TagComponent>(lEntity);
            OPAAX_CORE_WARN("Tag Name {}", lTagComp->Tag.ToString().CStr());
            
            json lEntityJson;
            lEntityJson["tag"] = lTagComp->Tag.ToString().CStr();
            lEntityJson["components"] = json::object();

            // TransformComponent
            if (const ECS::TransformComponent* lT = lWorld.GetComponent<ECS::TransformComponent>(lEntity))
            {
                lEntityJson["components"]["TransformComponent"] = SerializeTransform(*lT);
            }
            
            // SpriteComponent
            if (const ECS::SpriteComponent* lS = lWorld.GetComponent<ECS::SpriteComponent>(lEntity))
            {
                lEntityJson["components"]["SpriteComponent"] = SerializeSprite(*lS);
            }
            
            lRoot["entities"].push_back(lEntityJson);

        }

        // Write to disk
        std::ofstream lFile(InPath);
        if (!lFile.is_open())
        {
            OPAAX_CORE_ERROR("SceneSerializer::Serialize — cannot open '{}' for writing.", InPath);
            return false;
        }

        std::string lString = lRoot.dump(4);
        lFile << lString;  // 4-space indent — human readable
        OPAAX_CORE_INFO("SceneSerializer: saved '{}' ({} entities).",
            InPath, lRoot["entities"].size());

        return true;
    }

    // =============================================================================
    // Deserialize
    // =============================================================================
    bool SceneSerializer::Deserialize(Scene& InScene, const char* InPath)
    {
        std::ifstream lFile(InPath);
        if (!lFile.is_open())
        {
            OPAAX_CORE_ERROR("SceneSerializer::Deserialize — cannot open '{}'.", InPath);
            return false;
        }

        json lRoot;

        try
        {
            lFile >> lRoot;
        }
        catch (const json::parse_error& e)
        {
            OPAAX_CORE_ERROR("SceneSerializer::Deserialize — parse error in '{}': {}", InPath, e.what());
            return false;
        }

        auto& lWorld = InScene.GetWorld();

        for (const auto& lEntityJson : lRoot["entities"])
        {
            const std::string lTag = lEntityJson["tag"].get<std::string>();
            const EntityID lEntity = lWorld.CreateEntity(lTag.c_str());

            const auto& lComponents = lEntityJson["components"];

            if (lComponents.contains("TransformComponent"))
            {
                ECS::TransformComponent lTS = DeserializeTransform(lComponents["TransformComponent"]);
                lWorld.AddComponent<ECS::TransformComponent>(
                    lEntity, lTS);
            }

            if (lComponents.contains("SpriteComponent"))
            {
                ECS::SpriteComponent lSc = DeserializeSprite(lComponents["SpriteComponent"]);
                lWorld.AddComponent<ECS::SpriteComponent>(
                    lEntity, lSc);
            }
        }

        OPAAX_CORE_INFO("SceneSerializer: loaded '{}' ({} entities).", InPath, lRoot["entities"].size());

        return true;
    }

} // namespace Opaax