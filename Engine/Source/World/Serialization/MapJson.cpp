#include "World/Serialization/MapJson.h"

#include <algorithm>
#include <type_traits>

namespace Opaax
{
    namespace
    {
        // Shared by the copying and consuming versions (moves the payloads for a temporary).
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

            // Built by subscript (see EntitiesToJson).
            nlohmann::json lRoot = nlohmann::json::object();
            lRoot[MapJson::KEY_VERSION]  = MapJson::MAP_FORMAT_VERSION;
            lRoot[MapJson::KEY_MAP_ID]   = EntityJson::IdToText(InData.Id);
            lRoot[MapJson::KEY_ENTITIES] = Move(lEntities);

            // Omitted when empty.
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

        // The map's name first. Old files have none; the entities are asked afterwards.
        const MapId lDeclaredId = EntityJson::IdFromText(EntityJson::ReadString(InJson, KEY_MAP_ID));

        MapData lParsed;

        // Read before the early-out: a map may hold only placements.
        EntityJson::InstancesFromJson(InJson, lParsed.Instances);

        const auto lEntitiesIt = InJson.find(KEY_ENTITIES);
        if (lEntitiesIt == InJson.end() || !lEntitiesIt->is_array())
        {
            // No entities array: an empty map (still named).
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

        // The declared id wins; the entities are the fallback for old files.
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
        // No exceptions: malformed text gives a discarded value.
        const nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);

        if (lJson.is_discarded())
        {
            OPAAX_LOG(LogMapJson, Error, "Map text is not valid json — refusing");
            return false;
        }

        return FromJson(lJson, OutData);
    }
}
