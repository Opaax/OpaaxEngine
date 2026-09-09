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

        // The marker as a typed value, or nothing. Never throws — a hand-edited payload is an
        // ordinary input at this layer (**MP3**).
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
            // Nothing can be carrying a marker, so there is nothing to fold. Not a refusal.
            return 0;
        }

        // Grouped by placement, in FIRST-SEEN order; MapJson sorts the records on write (**MP2**),
        // so nothing here has to care about ordering.
        TDynArray<PrefabInstanceRecord> lRecords;
        TDynArray<TDynArray<Guid>>      lPresent;   // per record: the templates still in the world
        TDynArray<EntityData>           lLoose;
        lLoose.reserve(InOutData.Entities.size());

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
                // EXPANDED, not dropped. A renamed prefab file must cost the author a link, never
                // their level — see the header.
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
            }

            // WHICH TEMPLATES THIS PLACEMENT STILL HAS. The complement is what was DELETED, and
            // without it a removal is unrepresentable — the record would be identical to one where
            // the entity is simply unmodified, and Expand would bring it back.
            lPresent[lIndex].emplace_back(lMarker.TemplateGuid);

            nlohmann::json lPatch = PrefabOverrides::Diff(*lTemplate, lEntity, lMarkerName);
            if (!PrefabOverrides::IsEmpty(lPatch))
            {
                lRecords[lIndex].Overrides.emplace_back(
                    PrefabOverrideEntry{ lMarker.TemplateGuid, Move(lPatch) });
            }
        }

        // THE REMOVALS, recorded as a NULL patch — merge-patch's own convention, and the same one
        // PrefabOverrides already uses a level down for a deleted component. An entity of the
        // prefab that this placement no longer has gets `"<templateGuid>": null`.
        for (Uint64 lIndex = 0; lIndex < lRecords.size(); ++lIndex)
        {
            const PrefabData* lPrefab = InResolver.Resolve(lRecords[lIndex].Prefab);
            if (lPrefab == nullptr) { continue; }   // already warned above

            for (const EntityData& lTemplate : lPrefab->Entities)
            {
                bool lStillHere = false;
                for (const Guid& lSeen : lPresent[lIndex])
                {
                    if (lSeen == lTemplate.Id) { lStillHere = true; break; }
                }

                if (lStillHere) { continue; }

                lRecords[lIndex].Overrides.emplace_back(
                    PrefabOverrideEntry{ lTemplate.Id, nlohmann::json() });   // null == removed
            }
        }

        InOutData.Entities = Move(lLoose);

        // APPENDED, not assigned: a MapData that already carried records (folded twice, or read
        // from a file and folded again) must not lose them.
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
                // The entities live only in the prefab file, so this placement is genuinely gone.
                // Error rather than Warn: it is data the author will notice missing.
                OPAAX_LOG(LogPrefabFold, Error,
                          "Prefab '{}' could not be resolved — that placement is missing from the map",
                          lRecord.Prefab.CStr());
                continue;
            }

            // Rebuilt through the SAME derivation the first placement used (**K2**), so every guid
            // comes back identical and nothing that referenced one had its target moved.
            MapData lInstance = PrefabFactory::BuildInstance(*lPrefab, lRecord.Prefab,
                                                             lRecord.InstanceId, InOutData.Id,
                                                             InRegistry);

            for (EntityData& lEntity : lInstance.Entities)
            {
                bool lRemoved = false;

                // The patch is keyed by TEMPLATE guid, which BuildInstance derived from — so it is
                // recovered the same way rather than stored a second time on the entity.
                for (const PrefabOverrideEntry& lEntry : lRecord.Overrides)
                {
                    const Guid lDerived = Guid::Derive(lRecord.InstanceId, lEntry.TemplateGuid);
                    if (lDerived != lEntity.Id) { continue; }

                    // A NULL patch means this placement DELETED that piece of the prefab. Checked
                    // before Apply, which reads null as "nothing to do" — the reading that made a
                    // deletion silently come back.
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
