#pragma once

#include <optional>

#include "Resources/ResourceFormat.h"
#include "Movement/Assets/MoveModeData.h"
#include "Movement/Assets/MoveModeFile.h"

namespace Opaax
{
    // =============================================================================
    // MoveModeResource — a .opaaxmovemode as a resource.
    //   Placeholder policy: a missing tuning gives the defaults (a walking mode).
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
                return std::nullopt;   // MoveModeFile already logged why
            }

            return lResource;
        }

        /** The defaults: a walking mode. */
        static MoveModeResource Placeholder() { return MoveModeResource{}; }

        /** Plain data. */
        Uint64 ByteSize() const noexcept { return sizeof(MoveModeResource); }
    };
}
