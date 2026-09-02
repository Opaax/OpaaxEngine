#include "World/Serialization/MapSerializer.h"

#include "World/Components/ComponentRegistry.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        // One entity as data — the unit every capture below shares, so a subset capture and a
        // whole-world one cannot disagree about what an entity IS.
        EntityData CaptureOne(const EntityRegistry& InEntities, const ComponentRegistry& InRegistry,
                              const EntityID InEntity, const EntityMeta& InMeta)
        {
            EntityData lData;
            lData.Id       = InMeta.Id;
            lData.Name     = InMeta.Name;
            lData.OwnerMap = InMeta.OwnerMap;

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

        // The shared walk. An invalid InMapId takes EVERYTHING — which is why this is private:
        // the two public entry points are what decide that, so the ambiguity that made MP10
        // possible is not reachable from outside this file.
        MapData CaptureFiltered(const World& InWorld, const ComponentRegistry& InRegistry, MapId InMapId)
        {
            MapData lData;

            const EntityRegistry& lRegistry = InWorld.GetRegistry();

            const auto lView = lRegistry.view<EntityMeta>();

            // ONE allocation for the whole walk. This is the WORLD's entity count, so a filtered
            // capture over-reserves — deliberately: an EntityData owns a string and a vector, and
            // regrowing this move-constructs every one of them log2(n) times.
            lData.Entities.reserve(lView.size());

            // EntityMeta is emplaced by World::CreateEntityWithGuid, through which EVERY entity is
            // created — so this view is the complete all-entities view, not a subset.
            for (const auto [lEntity, lMeta] : lView.each())
            {
                if (InMapId.IsValid() && lMeta.OwnerMap != InMapId)
                {
                    continue;
                }

                lData.Entities.emplace_back(CaptureOne(lRegistry, InRegistry, lEntity, lMeta));
            }

            // TRACE, not Info. Capture is a pure transformation with several callers, and one of
            // them — the editor's derived dirty check — runs on a timer while the editor is simply
            // open. An Info line here put ~1700 identical entries in a 10-second run. The events
            // worth an Info are the ones that actually happen TO something: MapFile's Save/Load and
            // MapFactory's Instantiate, which all still log at Info.
            OPAAX_LOG(LogMapSerializer, Trace, "Captured {} entity(ies) from world '{}'{}",
                      lData.EntityCount(), InWorld.GetName().CStr(),
                      InMapId.IsValid() ? " (filtered)" : "");

            return lData;
        }
    }

    MapData MapSerializer::CaptureWorld(const World& InWorld, const ComponentRegistry& InRegistry)
    {
        // Id deliberately left invalid — a whole-world snapshot is not a map and never reaches a
        // file. MapJson writes that as "" (MP1), so nothing can mistake it for one either.
        return CaptureFiltered(InWorld, InRegistry, MapId());
    }

    MapData MapSerializer::CaptureMap(const World& InWorld, const ComponentRegistry& InRegistry, MapId InMapId)
    {
        if (!InMapId.IsValid())
        {
            // NOTHING, and loudly. An invalid id names no map; the one thing it must never mean
            // here is "everything", which is what the old defaulted-filter signature made it mean
            // for any map whose entities had not claimed it yet (**MP10**).
            OPAAX_LOG(LogMapSerializer, Warn,
                      "CaptureMap on world '{}' with an invalid map id — captured nothing",
                      InWorld.GetName().CStr());
            return MapData();
        }

        MapData lData = CaptureFiltered(InWorld, InRegistry, InMapId);

        // The map carries its OWN name to the file, so an empty one is still identifiable.
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
                continue;   // destroyed under the caller, or never ours — see the header
            }

            lData.Entities.emplace_back(CaptureOne(lRegistry, InRegistry, lEntity, *lMeta));
        }

        // NO LOG LINE, unlike its two neighbours. This one answers a question about a HANDFUL of
        // entities on an interactive path — the editor calls it per recorded edit — so a line here
        // says nothing the verb's own Info line does not, and one caller flooding the log is what
        // it cost last time.
        return lData;
    }
}
