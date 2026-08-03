#include "World/Serialization/LevelLoader.h"

#include "Application/Services/IPaths.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"   // before the resources — completes LoadContext
#include "World/ComponentRegistry.h"
#include "World/Serialization/LevelResource.hpp"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapResource.hpp"
#include "World/World.h"

namespace Opaax
{
    LevelLoader::Result LevelLoader::LoadInto(const LevelData& InLevel, World& InWorld,
                                              const ComponentRegistry& InRegistry,
                                              const IPaths& InPaths, ResourceManager& InResources)
    {
        Result lResult;

        for (const OpaaxString& lMapRelPath : InLevel.Maps)
        {
            const OpaaxString lAbsPath = InPaths.AssetToAbsolute(lMapRelPath);

            // Through the manager, not MapFile::Load — see the header. The Ref is scoped to this
            // iteration: once the entities exist, the parsed MapData has no further consumer.
            const ResourceRef<MapResource> lRef = InResources.Load<MapResource>(lAbsPath.CStr());
            const MapResource* const       lMap = lRef.Get();

            if (lMap == nullptr)
            {
                // FailFast: a missing map resolves to null rather than to an empty placeholder,
                // which is the whole reason the policy is what it is. Count it and carry on —
                // one bad file should cost that map, not the level.
                OPAAX_LOG(LogLevelLoader, Error, "Map '{}' failed to load — skipped", lMapRelPath.CStr())
                ++lResult.MapsFailed;
                continue;
            }

            ++lResult.MapsLoaded;
            lResult.EntitiesCreated += MapFactory::Instantiate(lMap->Data, InWorld, InRegistry);
        }

        return lResult;
    }

    LevelLoader::Result LevelLoader::LoadLevelInto(const OpaaxString& InAssetRelPath, World& InWorld,
                                                   const ComponentRegistry& InRegistry,
                                                   const IPaths& InPaths, ResourceManager& InResources)
    {
        const OpaaxString lAbsPath = InPaths.AssetToAbsolute(InAssetRelPath);

        const ResourceRef<LevelResource> lRef   = InResources.Load<LevelResource>(lAbsPath.CStr());
        const LevelResource* const       lLevel = lRef.Get();

        if (lLevel == nullptr)
        {
            OPAAX_LOG(LogLevelLoader, Error, "Level '{}' failed to load — world left empty",
                      InAssetRelPath.CStr())
            return Result{};
        }

        Result lResult = LoadInto(lLevel->Data, InWorld, InRegistry, InPaths, InResources);

        // The SUCCESS branch says what actually arrived, not merely that nothing failed — an
        // empty world and a loaded one look identical in a log that only reports errors ([[L15]]).
        OPAAX_LOG(LogLevelLoader, Info, "Level '{}' -> world '{}': {} map(s), {} entity(ies){}",
                  InAssetRelPath.CStr(), InWorld.GetName().CStr(),
                  lResult.MapsLoaded, lResult.EntitiesCreated,
                  lResult.MapsFailed > 0 ? " (some maps FAILED — see above)" : "")

        return lResult;
    }
}
