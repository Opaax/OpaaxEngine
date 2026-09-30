#include "World/Prefab/PrefabFold.h"

#include <entt/entt.hpp>

#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Prefab/PrefabFactory.h"
#include "World/Prefab/PrefabOverrides.h"

namespace Opaax
{
    namespace
    {
        OpaaxStringID MarkerName(const ComponentRegistry& InRegistry)
        {
            const IComponentEntry* lEntry =
                InRegistry.FindByTypeId(entt::type_hash<PrefabInstanceComponent>::value());

            return (lEntry != nullptr) ? lEntry->GetName() : OpaaxStringID();
        }

        // The marker as a typed value, or nothing. Never throws.
        bool ReadMarker(const EntityData& InEntity, OpaaxStringID InMarkerName,
                        PrefabInstanceComponent& OutMarker)
        {
            for (const ComponentData& lComponent : InEntity.Components)
            {
                if (lComponent.TypeName != InMarkerName) { continue; }

                try
                {
                    OutMarker = lComponent.Payload.get<PrefabInstanceComponent>();
                }
                catch (const nlohmann::json::exception&)
                {
                    return false;
                }

                return OutMarker.IsLinked();
            }

            return false;
        }

        const EntityData* FindTemplate(const PrefabData& InPrefab, const Guid& InTemplateGuid)
        {
            for (const EntityData& lEntity : InPrefab.Entities)
            {
                if (lEntity.Id == InTemplateGuid) { return &lEntity; }
            }

            return nullptr;
        }
    }

    Uint64 PrefabFold::Fold(MapData& InOutData, const IPrefabResolver& InResolver,
                            const ComponentRegistry& InRegistry)
    {
        const OpaaxStringID lMarkerName = MarkerName(InRegistry);
        if (!lMarkerName.IsValid())
        {
            // Not registered: nothing to fold.
            return 0;
        }

        // Grouped by placement, in first-seen order (MapJson sorts records on write).
        TDynArray<PrefabInstanceRecord> lRecords;
        TDynArray<TDynArray<Guid>>      lPresent;    // per record: templates still in the world
        TDynArray<MapData>              lPristine;   // per record: the instance as built (diff baseline)
        TDynArray<EntityData>           lLoose;
        lLoose.reserve(InOutData.Entities.size());

        // BuildInstance needs a map id: use a placeholder (OwnerMap is not compared).
        const MapId lBuildMap = InOutData.Id.IsValid() ? InOutData.Id : MapId("Pending");

        for (EntityData& lEntity : InOutData.Entities)
        {
            PrefabInstanceComponent lMarker;
            if (!ReadMarker(lEntity, lMarkerName, lMarker))
            {
                lLoose.emplace_back(Move(lEntity));
                continue;
            }

            const PrefabData* lPrefab = InResolver.Resolve(lMarker.Prefab.Path);
            if (lPrefab == nullptr)
            {
                // Kept expanded: a renamed prefab loses its link, never the entities.
                OPAAX_LOG(LogPrefabFold, Warn,
                          "Prefab '{}' could not be resolved — its entities are saved expanded, and "
                          "the link is lost", lMarker.Prefab.Path.CStr());
                lLoose.emplace_back(Move(lEntity));
                continue;
            }

            const EntityData* lTemplate = FindTemplate(*lPrefab, lMarker.TemplateGuid);
            if (lTemplate == nullptr)
            {
                OPAAX_LOG(LogPrefabFold, Warn,
                          "Entity '{}' claims a template that is no longer in '{}' — saved expanded",
                          lEntity.Name.CStr(), lMarker.Prefab.Path.CStr());
                lLoose.emplace_back(Move(lEntity));
                continue;
            }

            Uint64 lIndex = 0;
            for (; lIndex < lRecords.size(); ++lIndex)
            {
                if (lRecords[lIndex].InstanceId == lMarker.InstanceId) { break; }
            }

            if (lIndex == lRecords.size())
            {
                lRecords.emplace_back(PrefabInstanceRecord{ lMarker.Prefab.Path, lMarker.InstanceId, {} });
                lPresent.emplace_back();

                // Diff against the instance as built (derived guids and parents), not the raw template,
                // so Fold is the exact inverse of Expand.
                lPristine.emplace_back(PrefabFactory::BuildInstance(*lPrefab, lMarker.Prefab.Path,
                                                                    lMarker.InstanceId, lBuildMap, InRegistry));
            }

            // The templates this placement still has; the others were deleted.
            lPresent[lIndex].emplace_back(lMarker.TemplateGuid);

            const EntityData* lBaseline = lTemplate;
            for (const EntityData& lBuilt : lPristine[lIndex].Entities)
            {
                if (lBuilt.Id == lEntity.Id) { lBaseline = &lBuilt; break; }
            }

            nlohmann::json lPatch = PrefabOverrides::Diff(*lBaseline, lEntity, lMarkerName);
            if (!PrefabOverrides::IsEmpty(lPatch))
            {
                lRecords[lIndex].Overrides.emplace_back(
                    PrefabOverrideEntry{ lMarker.TemplateGuid, Move(lPatch) });
            }
        }

        // Removed entities are recorded as a null patch.
        for (Uint64 lIndex = 0; lIndex < lRecords.size(); ++lIndex)
        {
            const PrefabData* lPrefab = InResolver.Resolve(lRecords[lIndex].Prefab);
            if (lPrefab == nullptr) { continue; }   // already warned

            for (const EntityData& lTemplate : lPrefab->Entities)
            {
                bool lStillHere = false;
                for (const Guid& lSeen : lPresent[lIndex])
                {
                    if (lSeen == lTemplate.Id) { lStillHere = true; break; }
                }

                if (lStillHere) { continue; }

                lRecords[lIndex].Overrides.emplace_back(
                    PrefabOverrideEntry{ lTemplate.Id, nlohmann::json() });   // null = removed
            }
        }

        InOutData.Entities = Move(lLoose);

        // Appended: existing records are kept.
        for (PrefabInstanceRecord& lRecord : lRecords)
        {
            InOutData.Instances.emplace_back(Move(lRecord));
        }

        return static_cast<Uint64>(lRecords.size());
    }

    Uint64 PrefabFold::Expand(MapData& InOutData, const IPrefabResolver& InResolver,
                              const ComponentRegistry& InRegistry)
    {
        if (InOutData.Instances.empty()) { return 0; }

        Uint64 lExpanded = 0;

        for (const PrefabInstanceRecord& lRecord : InOutData.Instances)
        {
            const PrefabData* lPrefab = InResolver.Resolve(lRecord.Prefab);
            if (lPrefab == nullptr)
            {
                // The entities only exist in the prefab file: this placement is lost (error).
                OPAAX_LOG(LogPrefabFold, Error,
                          "Prefab '{}' could not be resolved — that placement is missing from the map",
                          lRecord.Prefab.CStr());
                continue;
            }

            // Same derivation as the first placement, so every guid comes back identical.
            MapData lInstance = PrefabFactory::BuildInstance(*lPrefab, lRecord.Prefab,
                                                             lRecord.InstanceId, InOutData.Id,
                                                             InRegistry);

            for (EntityData& lEntity : lInstance.Entities)
            {
                bool lRemoved = false;

                // The patch is keyed by template guid.
                for (const PrefabOverrideEntry& lEntry : lRecord.Overrides)
                {
                    const Guid lDerived = Guid::Derive(lRecord.InstanceId, lEntry.TemplateGuid);
                    if (lDerived != lEntity.Id) { continue; }

                    // A null patch means this entity was deleted from the placement.
                    lRemoved = lEntry.Patch.is_null();
                    if (!lRemoved) { PrefabOverrides::Apply(lEntry.Patch, lEntity); }
                    break;
                }

                if (lRemoved) { continue; }

                InOutData.Entities.emplace_back(Move(lEntity));
            }

            ++lExpanded;
        }

        InOutData.Instances.clear();

        return lExpanded;
    }
}
