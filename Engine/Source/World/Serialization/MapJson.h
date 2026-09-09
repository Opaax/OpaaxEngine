#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Serialization/EntityJson.h"
#include "World/Serialization/MapData.h"

namespace Opaax
{
    inline constexpr LogCategory LogMapJson{"MapJson"};

    // =============================================================================
    // MapJson — the TEXT form of a Map: MapData <-> json. No file IO (that is MapFile),
    //   no World (that is MapSerializer/MapFactory). One transformation, in one place.
    //
    //   This is the layer M3 deliberately left for M5, and it exists because the in-memory
    //   MapData is NOT directly writable: an OpaaxStringID is an intern-table INDEX, so
    //   writing one as a number produces a file that means something different on the next
    //   run (WM2). Everything interned is written as its STRING here, and nowhere else — with
    //   the INVALID id written as "", not as the "None" its ToString() answers (see IdToText).
    //
    //   Free functions in a namespace, not a class: there is no state, no overload set to
    //   group and nothing to derive from. MapSerializer/MapFactory are classes-of-statics
    //   for symmetry with each other; this has no partner to be symmetric with.
    // =============================================================================
    namespace MapJson
    {
        /**
         * The format this build writes, and the highest it will read.
         *
         * Bumped ONLY for a change a v1 reader would misread. Adding a field does not qualify —
         * FromJson ignores what it does not know, so an older build opening a newer map loses
         * the new field and nothing else, which is the same forward-compatibility MapFactory
         * already gives unknown COMPONENTS.
         */
        inline constexpr Uint32 MAP_FORMAT_VERSION = 1;

        // ---- keys ---------------------------------------------------------------
        // `mapId` is the only key a MAP owns; everything else describes an ENTITY and is
        // EntityJson's, shared with the prefab format so the two cannot drift (⑦-C P1a).
        // Re-exported rather than re-declared: one definition, and no caller moved.
        inline constexpr const char* KEY_MAP_ID      = "mapId";

        inline constexpr const char* KEY_VERSION     = EntityJson::KEY_VERSION;
        inline constexpr const char* KEY_ENTITIES    = EntityJson::KEY_ENTITIES;
        inline constexpr const char* KEY_GUID        = EntityJson::KEY_GUID;
        inline constexpr const char* KEY_NAME        = EntityJson::KEY_NAME;
        inline constexpr const char* KEY_OWNER_MAP   = EntityJson::KEY_OWNER_MAP;
        inline constexpr const char* KEY_COMPONENTS  = EntityJson::KEY_COMPONENTS;

        // ---- dump forms ---------------------------------------------------------
        inline constexpr int k_FileIndent    = EntityJson::k_FileIndent;
        inline constexpr int k_CompactIndent = EntityJson::k_CompactIndent;

        /**
         * Serialize InData.
         *
         * ENTITIES ARE SORTED BY GUID, which is a decision rather than a detail:
         *   - a map file lives in git, and entt's view order is storage order — an unsorted
         *     write would reshuffle the whole file every time an entity was destroyed;
         *   - the editor's dirty check (M5 S5) compares a fresh capture against the last
         *     written text, and an order-dependent comparison would report "dirty" for a world
         *     nobody edited.
         * Nothing reads a map in order — MapFactory::Instantiate looks every component up by
         * name — so the ordering is free to serve the two consumers that do care.
         *
         * Components are written as an OBJECT keyed by authoring name: a duplicate type on one
         * entity becomes unrepresentable instead of merely unlikely, and nlohmann's ordering
         * keeps the keys stable for the same diff reason.
         *
         * `mapId` IS ALWAYS WRITTEN, including as "" for a capture that named no map (a PIE
         * clone, which never reaches a file). A map that only sometimes says what it is would put
         * the reader back to guessing exactly where guessing was the bug (**MP10**).
         */
        OPAAX_API nlohmann::json ToJson(const MapData& InData);

        /**
         * Parse InJson into OutData.
         *
         * TOLERANT AND TOTAL — never throws. nlohmann's accessors throw on a type mismatch, so
         * the whole walk is guarded; a malformed map is an ordinary `false`, because "the user
         * hand-edited a file" is a normal thing to survive, not an exceptional one.
         *
         * A missing field takes its default. An unknown field is ignored. An entity whose guid
         * is missing or unparseable is SKIPPED with a warning rather than silently given a
         * fresh identity — a fabricated Guid would break every reference that pointed at it.
         *
         * @return false when InJson is not an object, or when its version is NEWER than
         *   MAP_FORMAT_VERSION. Refusing the future is deliberate: half-reading a format whose
         *   meaning has changed produces a plausible-looking wrong world, and the next save
         *   would write that back over the original.
         */
        OPAAX_API bool FromJson(const nlohmann::json& InJson, MapData& OutData);

        /** ToJson + dump, indented — a map file is meant to be readable and diffable. */
        OPAAX_API OpaaxString Serialize(const MapData& InData);

        /**
         * Serialize InData by CONSUMING it — identical bytes, but the component payloads move into
         * the json instead of being deep-copied. Each payload is a whole json tree, so on a capture
         * the caller is about to drop that copy is most of what serializing costs.
         *
         * Prefer this wherever the MapData is a temporary; the const& overload stays for the callers
         * that still need theirs afterwards (MapFile::Save logs its entity count).
         */
        OPAAX_API OpaaxString Serialize(MapData&& InData);

        /**
         * The COMPARISON form: the same json, dumped with no whitespace.
         *
         * NOT A FILE FORMAT — nothing writes this and nothing parses it. It exists for the editor's
         * dirty check, which only ever asks "same or not" and pays for every space it does not read:
         * indenting a 1k-entity map takes it from 231 KB to 542 KB, and the dump with it.
         *
         * A baseline built with this may only ever be compared against text built with this.
         */
        OPAAX_API OpaaxString SerializeCompact(const MapData& InData);
        OPAAX_API OpaaxString SerializeCompact(MapData&& InData);

        /** Parse text (never throws) then FromJson. @return false on malformed json. */
        OPAAX_API bool Deserialize(const OpaaxString& InText, MapData& OutData);
    }
}
