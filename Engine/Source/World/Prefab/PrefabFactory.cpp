#include "World/Prefab/PrefabFactory.h"

#include <entt/entt.hpp>

#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"

namespace Opaax
{
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

            // A prefab file may already carry a marker — that is a NESTED instance, and composing
            // the two is ⑦-C P7. Until then the outer instance owns the entity, so its marker
            // replaces rather than stacks; erased first so the payload cannot appear twice.
            std::erase_if(lEntity.Components,
                          [lMarkerName](const ComponentData& InComponent)
                          {
                              return InComponent.TypeName == lMarkerName;
                          });

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

            if (lMarkerName.IsValid())
            {
                std::erase_if(lEntity.Components,
                              [lMarkerName](const ComponentData& InComponent)
                              {
                                  return InComponent.TypeName == lMarkerName;
                              });
            }

            lPrefab.Entities.emplace_back(Move(lEntity));
        }

        OPAAX_LOG(LogPrefabFactory, Trace, "Built a prefab of {} entity(ies)", lPrefab.EntityCount());

        return lPrefab;
    }
}
