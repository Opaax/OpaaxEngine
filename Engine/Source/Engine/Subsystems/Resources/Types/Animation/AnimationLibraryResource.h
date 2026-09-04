#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryData.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationLibraryFile.h"

namespace Opaax
{
    // =============================================================================
    // AnimationLibraryResource — a `.opaaxanim` as a RESOURCE. AnimationClipResource's shape.
    //
    //   PLACEHOLDER: a missing library resolves no clip name, so the animator leaves the sprite's
    //   AUTHORED frame alone. Degraded and visible, never a lie.
    //
    //   IT DOES NOT Acquire ITS CLIPS, for SS3's placement reason: LoadContext::Acquire takes an
    //   ABSOLUTE path and asset-relative -> absolute lives in IPaths, which the Resources layer
    //   does not reach. SpriteAnimationSubsystem resolves both, through the ref caches it owns.
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
                return std::nullopt;   // AnimationLibraryFile already logged which reason it was
            }

            return lResource;
        }

        /** An empty library. Every Find answers nullptr, so nothing animates and nothing breaks. */
        static AnimationLibraryResource Placeholder() { return AnimationLibraryResource{}; }

        /** Structural size — the entry records and their paths. */
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
