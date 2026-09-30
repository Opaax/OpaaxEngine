#include "World/Serialization/MapSerializer.h"

#include "World/Components/ComponentRegistry.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        // One entity as data, shared by every capture.
        EntityData CaptureOne(const EntityRegistry& InEntities, const ComponentRegistry& InRegistry,
                              const EntityID InEntity, const EntityMeta& InMeta)
        {
            EntityData lData;
            lData.Id       = InMeta.Id;
            lData.Name     = InMeta.Name;
            lData.OwnerMap = InMeta.OwnerMap;
            lData.Parent   = InMeta.Parent;

            InRegistry.ForEach([&](const IComponentEntry& InEntry)
            {
                if (!InEntry.Has(InEntities, InEntity))
                {
                    return;
                }

                lData.Components.emplace_back(InEntry.GetName(), InEntry.Save(InEntities, InEntity));
            });

            return lData;
        }

        // Shared walk. An invalid InMapId takes everything, so it stays private.
        MapData CaptureFiltered(const World& InWorld, const ComponentRegistry& InRegistry, MapId InMapId)
        {
            MapData lData;

            const EntityRegistry& lRegistry = InWorld.GetRegistry();

            const auto lView = lRegistry.view<EntityMeta>();

            // Reserve the world's entity count once (over-reserves for a filtered capture).
            lData.Entities.reserve(lView.size());

            // Every entity has an EntityMeta.
            for (const auto [lEntity, lMeta] : lView.each())
            {
                if (InMapId.IsValid() && lMeta.OwnerMap != InMapId)
                {
                    continue;
                }

                lData.Entities.emplace_back(CaptureOne(lRegistry, InRegistry, lEntity, lMeta));
            }

            // No log: the editor's dirty check captures on a timer.
            return lData;
        }
    }

    MapData MapSerializer::CaptureWorld(const World& InWorld, const ComponentRegistry& InRegistry)
    {
        // Id left invalid: a world snapshot is not a map (written as "").
        return CaptureFiltered(InWorld, InRegistry, MapId());
    }

    MapData MapSerializer::CaptureMap(const World& InWorld, const ComponentRegistry& InRegistry, MapId InMapId)
    {
        if (!InMapId.IsValid())
        {
            // An invalid id captures nothing (with a warning), never everything.
            OPAAX_LOG(LogMapSerializer, Warn,
                      "CaptureMap on world '{}' with an invalid map id — captured nothing",
                      InWorld.GetName().CStr());
            return MapData();
        }

        MapData lData = CaptureFiltered(InWorld, InRegistry, InMapId);

        // The map carries its own name, so an empty map is still identified.
        lData.Id = InMapId;

        return lData;
    }

    MapData MapSerializer::CaptureEntities(const World& InWorld, const ComponentRegistry& InRegistry,
                                           const TDynArray<EntityID>& InEntities)
    {
        MapData lData;
        lData.Entities.reserve(InEntities.size());

        const EntityRegistry& lRegistry = InWorld.GetRegistry();

        for (const EntityID lEntity : InEntities)
        {
            const EntityMeta* lMeta = InWorld.IsValid(lEntity) ? lRegistry.try_get<EntityMeta>(lEntity)
                                                               : nullptr;

            if (lMeta == nullptr)
            {
                continue;   // destroyed meanwhile, or not ours
            }

            lData.Entities.emplace_back(CaptureOne(lRegistry, InRegistry, lEntity, *lMeta));
        }

        // No log: called for every recorded edit.
        return lData;
    }
}
