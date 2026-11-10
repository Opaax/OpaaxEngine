#include "World/Serialization/EntityClone.h"

#include "World/Components/ComponentRegistry.h"
#include "World/Components/PrefabInstanceComponent.h"

namespace Opaax
{
    namespace
    {
        OpaaxStringID MarkerName(const ComponentRegistry& InRegistry)
        {
            const IComponentEntry* lEntry = InRegistry.Find<PrefabInstanceComponent>();
            return lEntry != nullptr ? lEntry->GetName() : OpaaxStringID();
        }

        /** InEntity's marker component, or null when it has none or the type is not registered. */
        ComponentData* FindMarker(EntityData& InEntity, const OpaaxStringID InMarkerName)
        {
            if (!InMarkerName.IsValid()) { return nullptr; }

            for (ComponentData& lComponent : InEntity.Components)
            {
                if (lComponent.TypeName == InMarkerName) { return &lComponent; }
            }
            return nullptr;
        }

        /** The linked marker of InEntity, read without throwing. False when there is none. */
        bool ReadMarker(const EntityData& InEntity, const OpaaxStringID InMarkerName, PrefabInstanceComponent& OutMarker)
        {
            if (!InMarkerName.IsValid()) { return false; }

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
    }

    MapData EntityClone::Clone(const MapData& InSource, const Options& InOptions, const ComponentRegistry& InRegistry)
    {
        const OpaaxStringID lMarkerName = MarkerName(InRegistry);

        MapData lCopy;
        lCopy.Id = InOptions.OwnerMap.IsValid() ? InOptions.OwnerMap : InSource.Id;
        lCopy.Entities = InSource.Entities;

        // Old identity -> new, first for every entity, so parents can be remapped in any order.
        TUnorderedMap<Guid, Guid> lNewIds;
        TUnorderedMap<Guid, Guid> lNewInstances;

        for (EntityData& lEntity : lCopy.Entities)
        {
            Guid lNew;

            PrefabInstanceComponent lMarker;
            if (ReadMarker(lEntity, lMarkerName, lMarker))
            {
                auto lFound = lNewInstances.find(lMarker.InstanceId);
                if (lFound == lNewInstances.end())
                {
                    lFound = lNewInstances.emplace(lMarker.InstanceId, Guid::New()).first;
                }

                lMarker.InstanceId = lFound->second;
                FindMarker(lEntity, lMarkerName)->Payload = lMarker;

                // As PrefabFactory::BuildInstance names them: what the fold looks for.
                lNew = Guid::Derive(lMarker.InstanceId, lMarker.TemplateGuid);
            }
            else
            {
                lNew = Guid::New();
            }

            lNewIds.emplace(lEntity.Id, lNew);
            lEntity.Id = lNew;

            if (InOptions.OwnerMap.IsValid()) { lEntity.OwnerMap = InOptions.OwnerMap; }
        }

        for (EntityData& lEntity : lCopy.Entities)
        {
            if (!lEntity.Parent.IsValid()) { continue; }

            if (const auto lInside = lNewIds.find(lEntity.Parent); lInside != lNewIds.end())
            {
                lEntity.Parent = lInside->second;
            }
            else if (!InOptions.bKeepOutsideParents)
            {
                lEntity.Parent = Guid{};
            }
        }

        return lCopy;
    }

    TDynArray<Guid> EntityClone::InstancesOf(const MapData& InData, const ComponentRegistry& InRegistry)
    {
        const OpaaxStringID lMarkerName = MarkerName(InRegistry);

        TDynArray<Guid> lInstances;
        for (const EntityData& lEntity : InData.Entities)
        {
            PrefabInstanceComponent lMarker;
            if (!ReadMarker(lEntity, lMarkerName, lMarker)) { continue; }

            bool bSeen = false;
            for (const Guid& lKnown : lInstances)
            {
                bSeen = bSeen || lKnown == lMarker.InstanceId;
            }
            if (!bSeen) { lInstances.emplace_back(lMarker.InstanceId); }
        }
        return lInstances;
    }

    Uint64 EntityClone::CountOfInstance(const MapData& InData, const Guid& InInstanceId,
                                        const ComponentRegistry& InRegistry)
    {
        const OpaaxStringID lMarkerName = MarkerName(InRegistry);

        Uint64 lCount = 0;
        for (const EntityData& lEntity : InData.Entities)
        {
            PrefabInstanceComponent lMarker;
            if (ReadMarker(lEntity, lMarkerName, lMarker) && lMarker.InstanceId == InInstanceId) { ++lCount; }
        }
        return lCount;
    }

    Uint64 EntityClone::Unlink(MapData& InOutData, const Guid& InInstanceId, const ComponentRegistry& InRegistry)
    {
        const OpaaxStringID lMarkerName = MarkerName(InRegistry);

        Uint64 lCount = 0;
        for (EntityData& lEntity : InOutData.Entities)
        {
            PrefabInstanceComponent lMarker;
            if (!ReadMarker(lEntity, lMarkerName, lMarker) || lMarker.InstanceId != InInstanceId) { continue; }

            for (Uint64 lIndex = 0; lIndex < lEntity.Components.size(); ++lIndex)
            {
                if (lEntity.Components[lIndex].TypeName != lMarkerName) { continue; }

                lEntity.Components.erase(lEntity.Components.begin() + static_cast<PtrDiff>(lIndex));
                break;
            }
            ++lCount;
        }
        return lCount;
    }
}
