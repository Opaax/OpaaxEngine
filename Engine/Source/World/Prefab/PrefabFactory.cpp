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

        bool Names(const TDynArray<EntityData>& InEntities, const Guid& InId)
        {
            for (const EntityData& lEntity : InEntities)
            {
                if (lEntity.Id == InId) { return true; }
            }
            return false;
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
            // Without an owner map the entities would be runtime-spawned and never saved.
            OPAAX_LOG(LogPrefabFactory, Error,
                      "Cannot build an instance of '{}' without a target map", InPrefabAssetPath.CStr());
            return lInstance;
        }

        // Name from the registry (it is the key saved in map files).
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

            // Parent links are derived with the guids; links to outside the prefab are dropped.
            lEntity.Parent   = Names(InPrefab.Entities, lTemplate.Parent) ? Guid::Derive(InInstanceId, lTemplate.Parent)
                                                                           : Guid{};

            // The outer instance owns the entity: replace any existing marker.
            StripMarker(lEntity, lMarkerName);

            PrefabInstanceComponent lMarker;
            lMarker.Prefab.Path  = InPrefabAssetPath;
            lMarker.InstanceId   = InInstanceId;
            lMarker.TemplateGuid = lTemplate.Id;   // the prefab's guid, not the derived one

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

        // Not registered: no entity can carry one, nothing to strip.
        const IComponentEntry* lMarkerEntry =
            InRegistry.FindByTypeId(entt::type_hash<PrefabInstanceComponent>::value());
        const OpaaxStringID    lMarkerName = (lMarkerEntry != nullptr) ? lMarkerEntry->GetName()
                                                                      : OpaaxStringID();

        lPrefab.Entities.reserve(InCaptured.Entities.size());

        for (const EntityData& lCaptured : InCaptured.Entities)
        {
            EntityData lEntity = lCaptured;

            // Guids are kept: they become the file's template ids.
            lEntity.OwnerMap = MapId();
            StripMarker(lEntity, lMarkerName);

            // Roots have no parent. Only checked for an unfolded capture (with placements, an entity may
            // hang under a placement's entity).
            if (lEntity.Parent.IsValid() && InCaptured.Instances.empty() && !Names(InCaptured.Entities, lEntity.Parent))
            {
                OPAAX_LOG(LogPrefabFactory, Warn, "Entity '{}' was parented outside the prefab — it becomes a root",
                          lEntity.Name.CStr());
                lEntity.Parent = Guid{};
            }

            lPrefab.Entities.emplace_back(Move(lEntity));
        }

        // Placements are stored as records; the reader expands them.
        lPrefab.Instances = InCaptured.Instances;

        OPAAX_LOG(LogPrefabFactory, Trace, "Built a prefab of {} entity(ies) and {} placement(s)",
                  lPrefab.EntityCount(), lPrefab.InstanceCount());

        return lPrefab;
    }

    PrefabData PrefabFactory::Flatten(const PrefabData& InRaw, const OpaaxString& InPrefabAssetPath,
                                      const IPrefabResolver& InResolver, const ComponentRegistry& InRegistry)
    {
        PrefabData lFlat;
        lFlat.Entities = InRaw.Entities;   // its own entities

        if (InRaw.Instances.empty()) { return lFlat; }

        // Expanded like a map's placements, on a placeholder map (the map id is cleared afterwards).
        MapData lPlacements;
        lPlacements.Id        = MapId("Prefab");
        lPlacements.Instances = InRaw.Instances;

        const Uint64 lExpanded = PrefabFold::Expand(lPlacements, InResolver, InRegistry);

        // Flattened: no map, no marker. Each keeps its derived guid (its identity inside the prefab).
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

        // Every entity of the base is marked as this instance of it. Fold then diffs each against its
        // template. Guids and parent links are derived first, like a level placement's.
        for (EntityData& lEntity : lAsInstance.Entities)
        {
            if (Names(lBase->Entities, lEntity.Parent)) { lEntity.Parent = Guid::Derive(lInstanceId, lEntity.Parent); }

            if (!Names(lBase->Entities, lEntity.Id)) { continue; }

            StripMarker(lEntity, lMarkerName);

            PrefabInstanceComponent lMarker;
            lMarker.Prefab.Path  = InBaseAssetPath;
            lMarker.InstanceId   = lInstanceId;
            lMarker.TemplateGuid = lEntity.Id;

            lEntity.Components.emplace_back(lMarkerName, nlohmann::json(lMarker));
            lEntity.Id = Guid::Derive(lInstanceId, lEntity.Id);
        }

        PrefabFold::Fold(lAsInstance, InResolver, InRegistry);

        // An empty base gives no record: add one (a variant must name its base).
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
