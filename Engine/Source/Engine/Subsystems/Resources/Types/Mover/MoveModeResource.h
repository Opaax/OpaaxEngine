#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeData.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeFile.h"

namespace Opaax
{
    // =============================================================================
    // MoveModeResource — a `.opaaxmovemode` as a RESOURCE. AnimationClipResource's shape.
    //
    //   PLACEHOLDER: a missing tuning yields the DEFAULTS, which is a mover that walks at a
    //   sensible speed rather than one that cannot move at all. Degraded and visible — the thing
    //   still responds, it just does not respond the way it was authored to.
    // =============================================================================
    struct MoveModeResource final
    {
        MoveModeData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Move Mode", MoveModeFile::MOVE_MODE_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<MoveModeResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            MoveModeResource lResource;
            if (!MoveModeFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // MoveModeFile already logged which reason it was
            }

            return lResource;
        }

        /** The defaults: a walkable ground mode. Something moves rather than nothing. */
        static MoveModeResource Placeholder() { return MoveModeResource{}; }

        /** Flat POD — the tuning is floats and one interned id. */
        Uint64 ByteSize() const noexcept { return sizeof(MoveModeResource); }
    };
}
