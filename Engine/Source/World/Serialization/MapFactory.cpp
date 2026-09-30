#include "World/Serialization/MapFactory.h"

#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        // Writes one entity's components onto it. Shared by Instantiate and Restore.
        void LoadComponents(const EntityData& InData, EntityRegistry& InEntities, const EntityID InEntity,
                            const ComponentRegistry& InRegistry)
        {
            for (const ComponentData& lComponent : InData.Components)
            {
                const IComponentEntry* lEntry = InRegistry.FindByName(lComponent.TypeName);

                if (lEntry == nullptr)
                {
                    // Unknown component (map from a newer build): warn and skip. It will be lost on the next save.
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
                    // Wrong-typed value or non-object payload (hand-edited or truncated file): warn and continue.
                    // This runs at startup, so an exception must not escape. The component keeps its defaults.
                    OPAAX_LOG(LogMapFactory, Warn,
                              "Component '{}' on entity '{}' could not be read ({}) — left at its defaults.",
                              lComponent.TypeName, InData.Name.CStr(), lError.what());
                }
            }
        }

        // Remove registered components the record does not name (undoes an Add).
        // Essential types refuse by themselves.
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
                // World logged why (invalid or duplicate guid). Skip this entity.
                continue;
            }

            ++lCreated;

            lEntity.Get<EntityMeta>().Parent = lEntityData.Parent;

            LoadComponents(lEntityData, InWorld.GetRegistry(), lEntity.GetHandle(), InRegistry);
        }

        // A parent that never arrived: clear the link (with a warning).
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
                    continue;   // World logged why
                }

                lEntity.Get<EntityMeta>().Parent = lEntityData.Parent;
            }

            LoadComponents(lEntityData, InWorld.GetRegistry(), lEntity.GetHandle(), InRegistry);
            RemoveUnrecorded(lEntityData, InWorld.GetRegistry(), lEntity.GetHandle(), InRegistry);

            ++lRestored;
        }

        // A component written in place does not change the world's revision: bump it (dirty check).
        if (lRestored > 0)
        {
            InWorld.MarkChanged();
        }

        OPAAX_LOG(LogMapFactory, Trace, "Restored {}/{} entity(ies) in world '{}'",
                  lRestored, InData.EntityCount(), InWorld.GetName().CStr());

        return lRestored;
    }
}
