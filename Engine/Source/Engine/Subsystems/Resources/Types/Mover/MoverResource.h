#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverData.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverFile.h"

namespace Opaax
{
    // =============================================================================
    // MoverResource — a .opaaxmover as a resource.
    //   Placeholder policy: an empty mover (does not move).
    //   Its modes are loaded by MoverSubsystem (needs IPaths).
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
                return std::nullopt;   // MoverFile already logged why
            }

            return lResource;
        }

        /** An empty mover: nothing moves. */
        static MoverResource Placeholder() { return MoverResource{}; }

        /** Size of the entries and their paths. */
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
