#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"

#include "World/Serialization/EntityJson.h"
#include "World/Serialization/MapData.h"

namespace Opaax
{
    inline constexpr LogCategory LogMapJson{"MapJson"};

    // =============================================================================
    // MapJson — MapData <-> JSON. No file IO (MapFile), no World (MapSerializer/MapFactory).
    //   Interned ids are written as strings; an invalid id as "".
    // =============================================================================
    namespace MapJson
    {
        /**
         * The format version this build writes, and the highest it reads.
         * Bump only for a change an older reader would misread (a new optional field does not count).
         */
        inline constexpr Uint32 MAP_FORMAT_VERSION = 4;

        /**
         * v2: prefabInstances (an older reader would silently drop placed entities).
         */
        inline constexpr Uint32 MAP_FORMAT_VERSION_PREFABS = 2;

        /**
         * v3: a deleted instance entity is saved as a null patch.
         */
        inline constexpr Uint32 MAP_FORMAT_VERSION_PREFAB_REMOVALS = 3;

        /**
         * v4: parent on entities (Transform becomes local). Omitted for a root.
         */
        inline constexpr Uint32 MAP_FORMAT_VERSION_PARENTS = 4;

        // ---- keys ---------------------------------------------------------------
        // mapId is the only map key; entity keys are EntityJson's (re-exported).
        inline constexpr const char* KEY_MAP_ID      = "mapId";

        // Placement keys (EntityJson's, re-exported).
        inline constexpr const char* KEY_INSTANCES    = EntityJson::KEY_INSTANCES;
        inline constexpr const char* KEY_PREFAB       = EntityJson::KEY_PREFAB;
        inline constexpr const char* KEY_INSTANCE_ID  = EntityJson::KEY_INSTANCE_ID;
        inline constexpr const char* KEY_OVERRIDES    = EntityJson::KEY_OVERRIDES;

        inline constexpr const char* KEY_VERSION     = EntityJson::KEY_VERSION;
        inline constexpr const char* KEY_ENTITIES    = EntityJson::KEY_ENTITIES;
        inline constexpr const char* KEY_GUID        = EntityJson::KEY_GUID;
        inline constexpr const char* KEY_NAME        = EntityJson::KEY_NAME;
        inline constexpr const char* KEY_OWNER_MAP   = EntityJson::KEY_OWNER_MAP;
        inline constexpr const char* KEY_PARENT      = EntityJson::KEY_PARENT;
        inline constexpr const char* KEY_COMPONENTS  = EntityJson::KEY_COMPONENTS;

        // ---- dump forms ---------------------------------------------------------
        inline constexpr int k_FileIndent    = EntityJson::k_FileIndent;
        inline constexpr int k_CompactIndent = EntityJson::k_CompactIndent;

        /**
         * Serializes InData. Entities sorted by guid (stable in git, needed by the dirty check).
         * Components are an object keyed by name. mapId is always written ("" for a non-map capture).
         */
        OPAAX_API nlohmann::json ToJson(const MapData& InData);

        /**
         * Parses InJson into OutData. Never throws; OutData is untouched on failure.
         * A missing field takes its default; an unknown field is ignored; an entity with a missing or
         * bad guid is skipped with a warning.
         * @return False if InJson is not an object, or its version is newer than MAP_FORMAT_VERSION
         */
        OPAAX_API bool FromJson(const nlohmann::json& InJson, MapData& OutData);

        /** ToJson + indented dump. */
        OPAAX_API OpaaxString Serialize(const MapData& InData);

        /**
         * Same text, moving the component payloads instead of copying them. Use for temporaries.
         */
        OPAAX_API OpaaxString Serialize(MapData&& InData);

        /**
         * Same JSON without whitespace, for the editor's dirty check only (not a file format).
         * Compare it only with text built the same way.
         */
        OPAAX_API OpaaxString SerializeCompact(const MapData& InData);
        OPAAX_API OpaaxString SerializeCompact(MapData&& InData);

        /** Parses text (never throws), then FromJson. @return False on malformed JSON. */
        OPAAX_API bool Deserialize(const OpaaxString& InText, MapData& OutData);
    }
}
