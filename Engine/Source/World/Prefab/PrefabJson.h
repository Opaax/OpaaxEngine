#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Prefab/PrefabData.h"
#include "World/Serialization/EntityJson.h"

namespace Opaax
{
    inline constexpr LogCategory LogPrefabJson{"PrefabJson"};

    // =============================================================================
    // PrefabJson — PrefabData <-> JSON. No file IO (PrefabFile), no World (PrefabFactory).
    //   The entity array is EntityJson's, shared with the map format.
    // =============================================================================
    namespace PrefabJson
    {
        /**
         * The format version this build writes, and the highest it reads.
         * Bump only for a change an older reader would misread (a new optional field does not count).
         *   v2: prefabInstances (nested placements, variants).
         *   v3: parent on entities (Transform becomes local).
         */
        inline constexpr Uint32 PREFAB_FORMAT_VERSION = 3;

        // Document keys. Entity and placement keys are EntityJson's.
        inline constexpr const char* KEY_VERSION   = EntityJson::KEY_VERSION;
        inline constexpr const char* KEY_ENTITIES  = EntityJson::KEY_ENTITIES;
        inline constexpr const char* KEY_INSTANCES = EntityJson::KEY_INSTANCES;

        /** Serializes InData. Entities sorted by Guid. */
        OPAAX_API nlohmann::json ToJson(const PrefabData& InData);

        /**
         * Parses InJson into OutData. Never throws; OutData is untouched on failure.
         * @return False if InJson is not an object, or its version is newer than PREFAB_FORMAT_VERSION
         */
        OPAAX_API bool FromJson(const nlohmann::json& InJson, PrefabData& OutData);

        /** ToJson + indented dump. */
        OPAAX_API OpaaxString Serialize(const PrefabData& InData);

        /** Same text, moving the component payloads instead of copying them. */
        OPAAX_API OpaaxString Serialize(PrefabData&& InData);

        /** Parses text (never throws), then FromJson. @return False on malformed JSON. */
        OPAAX_API bool Deserialize(const OpaaxString& InText, PrefabData& OutData);
    }
}
