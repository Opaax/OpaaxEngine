#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    // Forward-declared: TResourcePath only needs the name.
    struct AnimationLibraryResource;
    struct AnimationClipResource;

    // =============================================================================
    // SpriteAnimatorComponent — animates a SpriteComponent's frame (via SpriteAnimationSubsystem).
    //   A Library wins when set (Clip names one of its entries); otherwise ClipAsset is played.
    //   While active it controls the sprite's Sheet, Texture and Frame. Play worlds only.
    // =============================================================================
    struct SpriteAnimatorComponent
    {
        /** Asset-relative ("Anims/Hero.opaaxanim"). Wins over ClipAsset. */
        TResourcePath<AnimationLibraryResource> Library;

        /** The library clip, by name. Invalid = the library's DefaultClip. */
        OpaaxStringID Clip;

        /** Asset-relative ("Anims/Coin_Spin.opaaxclip"). Played when Library is empty. */
        TResourcePath<AnimationClipResource> ClipAsset;

        /** Speed multiplier. 0 freezes. */
        float Speed = 1.f;

        /** False freezes on the current step. */
        bool bPlaying = true;

        // =============================================================================
        // Runtime playback state (not saved; a Play copy starts at zero)
        // =============================================================================

        /** Seconds into the current clip. */
        float PlayTime = 0.f;

        /**
         * Path of the playing clip, so a change restarts at zero.
         */
        OpaaxStringID BoundClipPath;

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
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
