#include "World/Prefab/PrefabFactory.h"

#include <entt/entt.hpp>

#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Prefab/PrefabFold.h"

namespace Opaax
{
    namespace
    {
        OpaaxStringID MarkerName(const ComponentRegistry& InRegistry)
        {
            const IComponentEntry* const lEntry =
                InRegistry.FindByTypeId(entt::type_hash<PrefabInstanceComponent>::value());
            return lEntry != nullptr ? lEntry->GetName() : OpaaxStringID();
        }

        void StripMarker(EntityData& InOutEntity, const OpaaxStringID InMarkerName)
        {
            if (!InMarkerName.IsValid()) { return; }

            std::erase_if(InOutEntity.Components,
                          [InMarkerName](const ComponentData& InComponent)
                          {
                              return InComponent.TypeName == InMarkerName;
                          });
        }
    }

    MapData PrefabFactory::BuildInstance(const PrefabData& InPrefab, const OpaaxString& InPrefabAssetPath,
                                         const Guid& InInstanceId, MapId InOwnerMap,
                                         const ComponentRegistry& InRegistry)
    {
        MapData lInstance;

        if (!InInstanceId.IsValid())
        {
            OPAAX_LOG(LogPrefabFactory, Error,
                      "Cannot build an instance of '{}' without a valid instance id",
                      InPrefabAssetPath.CStr());
            return lInstance;
        }

        if (!InOwnerMap.IsValid())
        {
            // An invalid OwnerMap means "runtime-spawned" (**WM2**), so these entities would exist
            // and no Save could ever write them — a placement that silently does not persist.
            OPAAX_LOG(LogPrefabFactory, Error,
                      "Cannot build an instance of '{}' without a target map", InPrefabAssetPath.CStr());
            return lInstance;
        }

        // The authoring name comes from the REGISTRY, never a literal here: it is the key written
        // into the map file, and a second copy of it is how a writer and a reader drift.
        const IComponentEntry* lMarkerEntry =
            InRegistry.FindByTypeId(entt::type_hash<PrefabInstanceComponent>::value());

        if (lMarkerEntry == nullptr)
        {
            OPAAX_LOG(LogPrefabFactory, Error,
                      "PrefabInstanceComponent is not registered — refusing to build an untraceable "
                      "instance of '{}'", InPrefabAssetPath.CStr());
            return lInstance;
        }

        const OpaaxStringID lMarkerName = lMarkerEntry->GetName();

        lInstance.Id = InOwnerMap;
        lInstance.Entities.reserve(InPrefab.Entities.size());

        for (const EntityData& lTemplate : InPrefab.Entities)
        {
            EntityData lEntity = lTemplate;

            lEntity.Id       = Guid::Derive(InInstanceId, lTemplate.Id);
            lEntity.OwnerMap = InOwnerMap;

            // The outer instance owns the entity: its marker replaces rather than stacks, erased
            // first so the payload cannot appear twice. A FLATTENED prefab (P7) carries none, but
            // a caller handing raw entities that were an instance might.
            StripMarker(lEntity, lMarkerName);

            PrefabInstanceComponent lMarker;
            lMarker.Prefab.Path  = InPrefabAssetPath;
            lMarker.InstanceId   = InInstanceId;
            lMarker.TemplateGuid = lTemplate.Id;   // the PREFAB's guid, not the derived one

            lEntity.Components.emplace_back(lMarkerName, nlohmann::json(lMarker));

            lInstance.Entities.emplace_back(Move(lEntity));
        }

        OPAAX_LOG(LogPrefabFactory, Trace, "Built {} entity(ies) for one instance of '{}'",
                  lInstance.EntityCount(), InPrefabAssetPath.CStr());

        return lInstance;
    }

    PrefabData PrefabFactory::BuildPrefab(const MapData& InCaptured, const ComponentRegistry& InRegistry)
    {
        PrefabData lPrefab;

        // The marker's authoring name comes from the REGISTRY for BuildInstance's reason. Absent, no
        // entity can be carrying one, so there is nothing to strip and this is not a refusal.
        const IComponentEntry* lMarkerEntry =
            InRegistry.FindByTypeId(entt::type_hash<PrefabInstanceComponent>::value());
        const OpaaxStringID    lMarkerName = (lMarkerEntry != nullptr) ? lMarkerEntry->GetName()
                                                                      : OpaaxStringID();

        lPrefab.Entities.reserve(InCaptured.Entities.size());

        for (const EntityData& lCaptured : InCaptured.Entities)
        {
            EntityData lEntity = lCaptured;

            // Guids are KEPT — they become the file's template ids. See the header.
            lEntity.OwnerMap = MapId();
            StripMarker(lEntity, lMarkerName);

            lPrefab.Entities.emplace_back(Move(lEntity));
        }

        // The RECORDS come along (P7): a capture that was folded first holds its nested placements
        // here, and they are what the file stores — expanding them is the reader's business.
        lPrefab.Instances = InCaptured.Instances;

        OPAAX_LOG(LogPrefabFactory, Trace, "Built a prefab of {} entity(ies) and {} placement(s)",
                  lPrefab.EntityCount(), lPrefab.InstanceCount());

        return lPrefab;
    }

    PrefabData PrefabFactory::Flatten(const PrefabData& InRaw, const OpaaxString& InPrefabAssetPath,
                                      const IPrefabResolver& InResolver, const ComponentRegistry& InRegistry)
    {
        PrefabData lFlat;
        lFlat.Entities = InRaw.Entities;   // its own, as authored

        if (InRaw.Instances.empty()) { return lFlat; }

        // The same expansion a map's placements get, on a placeholder map: BuildInstance refuses
        // an invalid one, and the id is cleared off every entity below anyway (WM2 — a prefab's
        // entities belong to no map).
        MapData lPlacements;
        lPlacements.Id        = MapId("Prefab");
        lPlacements.Instances = InRaw.Instances;

        const Uint64 lExpanded = PrefabFold::Expand(lPlacements, InResolver, InRegistry);

        // "As if these were the prefab's own": no map, no marker. The guid each carries —
        // Derive(record.InstanceId, template) — IS the in-prefab identity a level's BuildInstance
        // derives from again and an override record keys by; nothing else about the nesting has to
        // survive into the flat view.
        const OpaaxStringID lMarkerName = MarkerName(InRegistry);

        for (EntityData& lEntity : lPlacements.Entities)
        {
            lEntity.OwnerMap = MapId();
            StripMarker(lEntity, lMarkerName);
            lFlat.Entities.emplace_back(Move(lEntity));
        }

        OPAAX_LOG(LogPrefabFactory, Trace, "Flattened '{}': {} own entity(ies) + {} of {} placement(s) -> {}",
                  InPrefabAssetPath.CStr(), InRaw.EntityCount(), lExpanded, InRaw.InstanceCount(),
                  lFlat.EntityCount());

        return lFlat;
    }

    PrefabData PrefabFactory::BuildVariant(const MapData& InState, const OpaaxString& InBaseAssetPath,
                                           const IPrefabResolver& InResolver, const ComponentRegistry& InRegistry)
    {
        const PrefabData* const lBase = InResolver.Resolve(InBaseAssetPath);
        if (lBase == nullptr)
        {
            OPAAX_LOG(LogPrefabFactory, Error, "Cannot build a variant of '{}' — it did not resolve",
                      InBaseAssetPath.CStr());
            return PrefabData{};
        }

        const OpaaxStringID lMarkerName = MarkerName(InRegistry);
        if (!lMarkerName.IsValid())
        {
            OPAAX_LOG(LogPrefabFactory, Error,
                      "PrefabInstanceComponent is not registered — a variant of '{}' could not name its base",
                      InBaseAssetPath.CStr());
            return PrefabData{};
        }

        const Guid lInstanceId = Guid::New();
        MapData    lAsInstance = InState;

        // Every entity the base has is re-marked as THIS instance of it — its own marker replaced,
        // since a nested entity of the base names the nested prefab, and the base's flatten already
        // carries that nesting. Fold then diffs each against its template and records what is gone.
        for (EntityData& lEntity : lAsInstance.Entities)
        {
            bool lOfBase = false;
            for (const EntityData& lTemplate : lBase->Entities)
            {
                if (lTemplate.Id == lEntity.Id) { lOfBase = true; break; }
            }
            if (!lOfBase) { continue; }

            StripMarker(lEntity, lMarkerName);

            PrefabInstanceComponent lMarker;
            lMarker.Prefab.Path  = InBaseAssetPath;
            lMarker.InstanceId   = lInstanceId;
            lMarker.TemplateGuid = lEntity.Id;

            lEntity.Components.emplace_back(lMarkerName, nlohmann::json(lMarker));
        }

        PrefabFold::Fold(lAsInstance, InResolver, InRegistry);

        // An empty base marks nothing, so Fold produced no record for it — added by hand: a variant
        // that names no base is not a variant.
        bool lNamed = false;
        for (const PrefabInstanceRecord& lRecord : lAsInstance.Instances)
        {
            if (lRecord.InstanceId == lInstanceId) { lNamed = true; break; }
        }
        if (!lNamed)
        {
            lAsInstance.Instances.emplace_back(PrefabInstanceRecord{ InBaseAssetPath, lInstanceId, {} });
        }

        PrefabData lVariant = BuildPrefab(lAsInstance, InRegistry);

        OPAAX_LOG(LogPrefabFactory, Trace, "Built a variant of '{}': {} own entity(ies), {} placement(s)",
                  InBaseAssetPath.CStr(), lVariant.EntityCount(), lVariant.InstanceCount());

        return lVariant;
    }
}
