#include "SceneSerializer.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"

using json = nlohmann::json;

namespace Opaax
{
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
            
            json lEntityJson;
            lEntityJson.merge_patch(lTagComp->Serialize());
            lEntityJson["components"] = json::object();

            // TransformComponent
            if (const ECS::TransformComponent* lT = lWorld.GetComponent<ECS::TransformComponent>(lEntity))
            {
                lEntityJson["components"]["TransformComponent"] = lT->Serialize();
            }
            
            // SpriteComponent
            if (const ECS::SpriteComponent* lS = lWorld.GetComponent<ECS::SpriteComponent>(lEntity))
            {
                lEntityJson["components"]["SpriteComponent"] = lS->Serialize();
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
            const std::string lTag = lEntityJson[Opaax::ECS::TagComponent::TagComponentName.CStr()].get<std::string>();
            const EntityID lEntity = lWorld.CreateEntity(lTag.c_str());

            const auto& lComponents = lEntityJson["components"];

            if (lComponents.contains("TransformComponent"))
            {
                ECS::TransformComponent& lTS = lWorld.AddComponent<ECS::TransformComponent>(lEntity);
                lTS.Deserialize(lComponents["TransformComponent"]);
            }

            if (lComponents.contains("SpriteComponent"))
            {
                ECS::SpriteComponent& lSc = lWorld.AddComponent<ECS::SpriteComponent>(lEntity);
                lSc.Deserialize(lComponents["SpriteComponent"]);
            }
        }

        OPAAX_CORE_INFO("SceneSerializer: loaded '{}' ({} entities).", InPath, lRoot["entities"].size());

        return true;
    }

} // namespace Opaax