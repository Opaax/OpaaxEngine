#pragma once

#include <optional>

#include "Core/IO/FileIO.h"
#include "Engine/Subsystems/Resources/ResourceFormat.h"

namespace Sandbox
{
    // =============================================================================
    // WaveResource — a .wave enemy-wave definition, a resource type the engine does not know.
    //   Satisfying CResource is the whole contract; OPAAX_RESOURCE_FORMAT registers the .wave
    //   extension. Not parsed yet. A wave that fails to load becomes an empty one.
    // =============================================================================
    struct WaveResource final
    {
        Opaax::TDynArray<Opaax::Uint8> Bytes;   // the raw definition (not parsed yet)

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
