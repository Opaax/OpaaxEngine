#include "Editor/Operation/FileOperations.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Core/IO/FileSystem.h"
#include "Core/Log/Logger.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/UniqueName.h"
#include "Editor/EditorContext.h"
#include "Editor/Operation/ResourceOperations.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceFormat.h"   // NormalizeExtension
#include "World/Components/ComponentRegistry.h"
#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/ResourcePrefabResolver.h"
#include "World/Serialization/EntityClone.h"
#include "World/Serialization/MapData.h"
#include "World/Serialization/MapFile.h"

namespace Opaax::Editor
{
    OPAAX_LOG_CATEGORY(FileOps);

    namespace
    {
        /** InAbsPath's folder and extension, under the next free stem ("Grunt 1"). */
        OpaaxString FreePathBeside(const OpaaxString& InAbsPath)
        {
            const OpaaxString lFolder    = PathString::Parent(InAbsPath);
            const OpaaxString lExtension = PathString::Extension(InAbsPath).ToString();
            const OpaaxString lStem      = UniqueName::Make(PathString::Stem(InAbsPath).ToString(),
                [&lFolder, &lExtension](const OpaaxString& InCandidate)
                {
                    return FileSystem::Exists(PathString::Join(lFolder, InCandidate + lExtension));
                });

            return PathString::Join(lFolder, lStem + lExtension);
        }

        /** A map written under InTo with its own id and new entity guids. */
        bool DuplicateMap(EditorContext& InContext, const OpaaxString& InFrom, const OpaaxString& InTo, OpaaxString& OutError)
        {
            MapData lData;
            if (!MapFile::Load(InFrom, lData))
            {
                OutError = "the map could not be read (see the log)";
                return false;
            }

            const ComponentRegistry& lRegistry = InContext.Engine.GetRegistries().Components();
            ResourcePrefabResolver   lResolver(InContext.Paths, InContext.Resources, lRegistry);

            // Placements as entities, so their guids are made anew with the rest, then folded back.
            PrefabFold::Expand(lData, lResolver, lRegistry);

            EntityClone::Options lOptions;
            lOptions.OwnerMap            = MapFile::StemId(InTo);
            lOptions.bKeepOutsideParents = true;

            MapData lCopy = EntityClone::Clone(lData, lOptions, lRegistry);
            lCopy.Id      = lOptions.OwnerMap;

            PrefabFold::Fold(lCopy, lResolver, lRegistry);

            if (!MapFile::Save(InTo, lCopy))
            {
                OutError = "the copy could not be written (see the log)";
                return false;
            }
            return true;
        }
    }

    OpaaxString FileOps::Duplicate(EditorContext& InContext, const OpaaxString& InAbsPath, OpaaxString* OutError)
    {
        OpaaxString lError;
        const auto  lRefuse = [&lError, OutError, &InAbsPath]()
        {
            OPAAX_LOG(LogFileOps, Warn, "Duplicate '{}' refused — {}", InAbsPath.CStr(), lError.CStr());
            if (OutError != nullptr) { *OutError = lError; }
            return OpaaxString();
        };

        if (InContext.Paths.AbsoluteToAsset(InAbsPath).IsEmpty())
        {
            lError = "it is outside the project's and the engine's assets";
            return lRefuse();
        }
        if (!FileSystem::Exists(InAbsPath))
        {
            lError = "no such file";
            return lRefuse();
        }

        const OpaaxString lCopy = FreePathBeside(InAbsPath);
        const bool        bMap  = NormalizeExtension(PathString::Extension(InAbsPath))
                                == NormalizeExtension(MapFile::MAP_EXTENSION);

        if (bMap ? !DuplicateMap(InContext, InAbsPath, lCopy, lError)
                 : !FileSystem::Copy(InAbsPath, lCopy, /*bInOverwrite*/ false))
        {
            if (lError.IsEmpty()) { lError = "the file could not be copied"; }
            return lRefuse();
        }

        // Announced like a save, so the Resource Browser lists it.
        ResourceOps::SavedToDisk<AssetResource>(InContext, lCopy);

        OPAAX_LOG(LogFileOps, Info, "Duplicated '{}' as '{}'", InAbsPath.CStr(), lCopy.CStr());
        return lCopy;
    }
}
