#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <type_traits>

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    inline constexpr LogCategory LogEntityJson{"EntityJson"};

    // =============================================================================
    // EntityJson — the entity array as JSON, shared by map and prefab files.
    // =============================================================================
    namespace EntityJson
    {
        // =====================================================================
        // Keys
        //
        // Named constants, so reader and writer agree.
        // =====================================================================
        inline constexpr const char* KEY_VERSION    = "version";
        inline constexpr const char* KEY_ENTITIES   = "entities";
        inline constexpr const char* KEY_GUID       = "guid";
        inline constexpr const char* KEY_NAME       = "name";
        inline constexpr const char* KEY_OWNER_MAP  = "ownerMap";
        inline constexpr const char* KEY_PARENT     = "parent";       // omitted for a root
        inline constexpr const char* KEY_COMPONENTS = "components";

        // =====================================================================
        // Dump forms
        // Files are indented (read in git diffs). The compact form is only for comparisons.
        // =====================================================================
        inline constexpr int k_FileIndent    =  4;
        inline constexpr int k_CompactIndent = -1;   // negative: no whitespace

        // =====================================================================
        // Interned ids as text
        // =====================================================================
        /**
         * An interned id as text. An invalid id gives "" (not "None").
         */
        const char* IdToText(OpaaxStringID InId);

        /**
         * Empty text gives an invalid id (for a MapId: runtime-spawned).
         */
        OpaaxStringID IdFromText(const OpaaxString& InText);

        /**
         * Reads a string field. Never throws.
         * @return Empty if the key is absent or not a string
         */
        OpaaxString ReadString(const nlohmann::json& InJson, const char* InKey);

        /**
         * dump() into an OpaaxString, keeping the length (no strlen).
         */
        OpaaxString DumpToString(const nlohmann::json& InJson, int InIndent);

        // =====================================================================
        // Placements (shared by map and prefab files)
        // =====================================================================
        inline constexpr const char* KEY_INSTANCES   = "prefabInstances";
        inline constexpr const char* KEY_PREFAB      = "prefab";
        inline constexpr const char* KEY_INSTANCE_ID = "instanceId";
        inline constexpr const char* KEY_OVERRIDES   = "overrides";

        /**
         * The records as a JSON array, sorted by InstanceId; overrides keyed by template guid text.
         * The caller's order is untouched.
         */
        nlohmann::json InstancesToJson(const TDynArray<PrefabInstanceRecord>& InInstances);

        /**
         * Reads the prefabInstances array under InRoot, if any, appending to OutInstances.
         * Never throws; a record without a path or with a bad id is skipped.
         * @return Number of skipped records
         */
        Uint64 InstancesFromJson(const nlohmann::json& InRoot, TDynArray<PrefabInstanceRecord>& OutInstances);

        // =====================================================================
        // The walk
        // =====================================================================
        /**
         * Parses a JSON array of entities, appending to OutEntities. Never throws.
         * A missing field takes its default; an unknown field is ignored; an entity with a missing or
         * bad guid is skipped. Does not log (the caller knows the document name).
         * @return Number of skipped entities
         */
        Uint64 EntitiesFromJson(const nlohmann::json& InEntities, TDynArray<EntityData>& OutEntities);

        /**
         * The JSON array of InEntities, sorted by guid (stable in git, and needed by the dirty check).
         * Components are an object keyed by name. Moves the payloads when given a temporary.
         * Objects are built by subscript (faster than initializer lists; same bytes).
         */
        template<typename TEntities>
        nlohmann::json EntitiesToJson(TEntities&& InEntities)
        {
            constexpr bool k_Consume = !std::is_lvalue_reference_v<TEntities>;
            using TEntity = std::conditional_t<k_Consume, EntityData, const EntityData>;

            // Sort a copy of the pointers: the input order is untouched.
            TDynArray<TEntity*> lOrdered;
            lOrdered.reserve(InEntities.size());
            for (TEntity& lEntity : InEntities)
            {
                lOrdered.emplace_back(&lEntity);
            }

            std::sort(lOrdered.begin(), lOrdered.end(),
                [](const EntityData* InLeft, const EntityData* InRight)
                {
                    // High then Low, like Guid::ToString.
                    return InLeft->Id.High != InRight->Id.High
                        ? InLeft->Id.High < InRight->Id.High
                        : InLeft->Id.Low  < InRight->Id.Low;
                });

            nlohmann::json lEntities = nlohmann::json::array();
            for (TEntity* lEntity : lOrdered)
            {
                nlohmann::json lComponents = nlohmann::json::object();
                for (auto& lComponent : lEntity->Components)
                {
                    if constexpr (k_Consume)
                    {
                        lComponents[IdToText(lComponent.TypeName)] = Move(lComponent.Payload);
                    }
                    else
                    {
                        lComponents[IdToText(lComponent.TypeName)] = lComponent.Payload;
                    }
                }

                nlohmann::json lEntityJson = nlohmann::json::object();
                lEntityJson[KEY_GUID]       = lEntity->Id.ToString().CStr();
                lEntityJson[KEY_NAME]       = lEntity->Name.CStr();
                lEntityJson[KEY_OWNER_MAP]  = IdToText(lEntity->OwnerMap);
                lEntityJson[KEY_COMPONENTS] = Move(lComponents);

                // Omitted for a root.
                if (lEntity->Parent.IsValid())
                {
                    lEntityJson[KEY_PARENT] = lEntity->Parent.ToString().CStr();
                }

                lEntities.emplace_back(Move(lEntityJson));
            }

            return lEntities;
        }
    }
}
