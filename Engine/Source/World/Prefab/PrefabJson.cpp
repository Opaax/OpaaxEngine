#include "World/Prefab/PrefabJson.h"

#include <type_traits>

namespace Opaax
{
    namespace
    {
        // ToJson's body, shared by the copying and the CONSUMING entry points — MapJson's shape.
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

            nlohmann::json lRoot = nlohmann::json::object();
            lRoot[PrefabJson::KEY_VERSION]  = PrefabJson::PREFAB_FORMAT_VERSION;
            lRoot[PrefabJson::KEY_ENTITIES] = Move(lEntities);

            return lRoot;
        }
    }

    nlohmann::json PrefabJson::ToJson(const PrefabData& InData)
    {
        return BuildJson(InData);
    }

    bool PrefabJson::FromJson(const nlohmann::json& InJson, PrefabData& OutData)
    {
        if (!InJson.is_object())
        {
            OPAAX_LOG(LogPrefabJson, Error, "Prefab is not a json object — refusing");
            return false;
        }

        const auto   lVersionIt = InJson.find(KEY_VERSION);
        const Uint32 lVersion   = (lVersionIt != InJson.end() && lVersionIt->is_number_unsigned())
                                      ? lVersionIt->get<Uint32>()
                                      : 0u;

        if (lVersion > PREFAB_FORMAT_VERSION)
        {
            OPAAX_LOG(LogPrefabJson, Error,
                      "Prefab format version {} is newer than this build reads ({}) — refusing rather "
                      "than half-reading it", lVersion, PREFAB_FORMAT_VERSION);
            return false;
        }

        const auto lEntitiesIt = InJson.find(KEY_ENTITIES);
        if (lEntitiesIt == InJson.end() || !lEntitiesIt->is_array())
        {
            // An EMPTY prefab is a real thing, not a broken one — the state a freshly created
            // prefab is in before anything is put in it.
            OutData.Entities.clear();
            return true;
        }

        PrefabData   lParsed;
        const Uint64 lSkipped = EntityJson::EntitiesFromJson(*lEntitiesIt, lParsed.Entities);

        if (lSkipped > 0)
        {
            OPAAX_LOG(LogPrefabJson, Warn, "Skipped {} entity(ies) with a missing or malformed guid",
                      lSkipped);
        }

        OutData = Move(lParsed);
        return true;
    }

    OpaaxString PrefabJson::Serialize(const PrefabData& InData)
    {
        return EntityJson::DumpToString(BuildJson(InData), EntityJson::k_FileIndent);
    }

    OpaaxString PrefabJson::Serialize(PrefabData&& InData)
    {
        return EntityJson::DumpToString(BuildJson(Move(InData)), EntityJson::k_FileIndent);
    }

    bool PrefabJson::Deserialize(const OpaaxString& InText, PrefabData& OutData)
    {
        // No exceptions — a hand-edited file is an ordinary input here, not an exceptional one.
        const nlohmann::json lJson = nlohmann::json::parse(InText.CStr(), nullptr, false);

        if (lJson.is_discarded())
        {
            OPAAX_LOG(LogPrefabJson, Error, "Prefab text is not valid json — refusing");
            return false;
        }

        return FromJson(lJson, OutData);
    }
}
