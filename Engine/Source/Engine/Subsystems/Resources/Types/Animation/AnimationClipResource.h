#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipData.h"
#include "Engine/Subsystems/Resources/Types/Animation/AnimationClipFile.h"

namespace Opaax
{
    // =============================================================================
    // AnimationClipResource — a .opaaxclip as a resource.
    //   Placeholder policy: a missing clip leaves the sprite on its authored frame.
    //   Its sheet is not loaded here (needs IPaths); SpriteAnimationSubsystem loads it.
    // =============================================================================
    struct AnimationClipResource final
    {
        AnimationClipData Data;

        // ---- CResource contract --------------------------------------------------
        OPAAX_RESOURCE_FORMAT("Animation Clip", AnimationClipFile::CLIP_EXTENSION)

        static constexpr EFailPolicy FailPolicy = EFailPolicy::Placeholder;

        static std::optional<AnimationClipResource> Load(const char* InPath, LoadContext& /*InCtx*/)
        {
            AnimationClipResource lResource;
            if (!AnimationClipFile::Load(OpaaxString(InPath), lResource.Data))
            {
                return std::nullopt;   // AnimationClipFile already logged why
            }

            return lResource;
        }

        /**
         * An empty clip: finished at once, so the sprite keeps its authored frame.
         */
        static AnimationClipResource Placeholder() { return AnimationClipResource{}; }

        /** Size of the step records. */
        Uint64 ByteSize() const noexcept
        {
            return sizeof(AnimationClipResource)
                 + static_cast<Uint64>(Data.Steps.size()) * sizeof(AnimationStep)
                 + Data.Sheet.Path.GetLength();
        }
    };
}
