#include "World/Serialization/MapFactory.h"

#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        // Write one entity's recorded components onto it. Shared by both entry points so a loaded
        // map and a restored one cannot diverge on what a payload means — including the two
        // tolerance policies below, which exist because this runs at BOOT.
        void LoadComponents(const EntityData& InData, EntityRegistry& InEntities, const EntityID InEntity,
                            const ComponentRegistry& InRegistry)
        {
            for (const ComponentData& lComponent : InData.Components)
            {
                const IComponentEntry* lEntry = InRegistry.FindByName(lComponent.TypeName);

                if (lEntry == nullptr)
                {
                    // Forward compatibility: a map written by a build that knew one more
                    // component type must still open here, minus that component. Warn — this
                    // IS data loss on the next save — but do not fail the load.
                    OPAAX_LOG(LogMapFactory, Warn,
                              "Unknown component '{}' on entity '{}' — skipped (not registered in this build).",
                              lComponent.TypeName, InData.Name.CStr());
                    continue;
                }

                try
                {
                    lEntry->Load(InEntities, InEntity, lComponent.Payload);
                }
                catch (const nlohmann::json::exception& lError)
                {
                    // The symmetric half of the unknown-type case above, and it reaches further than
                    // it looks: Instantiate runs at BOOT (Level::MountAll), so an escaping exception
                    // takes the whole app down before a window exists. A missing key is already
                    // handled — components use the _WITH_DEFAULT macro — so what lands here is a
                    // wrong-typed value or a payload that is not an object at all: a hand-edited or
                    // truncated file. BO4c's rule, one level down: warn, keep going.
                    //
                    // The component stays, at its defaults: Load emplaces before it reads, and the
                    // map did say this entity carried one.
                    OPAAX_LOG(LogMapFactory, Warn,
                              "Component '{}' on entity '{}' could not be read ({}) — left at its defaults.",
                              lComponent.TypeName, InData.Name.CStr(), lError.what());
                }
            }
        }

        // Take off every registered component the record does NOT name — the undo of an Add.
        // Remove refuses an essential type on its own, so this needs no exception of its own.
        void RemoveUnrecorded(const EntityData& InData, EntityRegistry& InEntities, const EntityID InEntity,
                              const ComponentRegistry& InRegistry)
        {
            InRegistry.ForEach([&](const IComponentEntry& InEntry)
            {
                if (!InEntry.Has(InEntities, InEntity))
                {
                    return;
                }

                const OpaaxStringID lName = InEntry.GetName();

                for (const ComponentData& lComponent : InData.Components)
                {
                    if (lComponent.TypeName == lName) { return; }
                }

                InEntry.Remove(InEntities, InEntity);
            });
        }
    }

    Uint64 MapFactory::Instantiate(const MapData& InData, World& InWorld, const ComponentRegistry& InRegistry)
    {
        Uint64 lCreated = 0;

        for (const EntityData& lEntityData : InData.Entities)
        {
            Entity lEntity = InWorld.CreateEntityWithGuid(lEntityData.Id, lEntityData.Name, lEntityData.OwnerMap);

            if (!lEntity.IsValid())
            {
                // World already logged the reason (invalid or duplicate Guid). Skipping one
                // entity beats abandoning the whole map.
                continue;
            }

            ++lCreated;

            lEntity.Get<EntityMeta>().Parent = lEntityData.Parent;

            LoadComponents(lEntityData, InWorld.GetRegistry(), lEntity.GetHandle(), InRegistry);
        }

        // Links are guids, so nothing above depended on order — but a link naming an entity that
        // never arrived (skipped, or not in this file) would be saved back forever. Cleared, and
        // said: the child now sits at its local pose as world, which the author will see.
        for (const EntityData& lEntityData : InData.Entities)
        {
            if (!lEntityData.Parent.IsValid() || InWorld.FindByGuid(lEntityData.Parent).IsValid()) { continue; }

            if (Entity lOrphan = InWorld.FindByGuid(lEntityData.Id); lOrphan.IsValid())
            {
                OPAAX_LOG(LogMapFactory, Warn, "Entity '{}' names a parent that is not in the world — now a root",
                          lEntityData.Name.CStr());
                lOrphan.Get<EntityMeta>().Parent = Guid{};
            }
        }

        OPAAX_LOG(LogMapFactory, Trace, "Instantiated {}/{} entity(ies) into world '{}'",
                  lCreated, InData.EntityCount(), InWorld.GetName().CStr());

        return lCreated;
    }

    Uint64 MapFactory::Restore(const MapData& InData, World& InWorld, const ComponentRegistry& InRegistry)
    {
        Uint64 lRestored = 0;

        for (const EntityData& lEntityData : InData.Entities)
        {
            Entity lEntity = InWorld.FindByGuid(lEntityData.Id);

            if (lEntity.IsValid())
            {
                EntityMeta& lMeta = lEntity.Get<EntityMeta>();

                lMeta.Name     = lEntityData.Name;
                lMeta.OwnerMap = lEntityData.OwnerMap;
                lMeta.Parent   = lEntityData.Parent;
            }
            else
            {
                lEntity = InWorld.CreateEntityWithGuid(lEntityData.Id, lEntityData.Name, lEntityData.OwnerMap);

                if (!lEntity.IsValid())
                {
                    continue;   // World logged the reason
                }

                lEntity.Get<EntityMeta>().Parent = lEntityData.Parent;
            }

            LoadComponents(lEntityData, InWorld.GetRegistry(), lEntity.GetHandle(), InRegistry);
            RemoveUnrecorded(lEntityData, InWorld.GetRegistry(), lEntity.GetHandle(), InRegistry);

            ++lRestored;
        }

        // See the header: an in-place component write is invisible to the world, and the dirty
        // check is gated on this number.
        if (lRestored > 0)
        {
            InWorld.MarkChanged();
        }

        OPAAX_LOG(LogMapFactory, Trace, "Restored {}/{} entity(ies) in world '{}'",
                  lRestored, InData.EntityCount(), InWorld.GetName().CStr());

        return lRestored;
    }
}
