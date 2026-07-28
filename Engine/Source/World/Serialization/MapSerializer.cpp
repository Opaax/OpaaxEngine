#include "World/Serialization/MapSerializer.h"

#include "World/ComponentRegistry.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

namespace Opaax
{
    MapData MapSerializer::Capture(const World& InWorld, const ComponentRegistry& InRegistry, MapId InFilter)
    {
        MapData lData;

        const EntityRegistry& lRegistry = InWorld.GetRegistry();

        // EntityMeta is emplaced by World::CreateEntityWithGuid, through which EVERY entity is
        // created — so this view is the complete all-entities view, not a subset.
        for (const auto [lEntity, lMeta] : lRegistry.view<EntityMeta>().each())
        {
            // No filter => the whole world (the PIE clone case). A valid filter keeps only that
            // map's entities, and runtime-spawned ones (invalid OwnerMap) can never match it.
            if (InFilter.IsValid() && lMeta.OwnerMap != InFilter)
            {
                continue;
            }

            EntityData lEntityData;
            lEntityData.Id       = lMeta.Id;
            lEntityData.Name     = lMeta.Name;
            lEntityData.OwnerMap = lMeta.OwnerMap;

            InRegistry.ForEach([&](const IComponentEntry& InEntry)
            {
                if (!InEntry.Has(lRegistry, lEntity))
                {
                    return;
                }

                lEntityData.Components.push_back(
                    ComponentData{ InEntry.GetName(), InEntry.Save(lRegistry, lEntity) });
            });

            lData.Entities.push_back(Move(lEntityData));
        }

        OPAAX_LOG(LogMapSerializer, Info, "Captured {} entity(ies) from world '{}'{}",
                  lData.EntityCount(), InWorld.GetName().CStr(),
                  InFilter.IsValid() ? " (filtered)" : "")

        return lData;
    }
}
