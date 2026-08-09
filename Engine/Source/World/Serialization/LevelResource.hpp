#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceConcept.hpp"
#include "World/Serialization/LevelFile.h"

namespace Opaax
{
    // =============================================================================
    // LevelResource — a `.opaaxlevel` as a RESOURCE (**WM4**).
    //
    //   FailFast, for the same reason MapResource is: a level DRIVES what exists, so a
    //   placeholder does not degrade, it lies — the game would come up in an empty world with
    //   nothing anywhere reporting a problem.
    //
    //   A LEVEL'S MAPS ARE NOT `Acquire`d, and this is the class where that rule is enforced
    //   rather than merely described (**WM4**). Acquire is for HARD dependencies: it loads them
    //   inline, chains their refcounts to the parent, and unloading the parent unloads them.
    //   Acquiring a level's maps would therefore load EVERY map in the level the moment the
    //   level loads, and make unloading a single one impossible — the exact opposite of what a
    //   level is for. So the manifest is carried as DATA and whoever wants a map asks for that
    //   map (the world's `Level`, which mounts and unmounts them one at a time).
    //
    //   A Map's TEXTURES will be Acquire'd when a component can name one. A Level's MAPS never
    //   are. The asymmetry is the whole rule.
    // =============================================================================
    struct LevelResource final
    {
        LevelData Data;

        // ---- CResource contract --------------------------------------------------
        static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;

        static std::optional<LevelResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            // InCtx unused BY DESIGN — see the WM4 note above. It is not an oversight to fix
            // later, and a future maintainer reaching for ctx.Acquire<MapResource> here is the
            // mistake this comment exists to stop.
            LevelResource lResource;
            if (!LevelFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // LevelFile already logged which reason it was
            }

            return lResource;
        }

        /** Required by the concept; never handed out under FailFast (the pool answers null). */
        static LevelResource Placeholder() { return LevelResource{}; }
    };
}
