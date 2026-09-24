#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    // Only NAMED here — TResourcePath never completes its parameter — so a component header does
    // not drag the resource system into every TU that animates one.
    struct AnimationLibraryResource;
    struct AnimationClipResource;

    // =============================================================================
    // SpriteAnimatorComponent — what drives a SpriteComponent's frame over time.
    //
    //   D7 exactly: authoring DATA, advanced by a world subsystem, never a polymorphic component.
    //   It is additive — SpriteAnimationSubsystem writes into the SpriteComponent the renderer
    //   already reads, so nothing in Renderer2D or RendererManager knows animation exists.
    //
    //   TWO WAYS TO NAME A CLIP, and the precedence is the contract: a Library wins when set and
    //   Clip names an entry in it; otherwise ClipAsset is played directly. Both exist because a
    //   one-off animated prop — a spinning coin — must not need a library to hold its single clip,
    //   while a hero with five states must not pay a string copy to switch between them.
    //   (SpriteComponent's own Sheet|Texture fork, one layer over.)
    //
    //   WHILE THIS DRIVES A SPRITE IT OWNS THAT SPRITE'S Sheet, Texture AND Frame. The sprite's
    //   authored values are what shows when no animator is present — which, since the subsystem is
    //   Play-only, is exactly what the editor viewport keeps showing.
    // =============================================================================
    struct SpriteAnimatorComponent
    {
        /** Asset-relative ("Anims/Hero.opaaxanim"). Set, it WINS over ClipAsset. */
        TResourcePath<AnimationLibraryResource> Library;

        /** Which of the library's clips, by its short name. INVALID = the library's DefaultClip. */
        OpaaxStringID Clip;

        /** Asset-relative ("Anims/Coin_Spin.opaaxclip"). Played when Library is empty. */
        TResourcePath<AnimationClipResource> ClipAsset;

        /** Per-entity multiplier on the clip's Fps. 0 freezes without changing bPlaying. */
        float Speed = 1.f;

        /** False freezes on the CURRENT step rather than reverting to the authored frame. */
        bool bPlaying = true;

        // =============================================================================
        // Transient — playback state, in NEITHER the json macro NOR the property table
        //
        //   So it cannot reach a `.opaaxmap`, and a PIE clone round-trips through the map
        //   snapshot (MP5/WM6) — which means a cloned world starts every animation at zero for
        //   free, with nothing to reset. Unreal's PaperFlipbookComponent::AccumulatedTime is the
        //   same shape.
        // =============================================================================

        /** Seconds into the current clip. */
        float PlayTime = 0.f;

        /**
         * The interned PATH of the clip being played, so a switch restarts at zero.
         *
         * The path rather than the name, because it is the one identity both routes share: a
         * library entry and a direct ClipAsset both resolve to a file.
         */
        OpaaxStringID BoundClipPath;

        // Satisfies CComponent. _WITH_DEFAULT is required, not preferred: the plain macro reads
        // every field with at(), which THROWS on a missing key, so adding a field here would
        // refuse every map saved before it existed — at boot, inside Level::MountAll.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpriteAnimatorComponent,
                                                    Library, Clip, ClipAsset, Speed, bPlaying)

        OPAAX_PROPERTIES(SpriteAnimatorComponent,
                         OPAAX_PROP(Library).SetTooltip("A set of named clips. Set, it WINS over Clip Asset."),
                         OPAAX_PROP(Clip).SetTooltip("Which of the library's clips, by name.\n"
                                                     "Empty plays the library's default."),
                         OPAAX_PROP(ClipAsset).SetTooltip("A single clip, for something that needs no\n"
                                                          "library. Ignored while a Library is set."),
                         OPAAX_PROP(Speed).SetRange(0.f, 16.f)
                                          .SetTooltip("Multiplies the clip's Fps for this entity."),
                         OPAAX_PROP(bPlaying).SetTooltip("Unchecked freezes on the current step."))
    };
}
