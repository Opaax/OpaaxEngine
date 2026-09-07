#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverData.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverFile.h"

namespace Opaax
{
    // =============================================================================
    // MoverResource — a `.opaaxmover` as a RESOURCE. AnimationLibraryResource's shape.
    //
    //   PLACEHOLDER: a missing mover resolves no mode name, so the mover does not move. Nothing
    //   crashes and nothing pretends — an entity that stands still is a readable failure.
    //
    //   IT DOES NOT Acquire ITS MODES, for AnimationLibraryResource's reason: LoadContext::Acquire
    //   takes an ABSOLUTE path, and asset-relative -> absolute lives in IPaths, which the Resources
    //   layer does not reach. MoverSubsystem resolves both, through ref caches it owns.
    // =============================================================================
    struct MoverResource final
    {
        MoverData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Mover", MoverFile::MOVER_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<MoverResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            MoverResource lResource;
            if (!MoverFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // MoverFile already logged which reason it was
            }

            return lResource;
        }

        /** An empty mover. Every Find answers nullptr, so nothing moves and nothing breaks. */
        static MoverResource Placeholder() { return MoverResource{}; }

        /** Structural size — the entry records and their paths. */
        Uint64 ByteSize() const noexcept
        {
            Uint64 lBytes = sizeof(MoverResource)
                          + static_cast<Uint64>(Data.Entries.size()) * sizeof(MoverEntry);

            for (const MoverEntry& lEntry : Data.Entries)
            {
                lBytes += lEntry.ModeAsset.Path.GetLength();
            }

            return lBytes;
        }
    };
}
