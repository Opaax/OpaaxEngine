#include "World/Serialization/MapJson.h"

#include <algorithm>

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
        OpaaxString IdToText(OpaaxStringID InId)
        {
            return InId.IsValid() ? InId.ToString() : OpaaxString();
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
    }

    nlohmann::json MapJson::ToJson(const MapData& InData)
    {
        // Sorted by Guid — see the header for why this is load-bearing rather than tidy. Sorting
        // a COPY of the pointers leaves InData untouched: capture is read-only everywhere else,
        // and a serializer that reorders its input would be a surprise to the next caller.
        TDynArray<const EntityData*> lOrdered;
        lOrdered.reserve(InData.Entities.size());
        for (const EntityData& lEntity : InData.Entities)
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
        for (const EntityData* lEntity : lOrdered)
        {
            nlohmann::json lComponents = nlohmann::json::object();
            for (const ComponentData& lComponent : lEntity->Components)
            {
                lComponents[IdToText(lComponent.TypeName).CStr()] = lComponent.Payload;
            }

            lEntities.push_back(nlohmann::json{
                { KEY_GUID,       lEntity->Id.ToString().CStr() },
                { KEY_NAME,       lEntity->Name.CStr()          },
                { KEY_OWNER_MAP,  IdToText(lEntity->OwnerMap).CStr() },
                { KEY_COMPONENTS, Move(lComponents)             }
            });
        }

        return nlohmann::json{
            { KEY_VERSION,  MAP_FORMAT_VERSION },
            { KEY_ENTITIES, Move(lEntities)    }
        };
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

        const auto lEntitiesIt = InJson.find(KEY_ENTITIES);
        if (lEntitiesIt == InJson.end() || !lEntitiesIt->is_array())
        {
            // A map with no entities array is EMPTY, not broken: an author who saves a cleared
            // world must get a file that opens back to a cleared world.
            OutData.Entities.clear();
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

        OutData = Move(lParsed);
        return true;
    }

    OpaaxString MapJson::Serialize(const MapData& InData)
    {
        return OpaaxString(ToJson(InData).dump(4).c_str());
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
