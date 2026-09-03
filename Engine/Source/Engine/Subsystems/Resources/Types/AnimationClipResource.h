#pragma once

#include <optional>

#include "Engine/Subsystems/Resources/ResourceFormat.h"
#include "Engine/Subsystems/Resources/Types/AnimationClipData.h"
#include "Engine/Subsystems/Resources/Types/AnimationClipFile.h"

namespace Opaax
{
    // =============================================================================
    // AnimationClipResource — a `.opaaxclip` as a RESOURCE. SpriteSheetResource's shape exactly: a
    //   plain struct satisfying CResource whose whole body is an adapter, so the file format stays
    //   in AnimationClipFile and going through the ResourceManager buys dedup and a lifetime nobody
    //   hand-manages.
    //
    //   PLACEHOLDER, not FailFast — a missing clip means a sprite keeps its AUTHORED frame instead
    //   of animating. Degraded, visible, and survivable, which is ResourceConcept's deciding
    //   question. (A map is FailFast because an empty map does not degrade, it lies.)
    //
    //   IT DOES NOT Acquire ITS SHEET, for SS3's placement reason and not laziness:
    //   LoadContext::Acquire takes an ABSOLUTE path, and asset-relative -> absolute lives in
    //   IPaths, an app service the Resources layer does not reach. The animation subsystem
    //   resolves it, through the ref cache it already owns for exactly that lifetime job.
    //
    //   No Initialize(): nothing here touches the GPU, so the payload is complete when Load returns.
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
                return std::nullopt;   // AnimationClipFile already logged which reason it was
            }

            return lResource;
        }

        /**
         * An empty clip naming no sheet. What a sprite pointed at a missing `.opaaxclip` gets: no
         * steps, so SampleClip reports finished and the animator leaves the sprite's authored
         * frame alone rather than blanking it.
         */
        static AnimationClipResource Placeholder() { return AnimationClipResource{}; }

        /** Structural size — the step records, which is all a clip holds. */
        Uint64 ByteSize() const noexcept
        {
            return sizeof(AnimationClipResource)
                 + static_cast<Uint64>(Data.Steps.size()) * sizeof(AnimationStep)
                 + Data.Sheet.Path.GetLength();
        }
    };
}
