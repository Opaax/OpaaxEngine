#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryData.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryFile.h"

namespace Opaax
{
    // =============================================================================
    // AnimationLibraryResource — a .opaaxanim as a resource.
    //   Placeholder policy: a missing library leaves the sprite on its authored frame.
    //   Its clips are loaded by SpriteAnimationSubsystem (needs IPaths).
    // =============================================================================
    struct AnimationLibraryResource final
    {
        AnimationLibraryData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Animation Library", AnimationLibraryFile::LIBRARY_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<AnimationLibraryResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            AnimationLibraryResource lResource;
            if (!AnimationLibraryFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // AnimationLibraryFile already logged why
            }

            return lResource;
        }

        /** An empty library: nothing animates. */
        static AnimationLibraryResource Placeholder() { return AnimationLibraryResource{}; }

        /** Size of the entries and their paths. */
        Uint64 ByteSize() const noexcept
        {
            Uint64 lBytes = sizeof(AnimationLibraryResource)
                          + static_cast<Uint64>(Data.Entries.size()) * sizeof(AnimationLibraryEntry);

            for (const AnimationLibraryEntry& lEntry : Data.Entries)
            {
                lBytes += lEntry.Clip.Path.GetLength();
            }

            return lBytes;
        }
    };
}
