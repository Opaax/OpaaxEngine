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

            // The guid has no default: a new one would break every reference to this entity. Skip it.
            if (!Guid::FromString(ReadString(lEntityJson, KEY_GUID), lEntity.Id))
            {
                ++lSkipped;
                continue;
            }

            lEntity.Name     = ReadString(lEntityJson, KEY_NAME);
            lEntity.OwnerMap = IdFromText(ReadString(lEntityJson, KEY_OWNER_MAP));

            // Absent means root. Unreadable also means root, with a warning.
            if (const OpaaxString lParentText = ReadString(lEntityJson, KEY_PARENT);
                !lParentText.IsEmpty() && !Guid::FromString(lParentText, lEntity.Parent))
            {
                OPAAX_LOG(LogEntityJson, Warn, "Entity '{}' names an unreadable parent '{}' — read as a root",
                          lEntity.Name.CStr(), lParentText.CStr());
            }

            const auto lComponentsIt = lEntityJson.find(KEY_COMPONENTS);
            if (lComponentsIt != lEntityJson.end() && lComponentsIt->is_object())
            {
                for (const auto& [lTypeName, lPayload] : lComponentsIt->items())
                {
                    if (lTypeName.empty()) { continue; }   // no name to look up

                    lEntity.Components.emplace_back(OpaaxStringID(lTypeName), lPayload);
                }
            }

            OutEntities.emplace_back(Move(lEntity));
        }

        return lSkipped;
    }

    // =========================================================================
    // Placements — shared by map and prefab files.
    // =========================================================================
    nlohmann::json EntityJson::InstancesToJson(const TDynArray<PrefabInstanceRecord>& InInstances)
    {
        nlohmann::json lInstances = nlohmann::json::array();

        // Sorted by InstanceId, so the file is stable in git. A copy of the pointers is sorted.
        TDynArray<const PrefabInstanceRecord*> lOrdered;
        lOrdered.reserve(InInstances.size());
        for (const PrefabInstanceRecord& lRecord : InInstances)
        {
            lOrdered.emplace_back(&lRecord);
        }

        std::sort(lOrdered.begin(), lOrdered.end(),
            [](const PrefabInstanceRecord* InLeft, const PrefabInstanceRecord* InRight)
            {
                return InLeft->InstanceId.High != InRight->InstanceId.High
                    ? InLeft->InstanceId.High < InRight->InstanceId.High
                    : InLeft->InstanceId.Low  < InRight->InstanceId.Low;
            });

        for (const PrefabInstanceRecord* lPtr : lOrdered)
        {
            const PrefabInstanceRecord& lRecord = *lPtr;

            nlohmann::json lOverrides = nlohmann::json::object();
            for (const PrefabOverrideEntry& lEntry : lRecord.Overrides)
            {
                // Keyed by guid text; nlohmann objects are sorted, so the order is stable.
                lOverrides[lEntry.TemplateGuid.ToString().CStr()] = lEntry.Patch;
            }

            nlohmann::json lRecordJson = nlohmann::json::object();
            lRecordJson[KEY_PREFAB]      = lRecord.Prefab.CStr();
            lRecordJson[KEY_INSTANCE_ID] = lRecord.InstanceId.ToString().CStr();
            lRecordJson[KEY_OVERRIDES]   = Move(lOverrides);

            lInstances.emplace_back(Move(lRecordJson));
        }

        return lInstances;
    }

    Uint64 EntityJson::InstancesFromJson(const nlohmann::json& InRoot, TDynArray<PrefabInstanceRecord>& OutInstances)
    {
        const auto lIt = InRoot.find(KEY_INSTANCES);
        if (lIt == InRoot.end() || !lIt->is_array()) { return 0; }

        Uint64 lSkipped = 0;

        for (const nlohmann::json& lRecordJson : *lIt)
        {
            if (!lRecordJson.is_object()) { ++lSkipped; continue; }

            PrefabInstanceRecord lRecord;
            lRecord.Prefab = ReadString(lRecordJson, KEY_PREFAB);

            if (lRecord.Prefab.IsEmpty()
                || !Guid::FromString(ReadString(lRecordJson, KEY_INSTANCE_ID), lRecord.InstanceId))
            {
                ++lSkipped;
                continue;
            }

            const auto lOverridesIt = lRecordJson.find(KEY_OVERRIDES);
            if (lOverridesIt != lRecordJson.end() && lOverridesIt->is_object())
            {
                for (const auto& [lGuidText, lPatch] : lOverridesIt->items())
                {
                    PrefabOverrideEntry lEntry;
                    if (!Guid::FromString(OpaaxString(lGuidText.c_str(), static_cast<Uint32>(lGuidText.size())),
                                          lEntry.TemplateGuid))
                    {
                        // An override naming no entity of the prefab cannot be applied: dropped.
                        continue;
                    }

                    lEntry.Patch = lPatch;
                    lRecord.Overrides.emplace_back(Move(lEntry));
                }
            }

            OutInstances.emplace_back(Move(lRecord));
        }

        if (lSkipped > 0)
        {
            OPAAX_LOG(LogEntityJson, Warn, "Skipped {} prefab instance(s) with a missing path or malformed id", lSkipped);
        }

        return lSkipped;
    }
}
