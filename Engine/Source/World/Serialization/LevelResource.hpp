#pragma once

#include <optional>

#include "Resources/ResourceFormat.h"
#include "World/Serialization/LevelFile.h"

namespace Opaax
{
    // =============================================================================
    // LevelResource — a .opaaxlevel as a resource.
    //   FailFast: a missing level would silently give an empty world.
    //   Its maps are not loaded here: the world's Level mounts them one by one
    //   (loading them as dependencies would load all at once and prevent unloading one).
    // =============================================================================
    struct LevelResource final
    {
        LevelData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Opaax Level", LevelFile::LEVEL_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::FailFast;

        static std::optional<LevelResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            // InCtx unused on purpose: do not load the maps here (see above).
            LevelResource lResource;
            if (!LevelFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // LevelFile already logged why
            }

            return lResource;
        }

        /** Required by the concept; never used with FailFast. */
        static LevelResource Placeholder() { return LevelResource{}; }
    };
}
