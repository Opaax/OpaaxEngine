#include "World/Serialization/MapFactory.h"

#include "World/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/World.h"

namespace Opaax
{
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

            for (const ComponentData& lComponent : lEntityData.Components)
            {
                const IComponentEntry* lEntry = InRegistry.FindByName(lComponent.TypeName);

                if (lEntry == nullptr)
                {
                    // Forward compatibility: a map written by a build that knew one more
                    // component type must still open here, minus that component. Warn — this
                    // IS data loss on the next save — but do not fail the load.
                    OPAAX_LOG(LogMapFactory, Warn,
                              "Unknown component '{}' on entity '{}' — skipped (not registered in this build).",
                              lComponent.TypeName.ToString().CStr(), lEntityData.Name.CStr());
                    continue;
                }

                lEntry->Load(InWorld.GetRegistry(), lEntity.GetHandle(), lComponent.Payload);
            }
        }

        OPAAX_LOG(LogMapFactory, Info, "Instantiated {}/{} entity(ies) into world '{}'",
                  lCreated, InData.EntityCount(), InWorld.GetName().CStr());

        return lCreated;
    }
}
