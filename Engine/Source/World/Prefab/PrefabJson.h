#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Prefab/PrefabData.h"
#include "World/Serialization/EntityJson.h"

namespace Opaax
{
    inline constexpr LogCategory LogPrefabJson{"PrefabJson"};

    // =============================================================================
    // PrefabJson — the TEXT form of a Prefab: PrefabData <-> json. No file IO (that is
    //   PrefabFile), no World (that is PrefabFactory). MapJson's shape, one document over.
    //
    //   The entity array itself is EntityJson's, shared with the map format so the two cannot
    //   drift (⑦-C **P1a**). What is left here is the two lines that make this a PREFAB file:
    //   its own version, and the absence of a `mapId`.
    // =============================================================================
    namespace PrefabJson
    {
        /**
         * The format this build writes, and the highest it will read.
         *
         * Bumped ONLY for a change a v1 reader would MISREAD — the rule MAP_FORMAT_VERSION
         * follows. Adding a field does not qualify, since FromJson ignores what it does not know.
         *
         * EXPECTED TO MOVE AT ⑦-C P3: an instance record a v1 reader skipped would produce a
         * prefab that is silently missing entities, which is exactly what the rule catches.
         */
        inline constexpr Uint32 PREFAB_FORMAT_VERSION = 1;

        // The document-level keys. Everything describing an ENTITY is EntityJson's.
        inline constexpr const char* KEY_VERSION  = EntityJson::KEY_VERSION;
        inline constexpr const char* KEY_ENTITIES = EntityJson::KEY_ENTITIES;

        /** Serialize InData. Entities sorted by Guid, for EntityJson::EntitiesToJson's reasons. */
        OPAAX_API nlohmann::json ToJson(const PrefabData& InData);

        /**
         * Parse InJson into OutData.
         *
         * TOLERANT AND TOTAL — never throws, exactly as MapJson::FromJson is (**MP3**), and
         * OutData IS LEFT UNTOUCHED ON FAILURE so a failed read cannot half-replace a prefab the
         * caller already held and then be written back over the original.
         *
         * @return false when InJson is not an object, or when its version is NEWER than
         *   PREFAB_FORMAT_VERSION.
         */
        OPAAX_API bool FromJson(const nlohmann::json& InJson, PrefabData& OutData);

        /** ToJson + dump, indented — a prefab file lives in git and a human reads the diff. */
        OPAAX_API OpaaxString Serialize(const PrefabData& InData);

        /** Serialize by CONSUMING InData — identical bytes, component payloads moved not copied. */
        OPAAX_API OpaaxString Serialize(PrefabData&& InData);

        /** Parse text (never throws) then FromJson. @return false on malformed json. */
        OPAAX_API bool Deserialize(const OpaaxString& InText, PrefabData& OutData);
    }
}
