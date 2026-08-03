#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Application/Services/ILogger.h"

namespace Opaax
{
    class World;
    class ComponentRegistry;
    class ResourceManager;
    class IPaths;
    struct LevelData;

    inline constexpr LogCategory LogLevelLoader{"LevelLoader"};

    // =============================================================================
    // LevelLoader — open a Level's Maps INTO a World.
    //
    //   The one place the three M5 layers meet: LevelFile says which maps, MapResource reads
    //   each one, MapFactory turns it into entities. Nothing above this has to know that order.
    //
    //   MAPS ARE LOADED THROUGH THE ResourceManager, not by calling MapFile::Load directly.
    //   That is deliberate on two counts: it gives MapResource a real caller on every boot
    //   rather than only in its own test, and two worlds opening the same map then parse it
    //   once. The ref is dropped as soon as the entities are instantiated — the parsed data has
    //   no further use, and holding it would be the refcount-without-a-purpose that deferring
    //   LevelManager exists to avoid.
    //
    //   ADDITIVE, like MapFactory::Instantiate: loading a level does NOT clear the world first.
    //   A caller that wants a replace calls World::Clear itself, which is what the editor's
    //   Open Map does and what streaming a second level in must NOT do.
    //
    //   Free functions, not a class: no state, nothing to derive from, and the two entry points
    //   are one calling the other.
    // =============================================================================
    namespace LevelLoader
    {
        // What a load actually did. Returned rather than logged-and-forgotten so a caller can
        // tell "the level was empty" from "every map in it failed" — an empty world looks
        // identical either way, which is precisely the confusion FailFast exists to prevent.
        struct Result
        {
            Uint64 MapsLoaded      = 0;
            Uint64 MapsFailed      = 0;
            Uint64 EntitiesCreated = 0;

            /** Nothing went wrong AND something arrived. An empty level is not a success. */
            bool IsOk() const noexcept { return MapsFailed == 0 && MapsLoaded > 0; }
        };

        /**
         * Instantiate every map InLevel names into InWorld.
         *
         * A map that fails to load is COUNTED AND SKIPPED rather than abandoning the rest: one
         * missing file in a ten-map level should cost that map, not the level. The caller reads
         * MapsFailed to decide how loud to be about it.
         *
         * @param InLevel     Map paths, ASSET-RELATIVE (resolved through IPaths::AssetToAbsolute).
         * @param InWorld     Receives the entities. Not cleared first — see the class note.
         * @param InRegistry  Decides which components can be rebuilt; an unknown one is skipped
         *                    with a warning by MapFactory, exactly as for a PIE clone.
         */
        OPAAX_API Result LoadInto(const LevelData& InLevel, World& InWorld,
                                  const ComponentRegistry& InRegistry,
                                  const IPaths& InPaths, ResourceManager& InResources);

        /**
         * Read the level at InAssetRelPath, then LoadInto.
         *
         * The entry point a host uses, so that "open this level" is one call rather than a
         * sequence a caller could get wrong.
         *
         * @param InAssetRelPath Asset-relative, e.g. "Levels/Main.opaaxlevel" — the form
         *                       `startupLevel` takes in the .opaaxproj.
         * @return A Result whose counts are all zero when the level file itself could not be read.
         */
        OPAAX_API Result LoadLevelInto(const OpaaxString& InAssetRelPath, World& InWorld,
                                       const ComponentRegistry& InRegistry,
                                       const IPaths& InPaths, ResourceManager& InResources);
    }
}
