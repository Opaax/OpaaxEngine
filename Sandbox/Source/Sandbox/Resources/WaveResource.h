#pragma once

#include <optional>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/ResourceFormat.h"

namespace Sandbox
{
    // =============================================================================
    // WaveResource — a `.wave` enemy-wave definition, and the first resource type the ENGINE has
    //   never heard of.
    //
    //   It exists to prove the route rather than to parse anything: satisfying CResource is the
    //   whole contract, and OPAAX_RESOURCE_FORMAT is what puts `.wave` in the engine's extension
    //   table so the browser can name the file without the editor knowing what a wave is.
    //
    //   Placeholder policy: a wave that fails to load degrades to an empty one. Nothing drives
    //   logic off it yet, so FailFast would only turn a content typo into a dead session.
    // =============================================================================
    struct WaveResource final
    {
        Opaax::TDynArray<Opaax::Uint8> Bytes;   // the raw definition; parsing is a later milestone

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Wave Definition", ".wave")

        static constexpr Opaax::EFailPolicy FailPolicy = Opaax::EFailPolicy::Placeholder;

        static std::optional<WaveResource> Load(const char* InPath, Opaax::LoadContext& /*InCtx*/)
        {
            WaveResource lResource;
            if (!Opaax::FileIO::ReadAllBytes(Opaax::OpaaxString(InPath), lResource.Bytes))
            {
                return std::nullopt;
            }

            return lResource;
        }

        static WaveResource Placeholder() { return WaveResource{}; }

        Opaax::Uint64 ByteSize() const noexcept { return static_cast<Opaax::Uint64>(Bytes.size()); }
    };
}
