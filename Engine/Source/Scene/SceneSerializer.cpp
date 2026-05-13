#include "SceneSerializer.h"

#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "ECS/Components/ParentComponent.h"
#include "ECS/Components/SceneIDComponent.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/UuidComponent.h"
#include "ECS/Hierarchy.h"
#include "World/World.h"

using json = nlohmann::json;

namespace Opaax
{
    // Bump when the on-disk scene format gains a non-backward-compatible field.
    // Loader treats absent / older versions leniently — see Deserialize.
    static constexpr int kSceneFormatVersion = 2;

    // =============================================================================
    // Serialize
    // =============================================================================
    bool SceneSerializer::Serialize(const Scene& InScene, const char* InPath, const World& InWorld)
    {
        json lRoot;
        lRoot["version"]  = kSceneFormatVersion;
        lRoot["scene"]    = InScene.GetName().CStr();
        lRoot["entities"] = json::array();

        const auto& lRegistry = InWorld.GetRegistry();

        // Only entities owned by InScene get serialized. Persistent entities
        // (SceneID == PersistentSceneID) are runtime-only and intentionally
        // excluded — they belong to no on-disk scene.
        const Uint32 lTargetSceneID = InScene.GetSceneID();
        auto lView = lRegistry.view<const ECS::TagComponent, const ECS::SceneIDComponent>();

        for (auto lEntity : lView)
        {
            if (lView.get<const ECS::SceneIDComponent>(lEntity).SceneID != lTargetSceneID) { continue; }
            OPAAX_ASSERT(lRegistry.valid(lEntity))

            const ECS::TagComponent* lTagComp = InWorld.GetComponent<ECS::TagComponent>(lEntity);

            json lEntityJson;
            lEntityJson.merge_patch(lTagComp->Serialize());

            // Stable id
            if (const ECS::UuidComponent* lU = InWorld.GetComponent<ECS::UuidComponent>(lEntity))
            {
                lEntityJson["uuid"] = std::to_string(lU->Id);
            }

            // Parent link — emitted only if the *parent* still has a UUID.
            if (const ECS::ParentComponent* lP = InWorld.GetComponent<ECS::ParentComponent>(lEntity))
            {
                if (lP->Parent != ENTITY_NONE && InWorld.GetRegistry().valid(lP->Parent))
                {
                    if (const ECS::UuidComponent* lParentU =
                            InWorld.GetComponent<ECS::UuidComponent>(lP->Parent))
                    {
                        lEntityJson["parent_uuid"] = std::to_string(lParentU->Id);
                    }
                }
            }

            lEntityJson["components"] = json::object();

            // TransformComponent
            if (const ECS::TransformComponent* lT = InWorld.GetComponent<ECS::TransformComponent>(lEntity))
            {
                lEntityJson["components"]["TransformComponent"] = lT->Serialize();
            }

            // SpriteComponent
            if (const ECS::SpriteComponent* lS = InWorld.GetComponent<ECS::SpriteComponent>(lEntity))
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
    bool SceneSerializer::Deserialize(Scene& InScene, const char* InPath, World& InWorld)
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

        // Pass 1 — create entities, restore non-relational state. We collect (child, parent_uuid)
        // pairs as we go and resolve them once every UUID is in place.
        struct PendingLink { EntityID Child; Uint64 ParentUuid; };
        TDynArray<PendingLink> lPending;

        for (const auto& lEntityJson : lRoot["entities"])
        {
            const std::string lTag = lEntityJson[Opaax::ECS::TagComponent::TagComponentName.CStr()].get<std::string>();
            // CreateEntity auto-tags with World::m_ActiveSceneID — caller must set
            // it to the target scene's SceneID before invoking Deserialize.
            const EntityID lEntity = InWorld.CreateEntity(lTag.c_str());

            // Restore the persisted UUID (CreateEntity already gave it a fresh one).
            if (lEntityJson.contains("uuid"))
            {
                const auto& lUuidNode = lEntityJson["uuid"];
                Uint64 lParsed = 0;
                if (lUuidNode.is_string())
                {
                    try { lParsed = std::stoull(lUuidNode.get<std::string>()); }
                    catch (...) { lParsed = 0; }
                }
                else if (lUuidNode.is_number_unsigned())
                {
                    lParsed = lUuidNode.get<Uint64>();
                }

                if (lParsed != 0)
                {
                    if (auto* lU = InWorld.GetComponent<ECS::UuidComponent>(lEntity))
                    {
                        lU->Id = lParsed;
                    }
                }
            }

            // Defer parent resolution until pass 2.
            if (lEntityJson.contains("parent_uuid"))
            {
                const auto& lPNode = lEntityJson["parent_uuid"];
                Uint64 lParsed = 0;
                if (lPNode.is_string())
                {
                    try { lParsed = std::stoull(lPNode.get<std::string>()); }
                    catch (...) { lParsed = 0; }
                }
                else if (lPNode.is_number_unsigned())
                {
                    lParsed = lPNode.get<Uint64>();
                }

                if (lParsed != 0)
                {
                    lPending.push_back({ lEntity, lParsed });
                }
            }

            const auto& lComponents = lEntityJson["components"];

            if (lComponents.contains("TransformComponent"))
            {
                ECS::TransformComponent& lTS = InWorld.AddComponent<ECS::TransformComponent>(lEntity);
                lTS.Deserialize(lComponents["TransformComponent"]);
            }

            if (lComponents.contains("SpriteComponent"))
            {
                ECS::SpriteComponent& lSc = InWorld.AddComponent<ECS::SpriteComponent>(lEntity);
                lSc.Deserialize(lComponents["SpriteComponent"]);
            }
        }

        // Pass 2 — resolve parent_uuid → EntityID and re-link.
        for (const PendingLink& lLink : lPending)
        {
            const EntityID lParent = InWorld.FindByUuid(lLink.ParentUuid);
            if (lParent == ENTITY_NONE)
            {
                OPAAX_CORE_WARN("SceneSerializer: unresolved parent_uuid {} — child left at root.",
                    lLink.ParentUuid);
                continue;
            }
            if (!ECS::Hierarchy::SetParent(InWorld, lLink.Child, lParent))
            {
                OPAAX_CORE_WARN("SceneSerializer: SetParent rejected (cycle or invalid) for uuid {}.",
                    lLink.ParentUuid);
            }
        }

        OPAAX_CORE_INFO("SceneSerializer: loaded '{}' ({} entities, {} parent links).",
            InPath, lRoot["entities"].size(), lPending.size());

        return true;
    }

} // namespace Opaax
