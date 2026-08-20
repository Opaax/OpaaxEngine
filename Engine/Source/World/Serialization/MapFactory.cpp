#include "World/Serialization/MapFactory.h"

#include "World/Components/ComponentRegistry.h"
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
                              lComponent.TypeName, lEntityData.Name.CStr());
                    continue;
                }

                try
                {
                    lEntry->Load(InWorld.GetRegistry(), lEntity.GetHandle(), lComponent.Payload);
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
                              lComponent.TypeName, lEntityData.Name.CStr(), lError.what());
                }
            }
        }

        OPAAX_LOG(LogMapFactory, Info, "Instantiated {}/{} entity(ies) into world '{}'",
                  lCreated, InData.EntityCount(), InWorld.GetName().CStr());

        return lCreated;
    }
}
