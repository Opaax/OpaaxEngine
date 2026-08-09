#pragma once

#include "Core/EngineAPI.h"
#include "Application/Services/ILogger.h"

#include "World/Serialization/MapData.h"

namespace Opaax
{
    class World;
    class ComponentRegistry;

    inline constexpr LogCategory LogMapSerializer{"MapSerializer"};

    // =============================================================================
    // MapSerializer — the CAPTURE half of the snapshot core: a live World becomes MapData.
    //
    //   Stateless (every member static, no instance): it is a transformation, and holding
    //   state here would be a second place for the truth to live.
    //
    //   TWO NAMED CAPTURES, NOT ONE WITH A DEFAULTED FILTER (**MP10**). It used to be a single
    //   `Capture(world, registry, filter = {})` where an INVALID filter meant "the whole world" —
    //   and a map's id is invalid exactly when nothing in it claims one, so a caller asking for
    //   an empty map got every entity in the world and no diagnostic. The two operations are
    //   genuinely different questions; giving each a name is what makes the dangerous one
    //   unwritable rather than merely discouraged.
    // =============================================================================
    class OPAAX_API MapSerializer
    {
    public:
        /**
         * EVERY entity in InWorld, whatever authored it — the PIE clone (**WM6**). A clone that
         * dropped runtime-spawned entities would start out already diverged from its source.
         *
         * The result's `Id` is left INVALID: a whole-world snapshot is not a map and never
         * reaches a file.
         *
         * @param InWorld    Read-only — capture never mutates the world it reads.
         * @param InRegistry Decides what is serializable. A component type absent from it is
         *                   simply not written: an unregistered type has no stable name to
         *                   write under, and inventing one would produce a map nothing can load.
         */
        static MapData CaptureWorld(const World& InWorld, const ComponentRegistry& InRegistry);

        /**
         * Only what InMapId authored — "save this map". Stamps the result's `Id`, so the data
         * carries its own name to the file rather than leaving the reader to infer one.
         *
         * A runtime-spawned entity carries an invalid `OwnerMap` (**WM2**), so it can never match
         * a valid id — keeping bullets and VFX out of a saved map falls out of the rule instead
         * of needing a special case.
         *
         * @param InMapId An INVALID id captures NOTHING and warns. It names no map, and the one
         *   thing it must never mean here is "everything".
         * @return The captured data. Empty when nothing matched.
         */
        static MapData CaptureMap(const World& InWorld, const ComponentRegistry& InRegistry, MapId InMapId);
    };
}
