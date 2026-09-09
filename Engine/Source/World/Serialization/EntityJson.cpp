#include "World/Serialization/EntityJson.h"

namespace Opaax
{
    const char* EntityJson::IdToText(OpaaxStringID InId)
    {
        return InId.IsValid() ? InId.CStr() : "";
    }

    OpaaxStringID EntityJson::IdFromText(const OpaaxString& InText)
    {
        return InText.IsEmpty() ? OpaaxStringID() : OpaaxStringID(InText);
    }

    OpaaxString EntityJson::ReadString(const nlohmann::json& InJson, const char* InKey)
    {
        const auto lIt = InJson.find(InKey);
        if (lIt == InJson.end() || !lIt->is_string())
        {
            return OpaaxString();
        }

        const std::string lText = lIt->get<std::string>();

        return OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size()));
    }

    OpaaxString EntityJson::DumpToString(const nlohmann::json& InJson, int InIndent)
    {
        const std::string lText = InJson.dump(InIndent);

        return OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size()));
    }

    Uint64 EntityJson::EntitiesFromJson(const nlohmann::json& InEntities, TDynArray<EntityData>& OutEntities)
    {
        Uint64 lSkipped = 0;

        for (const nlohmann::json& lEntityJson : InEntities)
        {
            if (!lEntityJson.is_object()) { ++lSkipped; continue; }

            EntityData lEntity;

            // Identity first, and it is the one field with no default. A missing or malformed
            // guid cannot be invented — a fresh one would silently retarget every reference
            // that pointed at this entity (**WM3**) — so the entity is dropped instead.
            if (!Guid::FromString(ReadString(lEntityJson, KEY_GUID), lEntity.Id))
            {
                ++lSkipped;
                continue;
            }

            lEntity.Name     = ReadString(lEntityJson, KEY_NAME);
            lEntity.OwnerMap = IdFromText(ReadString(lEntityJson, KEY_OWNER_MAP));

            const auto lComponentsIt = lEntityJson.find(KEY_COMPONENTS);
            if (lComponentsIt != lEntityJson.end() && lComponentsIt->is_object())
            {
                for (const auto& [lTypeName, lPayload] : lComponentsIt->items())
                {
                    if (lTypeName.empty()) { continue; }   // no name to look up in the registry

                    lEntity.Components.emplace_back(OpaaxStringID(lTypeName), lPayload);
                }
            }

            OutEntities.emplace_back(Move(lEntity));
        }

        return lSkipped;
    }
}
