#pragma once

#include <nlohmann/json.hpp>

#include "Core/GUID/Guid.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "World/Entity/EntityTypes.h"

namespace Opaax
{
    // =============================================================================
    // MapData — a Map's entities as DATA, detached from any World.
    //
    //   The Map is the serialization unit of the World > Level > Map model: a World owns
    //   the one entt::registry, a Level composes Maps, and a Map is pure entity data with
    //   no systems and no runtime ownership. This is that data in memory.
    //
    //   It sits between the two halves of the snapshot core:
    //       World --MapSerializer::Capture--> MapData --MapFactory::Instantiate--> World
    //   and it is deliberately the ONLY thing those two share, so neither half needs to
    //   know how the other reaches a World.
    //
    //   NO FILE IO HERE. M3 builds the in-memory core only; the .opaaxmap reader/writer is
    //   M5, and it is what converts the interned ids below to their STRING forms — an
    //   OpaaxStringID is an intern-table index and is not stable across runs, so it must
    //   never be written to disk as a number.
    //
    //   Plain aggregates, no OPAAX_API: no vtable, no out-of-line members, nothing to export.
    // =============================================================================

    // One component's serialized form. Keyed by the registry's authoring NAME rather than
    // by entt's type id, because the id is a compile-time hash of a C++ type name — renaming
    // the C++ type would silently orphan every component already written.
    struct ComponentData
    {
        OpaaxStringID  TypeName;
        nlohmann::json Payload;
    };

    // One entity: its identity (which the round trip must preserve exactly) plus whatever
    // registered components it carried. Identity is stored FLAT here rather than as an
    // EntityMeta entry in Components — EntityMeta is what an EntityData *is*, so nesting it
    // would put the entity's identity inside its own payload.
    struct EntityData
    {
        Guid                     Id;
        OpaaxString              Name;
        MapId                    OwnerMap;
        TDynArray<ComponentData> Components;
    };

    // One instance entity's deviation from its template. Kept as a LIST rather than a map because
    // it is written as a json object keyed by the guid's text, which nlohmann already sorts
    // (object_t is a std::map) — so the file's order is stable without a comparator on Guid.
    struct PrefabOverrideEntry
    {
        Guid           TemplateGuid;   // which entity OF the prefab
        nlohmann::json Patch;          // PrefabOverrides' form: { components: {...}, name: "..." }
    };

    // =============================================================================
    // PrefabInstanceRecord — ONE placement of a prefab, as a map file stores it (⑦-C **K3**).
    //
    //   THE ENTITIES ARE NOT HERE, and that is the whole point. A map records the LINK plus the
    //   deltas, exactly as o3de, Unity and Godot do, so an edit to the prefab reaches every
    //   instance the next time the map is read — with no propagation machinery anywhere.
    //
    //   It lives in MapData rather than behind a fold/expand step inside the json layer, which
    //   keeps `MapJson` a pure transformation: the format layer reads and writes this field like
    //   any other, and knows nothing about prefabs. Turning entities into records and back is
    //   `PrefabFold`'s job, called at the two moments that actually have a prefab to consult.
    // =============================================================================
    struct PrefabInstanceRecord
    {
        /** Asset-relative ("Prefabs/Turret.opaaxprefab"). */
        OpaaxString Prefab;

        /** Identifies this placement; the left half of Guid::Derive (**K2**). */
        Guid InstanceId;

        /** Only the entities that deviate. An untouched instance carries none. */
        TDynArray<PrefabOverrideEntry> Overrides;
    };

    struct MapData
    {
        /**
         * WHICH MAP THIS IS — the map naming ITSELF (**MP10**).
         *
         * It used to be derived from the entities alone (`OwnerId()` below), which left a map
         * with none of them anonymous — and an invalid `MapId` is the value the whole layer
         * reads as "no filter, the whole world". So a map identified only by its contents could
         * not be an EMPTY map without becoming a hole every other map fell into.
         *
         * Settled at the boundary that has the information: `MapJson` reads the `mapId` key and
         * falls back to what the entities claim (files written before the key existed), and
         * `MapFile::Load` falls back once more to the file's stem, since it is the only layer
         * holding the path. Everything above simply reads this and can assume it is valid.
         */
        MapId Id;

        TDynArray<EntityData> Entities;

        /**
         * The prefab placements this map holds, FOLDED (⑦-C P3).
         *
         * Empty in every MapData that has not been through `PrefabFold::Fold` — a capture, a PIE
         * clone snapshot (**WM6**) and an undo record all carry their instance entities expanded in
         * `Entities`, because none of them is a file and none has a prefab to consult. It is
         * populated on the way OUT to a `.opaaxmap` and consumed on the way back IN.
         */
        TDynArray<PrefabInstanceRecord> Instances;

        /** True only when there is nothing at all — no loose entities AND no placements. */
        bool   IsEmpty()      const noexcept { return Entities.empty() && Instances.empty(); }
        Uint64 EntityCount()  const noexcept { return static_cast<Uint64>(Entities.size()); }

        /** How many placements are folded here. Zero for anything that never went through Fold. */
        Uint64 InstanceCount() const noexcept { return static_cast<Uint64>(Instances.size()); }

        /**
         * WHICH MAP these entities claim to belong to — the first valid `OwnerMap` among them,
         * invalid when none does. The entities are the authority (**WM2**), which is why this
         * outranks the file's stem; `Id` outranks it in turn only by being the map's own word.
         *
         * MIGRATION PATH, not the answer: read `Id`. This exists so a map written before the
         * `mapId` key keeps identifying itself, and so a hand-edited file whose entities disagree
         * with its header can still be diagnosed.
         */
        MapId OwnerId() const noexcept
        {
            for (const EntityData& lEntity : Entities)
            {
                if (lEntity.OwnerMap.IsValid()) { return lEntity.OwnerMap; }
            }

            return MapId();
        }
    };
}
