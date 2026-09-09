#pragma once

#include <nlohmann/json.hpp>

#include <algorithm>
#include <type_traits>

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    inline constexpr LogCategory LogEntityJson{"EntityJson"};

    // =============================================================================
    // EntityJson — the ENTITY ARRAY as json, shared by every format that stores entities.
    //
    //   A Map is pure entity data (**WM1**) and a prefab is that same data without a map's
    //   membership, so the two formats must not own two writers — that is the drift MapFile.h
    //   warns about, one layer down. MapJson keeps what a MAP file is (its version key, its
    //   `mapId`); this owns what an ENTITY is, and both formats call it.
    //
    //   SPLIT OUT OF MapJson (⑦-C P1a), NOT WRITTEN FRESH. The bodies here are that file's,
    //   moved unchanged — which is the whole reason the extraction is safe to make: the
    //   existing byte-exact gates (MP6's round trip, MapJsonTests, MapRestoreTests) prove it
    //   cost nothing, and they could not prove that about a reimplementation.
    // =============================================================================
    namespace EntityJson
    {
        // =====================================================================
        // Keys
        //
        // Named constants rather than literals: the reader and the writer must agree, and a
        // typo in one of them is a silently-empty field rather than a compile error.
        // =====================================================================
        inline constexpr const char* KEY_VERSION    = "version";
        inline constexpr const char* KEY_ENTITIES   = "entities";
        inline constexpr const char* KEY_GUID       = "guid";
        inline constexpr const char* KEY_NAME       = "name";
        inline constexpr const char* KEY_OWNER_MAP  = "ownerMap";
        inline constexpr const char* KEY_COMPONENTS = "components";

        // =====================================================================
        // Dump forms
        //
        // A file is indented because it lives in git and a human reads the diff. The
        // comparison form is not a file and has no reader — see MapJson::SerializeCompact.
        // =====================================================================
        inline constexpr int k_FileIndent    =  4;
        inline constexpr int k_CompactIndent = -1;   // nlohmann: negative => no whitespace

        // =====================================================================
        // Interned ids as text (**MP1**)
        // =====================================================================
        /**
         * An interned id as TEXT (**WM2**).
         *
         * The IsValid() guard is about what the FILE SAYS, not about the round trip. ToString()
         * answers "None" for an invalid id, and the round trip would in fact survive that —
         * OpaaxStringIDPool reserves index 0 for "None", so re-interning that text yields the
         * invalid id right back. What it would NOT survive is a human reading it: an entity no
         * map authored would claim to belong to a map called "None", and "" is simply the
         * truthful encoding of "runtime-spawned".
         *
         * CStr(), not ToString(): the pool's bytes outlive the call and every caller hands them
         * straight to json, so returning an OpaaxString would be a heap round trip per id per
         * entity for text nobody keeps.
         */
        OPAAX_API const char* IdToText(OpaaxStringID InId);

        /**
         * Empty text means INVALID, which for a MapId means runtime-spawned (**WM2**) — the exact
         * value an entity no map authored carries, so the round trip is closed. OpaaxStringID's
         * own ctor already maps empty to ID_None; this states it at the format boundary, where it
         * is a guarantee the file makes rather than a coincidence of the string type.
         */
        OPAAX_API OpaaxStringID IdFromText(const OpaaxString& InText);

        /**
         * Read a string field, tolerantly.
         *
         * nlohmann throws on a type mismatch, so every read goes through here — one hand-edited
         * field cannot take down a load (**MP3**).
         *
         * @return Empty when the key is absent or is not a string.
         */
        OPAAX_API OpaaxString ReadString(const nlohmann::json& InJson, const char* InKey);

        /**
         * dump() straight into an OpaaxString, with the LENGTH carried across.
         *
         * OpaaxString(const char*) would strlen a buffer whose size we are already holding —
         * half a megabyte of it for a 1k-entity map.
         */
        OPAAX_API OpaaxString DumpToString(const nlohmann::json& InJson, int InIndent);

        // =====================================================================
        // The walk
        // =====================================================================
        /**
         * Parse a json ARRAY of entity objects, appending to OutEntities.
         *
         * TOLERANT AND TOTAL — never throws. A missing field takes its default, an unknown field
         * is ignored, and an entity whose guid is missing or unparseable is SKIPPED rather than
         * silently given a fresh identity: a fabricated Guid would retarget every reference that
         * pointed at it (**WM3**).
         *
         * APPENDS rather than replaces, so a caller assembling one entity list from several
         * sources needs no second entry point. It does NOT log the skip count — the caller does,
         * because only the caller can name the document it was reading.
         *
         * @return How many entries were skipped. 0 means every entity in InEntities was read.
         */
        OPAAX_API Uint64 EntitiesFromJson(const nlohmann::json& InEntities, TDynArray<EntityData>& OutEntities);

        /**
         * Build the json ARRAY form of InEntities.
         *
         * ENTITIES ARE SORTED BY GUID, which is a decision rather than a detail:
         *   - a map file lives in git, and entt's view order is storage order — an unsorted write
         *     would reshuffle the whole file every time an entity was destroyed;
         *   - the editor's dirty check compares a fresh capture against the last written text, and
         *     an order-dependent comparison would report "dirty" for a world nobody edited.
         * Nothing reads a map in order — MapFactory::Instantiate looks every component up by name —
         * so the ordering is free to serve the two consumers that do care.
         *
         * Components are written as an OBJECT keyed by authoring name: a duplicate type on one
         * entity becomes unrepresentable instead of merely unlikely.
         *
         * TEntities is a forwarding reference: an lvalue deduces `TDynArray<EntityData>&`, a
         * temporary deduces the value type. Only the second may MOVE, and moving is the point — a
         * component payload is a whole json tree, and every caller on the hot paths hands over a
         * capture it then drops.
         *
         * OBJECTS ARE BUILT BY SUBSCRIPT, not by initializer list. `json{ {k,v}, ... }` cannot know
         * it is an object until the list is complete, so it first builds a json ARRAY of
         * two-element json ARRAYS and then rebuilds that as an object — roughly three times the
         * cost of the assignments below, for identical bytes. The keys still land sorted: json's
         * object_t is a std::map, so insertion order is not what the file records.
         *
         * Header-only template, no OPAAX_API (**I6**).
         */
        template<typename TEntities>
        nlohmann::json EntitiesToJson(TEntities&& InEntities)
        {
            constexpr bool k_Consume = !std::is_lvalue_reference_v<TEntities>;
            using TEntity = std::conditional_t<k_Consume, EntityData, const EntityData>;

            // Sorting a COPY of the pointers leaves the entity ORDER in InEntities untouched: a
            // serializer that reordered its input would be a surprise to the next caller.
            TDynArray<TEntity*> lOrdered;
            lOrdered.reserve(InEntities.size());
            for (TEntity& lEntity : InEntities)
            {
                lOrdered.emplace_back(&lEntity);
            }

            std::sort(lOrdered.begin(), lOrdered.end(),
                [](const EntityData* InLeft, const EntityData* InRight)
                {
                    // High then Low — the same big-endian order Guid::ToString writes, so the file
                    // reads as sorted by its own guid column.
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

                lEntities.emplace_back(Move(lEntityJson));
            }

            return lEntities;
        }
    }
}
