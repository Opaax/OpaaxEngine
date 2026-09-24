#include "World/Serialization/MapJson.h"

#include <algorithm>
#include <type_traits>

namespace Opaax
{
    namespace
    {
        // ToJson's body, shared by the copying and the CONSUMING entry points.
        //
        // TData is a forwarding reference: an lvalue MapData deduces MapData&, a temporary deduces
        // MapData. Only the second may move, and moving is the point — a component payload is a
        // whole json tree, and every caller on the hot paths hands over a capture it then drops.
        //
        // The entity array itself is EntityJson's (⑦-C P1a); what is left here is what makes this
        // a MAP file rather than any other document holding entities.
        template<typename TData>
        nlohmann::json BuildJson(TData&& InData)
        {
            constexpr bool k_Consume = !std::is_lvalue_reference_v<TData>;

            nlohmann::json lEntities;
            if constexpr (k_Consume)
            {
                lEntities = EntityJson::EntitiesToJson(Move(InData.Entities));
            }
            else
            {
                lEntities = EntityJson::EntitiesToJson(InData.Entities);
            }

            // Built by subscript, not by initializer list — see EntitiesToJson for why.
            nlohmann::json lRoot = nlohmann::json::object();
            lRoot[MapJson::KEY_VERSION]  = MapJson::MAP_FORMAT_VERSION;
            lRoot[MapJson::KEY_MAP_ID]   = EntityJson::IdToText(InData.Id);
            lRoot[MapJson::KEY_ENTITIES] = Move(lEntities);

            // OMITTED WHEN EMPTY, which is what keeps every existing map byte-identical until it
            // actually holds a placement — so MP6's round-trip gate does not light up across the
            // whole project the moment this ships.
            if (!InData.Instances.empty())
            {
                lRoot[MapJson::KEY_INSTANCES] = EntityJson::InstancesToJson(InData.Instances);
            }

            return lRoot;
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
        const MapId lDeclaredId = EntityJson::IdFromText(EntityJson::ReadString(InJson, KEY_MAP_ID));

        MapData lParsed;

        // ⑦-C P3. Read BEFORE the early-out below: a map may legitimately hold nothing but
        // placements — every entity folded away — and losing them because there was no `entities`
        // array would be exactly the silent data loss the version bump exists to prevent.
        EntityJson::InstancesFromJson(InJson, lParsed.Instances);

        const auto lEntitiesIt = InJson.find(KEY_ENTITIES);
        if (lEntitiesIt == InJson.end() || !lEntitiesIt->is_array())
        {
            // A map with no entities array is EMPTY, not broken: an author who saves a cleared
            // world must get a file that opens back to a cleared world — and it still knows which
            // map it is, which is the whole point of the key.
            OutData.Entities.clear();
            OutData.Instances = Move(lParsed.Instances);
            OutData.Id = lDeclaredId;
            return true;
        }

        const Uint64 lSkipped = EntityJson::EntitiesFromJson(*lEntitiesIt, lParsed.Entities);

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
        return EntityJson::DumpToString(BuildJson(InData), k_FileIndent);
    }

    OpaaxString MapJson::Serialize(MapData&& InData)
    {
        return EntityJson::DumpToString(BuildJson(Move(InData)), k_FileIndent);
    }

    OpaaxString MapJson::SerializeCompact(const MapData& InData)
    {
        return EntityJson::DumpToString(BuildJson(InData), k_CompactIndent);
    }

    OpaaxString MapJson::SerializeCompact(MapData&& InData)
    {
        return EntityJson::DumpToString(BuildJson(Move(InData)), k_CompactIndent);
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
