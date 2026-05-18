#include "SceneSerializer.h"

#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "Assets/AssetRegistry.h"
#include "Core/OpaaxPath.h"
#include "ECS/ComponentRegistry.h"
#include "ECS/Components/ParentComponent.h"
#include "ECS/Components/SceneIDComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/UuidComponent.h"
#include "ECS/Hierarchy.h"
#include "World/World.h"

using json = nlohmann::json;

namespace Opaax
{
    // Bump when the on-disk scene format gains a non-backward-compatible field.
    // Loader treats absent / older versions leniently — see DeserializeIntoActiveSceneID.
    static constexpr int kSceneFormatVersion = 2;

    // Label written into the "scene" field for the persistent-bucket dump.
    // Informational only — the loader doesn't gate on it.
    static constexpr const char* kPersistentSceneLabel = "__persistents__";

    // =============================================================================
    // Shared serialize helper
    //
    // Filters World entities by SceneID and writes a {version, scene, entities}
    // dump to InPath. Used by both Serialize(Scene&) and SerializePersistents.
    // =============================================================================
    static bool SerializeEntitiesBySceneID(const World&   InWorld,
                                           const char*    InPath,
                                           Uint32         InSceneID,
                                           const char*    InSceneLabel)
    {
        json lRoot;
        lRoot["version"]  = kSceneFormatVersion;
        lRoot["scene"]    = InSceneLabel;
        lRoot["entities"] = json::array();

        const auto& lRegistry = InWorld.GetRegistry();

        auto lView = lRegistry.view<const ECS::TagComponent, const ECS::SceneIDComponent>();

        for (auto lEntity : lView)
        {
            if (lView.get<const ECS::SceneIDComponent>(lEntity).SceneID != InSceneID) { continue; }
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

            // Iterate every registered component type — engine built-ins were
            // registered in CoreEngineApp::Initialize; game-side components were
            // registered in the game-app's OnInitialize override. Tag and Uuid are
            // special-cased above at the entity-json top level (display name +
            // stable id) — skip them here so they don't appear twice in the file.
            static const OpaaxStringID kTagName ("TagComponent");
            static const OpaaxStringID kUuidName("UuidComponent");
            ComponentRegistry::ForEach([&](const IComponentEntry& InEntry)
            {
                const OpaaxStringID lName = InEntry.GetName();
                if (lName == kTagName || lName == kUuidName) { return; }

                if (InEntry.Has(InWorld, lEntity))
                {
                    lEntityJson["components"][lName.ToString().CStr()] =
                        InEntry.Save(InWorld, lEntity);
                }
            });

            lRoot["entities"].push_back(lEntityJson);
        }

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
    // Shared deserialize helper
    //
    // Reads the {version, scene, entities} dump from InPath and contributes
    // entities to InWorld's current m_ActiveSceneID. Callers control that ID:
    //   - Per-scene Push() sets it to the scene's runtime SceneID.
    //   - DeserializePersistents() swaps it to PersistentSceneID for the call.
    // =============================================================================
    static bool DeserializeIntoActiveSceneID(World& InWorld, const char* InPath)
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

            // Registry-driven load — unknown component names are logged and skipped
            // so a forward-compat or extension scene still loads cleanly.
            for (auto it = lComponents.begin(); it != lComponents.end(); ++it)
            {
                const OpaaxStringID lKey(it.key().c_str());
                const IComponentEntry* lEntry = ComponentRegistry::FindByName(lKey);
                if (!lEntry)
                {
                    OPAAX_CORE_WARN("SceneSerializer: unknown component '{}' in scene file, skipping.",
                        it.key());
                    continue;
                }
                lEntry->Load(InWorld, lEntity, it.value());
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

    // =============================================================================
    // Public — scene-based
    // =============================================================================
    bool SceneSerializer::Serialize(const Scene& InScene, const char* InPath, const World& InWorld)
    {
        return SerializeEntitiesBySceneID(InWorld, InPath, InScene.GetSceneID(), InScene.GetName().CStr());
    }

    bool SceneSerializer::Deserialize(Scene& /*InScene*/, const char* InPath, World& InWorld)
    {
        // The caller (SceneManager::Push, EditorSubsystem::ExitPlayMode) must have
        // already set World::m_ActiveSceneID to the destination scene's runtime ID
        // before invoking us — see the helper's comment for the contract.
        return DeserializeIntoActiveSceneID(InWorld, InPath);
    }

    // =============================================================================
    // Public — persistent bucket (PIE snapshot/restore)
    // =============================================================================
    bool SceneSerializer::SerializePersistents(const World& InWorld, const char* InPath)
    {
        return SerializeEntitiesBySceneID(InWorld, InPath, World::PersistentSceneID, kPersistentSceneLabel);
    }

    bool SceneSerializer::DeserializePersistents(World& InWorld, const char* InPath)
    {
        // Swap the active SceneID for the duration of the load so CreateEntity
        // tags every restored entity as persistent (SceneID == 0), then restore.
        const Uint32 lSavedActive = InWorld.GetActiveSceneID();
        InWorld.SetActiveSceneID(World::PersistentSceneID);

        const bool bOk = DeserializeIntoActiveSceneID(InWorld, InPath);

        InWorld.SetActiveSceneID(lSavedActive);
        return bOk;
    }

} // namespace Opaax
