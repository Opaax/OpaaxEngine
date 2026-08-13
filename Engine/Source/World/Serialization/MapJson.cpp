#include "World/Serialization/MapJson.h"

#include <algorithm>
#include <type_traits>

namespace Opaax
{
    namespace
    {
        // An interned id as TEXT (WM2).
        //
        // The IsValid() guard is about what the FILE SAYS, not about the round trip. ToString()
        // answers "None" for an invalid id, and the round trip would in fact survive that —
        // OpaaxStringIDPool reserves index 0 for "None", so re-interning that text yields the
        // invalid id right back. What it would NOT survive is a human reading it: an entity no
        // map authored would claim to belong to a map called "None", and "" is simply the
        // truthful encoding of "runtime-spawned".
        //
        // CStr(), not ToString(): the pool's bytes outlive the call and every caller here hands
        // them straight to json, so the OpaaxString this used to return was a heap round trip
        // per id per entity for text nobody kept.
        const char* IdToText(OpaaxStringID InId)
        {
            return InId.IsValid() ? InId.CStr() : "";
        }

        // Empty text means INVALID, which for a MapId means runtime-spawned (WM2) — the exact
        // value an entity no map authored carries, so the round trip is closed. OpaaxStringID's
        // own ctor already maps empty to ID_None; this states it at the format boundary, where
        // it is a guarantee the file makes rather than a coincidence of the string type.
        OpaaxStringID IdFromText(const std::string& InText)
        {
            return InText.empty() ? OpaaxStringID() : OpaaxStringID(InText);
        }

        // nlohmann throws on a type mismatch; every read goes through these so one hand-edited
        // field cannot take down the load.
        std::string ReadString(const nlohmann::json& InJson, const char* InKey)
        {
            const auto lIt = InJson.find(InKey);
            return (lIt != InJson.end() && lIt->is_string()) ? lIt->get<std::string>() : std::string();
        }

        // ToJson's body, shared by the copying and the CONSUMING entry points.
        //
        // TData is a forwarding reference: an lvalue MapData deduces MapData&, a temporary deduces
        // MapData. Only the second may move, and moving is the point — a component payload is a
        // whole json tree, and every caller on the hot paths hands over a capture it then drops.
        //
        // OBJECTS ARE BUILT BY SUBSCRIPT, not by initializer list. `json{ {k,v}, ... }` cannot know
        // it is an object until the list is complete, so it first builds a json ARRAY of two-element
        // json ARRAYS and then rebuilds that as an object — roughly three times the cost of the
        // assignments below, for identical bytes. The keys still land sorted: json's object_t is a
        // std::map, so insertion order is not what the file records.
        template<typename TData>
        nlohmann::json BuildJson(TData&& InData)
        {
            constexpr bool k_Consume = !std::is_lvalue_reference_v<TData>;
            using TEntity = std::conditional_t<k_Consume, EntityData, const EntityData>;

            // Sorted by Guid — see the header for why this is load-bearing rather than tidy.
            // Sorting a COPY of the pointers leaves the entity ORDER in InData untouched: a
            // serializer that reordered its input would be a surprise to the next caller.
            TDynArray<TEntity*> lOrdered;
            lOrdered.reserve(InData.Entities.size());
            for (TEntity& lEntity : InData.Entities)
            {
                lOrdered.push_back(&lEntity);
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

                nlohmann::json lEntityJson  = nlohmann::json::object();
                lEntityJson[MapJson::KEY_GUID]       = lEntity->Id.ToString().CStr();
                lEntityJson[MapJson::KEY_NAME]       = lEntity->Name.CStr();
                lEntityJson[MapJson::KEY_OWNER_MAP]  = IdToText(lEntity->OwnerMap);
                lEntityJson[MapJson::KEY_COMPONENTS] = Move(lComponents);

                lEntities.push_back(Move(lEntityJson));
            }

            nlohmann::json lRoot = nlohmann::json::object();
            lRoot[MapJson::KEY_VERSION]  = MapJson::MAP_FORMAT_VERSION;
            lRoot[MapJson::KEY_MAP_ID]   = IdToText(InData.Id);
            lRoot[MapJson::KEY_ENTITIES] = Move(lEntities);

            return lRoot;
        }

        // dump() straight into an OpaaxString, with the LENGTH carried across. OpaaxString(const
        // char*) would strlen a buffer whose size we are holding — half a megabyte of it for a
        // 1k-entity map.
        OpaaxString DumpToString(const nlohmann::json& InJson, int InIndent)
        {
            const std::string lText = InJson.dump(InIndent);

            return OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size()));
        }
    }

    nlohmann::json MapJson::ToJson(const MapData& InData)
    {
        return BuildJson(InData);
    }

    bool MapJson::FromJson(const nlohmann::json& InJson, MapData& OutData)
    {
        if (!InJson.is_object())
        {
            OPAAX_LOG(LogMapJson, Error, "Map is not a json object — refusing");
            return false;
        }

        const auto   lVersionIt = InJson.find(KEY_VERSION);
        const Uint32 lVersion   = (lVersionIt != InJson.end() && lVersionIt->is_number_unsigned())
                                      ? lVersionIt->get<Uint32>()
                                      : 0u;

        if (lVersion > MAP_FORMAT_VERSION)
        {
            OPAAX_LOG(LogMapJson, Error,
                      "Map format version {} is newer than this build reads ({}) — refusing rather than "
                      "half-reading it", lVersion, MAP_FORMAT_VERSION);
            return false;
        }

        // The map's own name, before its contents (**MP10**). A file written before the key
        // existed has none, and the entities are asked instead once they are parsed.
        const MapId lDeclaredId = IdFromText(ReadString(InJson, KEY_MAP_ID));

        const auto lEntitiesIt = InJson.find(KEY_ENTITIES);
        if (lEntitiesIt == InJson.end() || !lEntitiesIt->is_array())
        {
            // A map with no entities array is EMPTY, not broken: an author who saves a cleared
            // world must get a file that opens back to a cleared world — and it still knows which
            // map it is, which is the whole point of the key.
            OutData.Entities.clear();
            OutData.Id = lDeclaredId;
            return true;
        }

        MapData lParsed;
        Uint64  lSkipped = 0;

        for (const nlohmann::json& lEntityJson : *lEntitiesIt)
        {
            if (!lEntityJson.is_object()) { ++lSkipped; continue; }

            EntityData lEntity;

            // Identity first, and it is the one field with no default. A missing or malformed
            // guid cannot be invented — a fresh one would silently retarget every reference
            // that pointed at this entity (WM3) — so the entity is dropped instead.
            if (!Guid::FromString(OpaaxString(ReadString(lEntityJson, KEY_GUID).c_str()), lEntity.Id))
            {
                ++lSkipped;
                continue;
            }

            lEntity.Name     = OpaaxString(ReadString(lEntityJson, KEY_NAME).c_str());
            lEntity.OwnerMap = IdFromText(ReadString(lEntityJson, KEY_OWNER_MAP));

            const auto lComponentsIt = lEntityJson.find(KEY_COMPONENTS);
            if (lComponentsIt != lEntityJson.end() && lComponentsIt->is_object())
            {
                for (const auto& [lTypeName, lPayload] : lComponentsIt->items())
                {
                    if (lTypeName.empty()) { continue; }   // no name to look up in the registry

                    lEntity.Components.push_back(
                        ComponentData{ OpaaxStringID(lTypeName), lPayload });
                }
            }

            lParsed.Entities.push_back(Move(lEntity));
        }

        if (lSkipped > 0)
        {
            OPAAX_LOG(LogMapJson, Warn, "Skipped {} entity(ies) with a missing or malformed guid", lSkipped);
        }

        // DECLARED WINS, entities are the fallback. A map written by this build always says its
        // name; one written before the key did not, and its entities are the authority there
        // (**WM2**) — which is exactly the migration path that costs no format version bump, since
        // a v1 reader ignoring `mapId` derives the same answer for any map that has entities.
        lParsed.Id = lDeclaredId.IsValid() ? lDeclaredId : lParsed.OwnerId();

        OutData = Move(lParsed);
        return true;
    }

    OpaaxString MapJson::Serialize(const MapData& InData)
    {
        return DumpToString(BuildJson(InData), k_FileIndent);
    }

    OpaaxString MapJson::Serialize(MapData&& InData)
    {
        return DumpToString(BuildJson(Move(InData)), k_FileIndent);
    }

    OpaaxString MapJson::SerializeCompact(const MapData& InData)
    {
        return DumpToString(BuildJson(InData), k_CompactIndent);
    }

    OpaaxString MapJson::SerializeCompact(MapData&& InData)
    {
        return DumpToString(BuildJson(Move(InData)), k_CompactIndent);
    }

    bool MapJson::Deserialize(const OpaaxString& InText, MapData& OutData)
    {
        // parse(input, callback, allow_exceptions): no callback, and NO EXCEPTIONS — malformed
        // text comes back as a discarded value instead of throwing. A hand-edited file is an
        // ordinary input here, not an exceptional one.
        const nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);

        if (lJson.is_discarded())
        {
            OPAAX_LOG(LogMapJson, Error, "Map text is not valid json — refusing");
            return false;
        }

        return FromJson(lJson, OutData);
    }
}
