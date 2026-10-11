#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct TextureResource;
    struct SpriteSheetResource;

    // =============================================================================
    // AnimationClipData — one animation (.opaaxclip).
    //   With a Sheet, each step names a sheet frame; without, each step names a texture.
    // =============================================================================

    /** What happens when a clip reaches its end. */
    enum class EAnimPlayMode : Uint8
    {
        Once,
        Loop,
        PingPong
    };

    /** Enum to string. */
    inline const char* ToString(const EAnimPlayMode InMode) noexcept
    {
        switch (InMode)
        {
        case EAnimPlayMode::Once:     return "Once";
        case EAnimPlayMode::Loop:     return "Loop";
        case EAnimPlayMode::PingPong: return "PingPong";
        }

        return "Loop";
    }
}

OPAAX_ENUM_VALUES(Opaax::EAnimPlayMode, Once, Loop, PingPong)

namespace Opaax
{
    /** One entry in a clip: which picture, and how long it holds. */
    struct AnimationStep
    {
        /** Sheet clips: the frame name (resolved once, when the clip binds). */
        OpaaxStringID Frame;

        /** Texture clips: the image. Only used when the clip has no sheet. */
        TResourcePath<TextureResource> Texture;

        /**
         * How many ticks this step lasts, at the clip's Fps. Zero counts as one.
         */
        Uint32 Hold = 1;

        Uint32 EffectiveHold() const noexcept { return (Hold > 0u) ? Hold : 1u; }

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AnimationStep, Frame, Texture, Hold)

        OPAAX_PROPERTIES(AnimationStep,
                         OPAAX_PROP(Frame).SetTooltip("Which of the sheet's frames, by name.\n"
                                                      "Ignored when the clip names no sheet."),
                         OPAAX_PROP(Texture).SetTooltip("The image for this step. Used only when\n"
                                                        "the clip has no sheet."),
                         OPAAX_PROP(Hold).SetRange(1.f, 512.f)
                                         .SetTooltip("How many ticks this step holds, at the clip's Fps."))
    };

    /**
     * A clip: which pictures, in which order, and how fast.
     * Steps are edited by the editor panel (lists are not drawn by the property system).
     */
    struct AnimationClipData
    {
        /** Asset-relative ("Sheets/Hero.opaaxsheet"). When set, steps name frames. */
        TResourcePath<SpriteSheetResource> Sheet;

        TDynArray<AnimationStep> Steps;

        /** Ticks per second. */
        float Fps = 12.f;

        EAnimPlayMode PlayMode = EAnimPlayMode::Loop;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AnimationClipData, Sheet, Steps, Fps, PlayMode)

        OPAAX_PROPERTIES(AnimationClipData,
                         OPAAX_PROP(Sheet).SetTooltip("The sliced image the steps name frames in.\n"
                                                      "Empty means the steps carry their own textures."),
                         OPAAX_PROP(Fps).SetRange(0.f, 240.f)
                                        .SetTooltip("Ticks per second. A step's Hold counts in these."),
                         OPAAX_PROP(PlayMode).SetTooltip("What happens at the end of the clip."))

        Uint32 StepCount() const noexcept { return static_cast<Uint32>(Steps.size()); }

        /**
         * Clip length in ticks (sum of every step's Hold).
         */
        Uint32 TotalTicks() const noexcept
        {
            Uint32 lTotal = 0u;

            for (const AnimationStep& lStep : Steps)
            {
                lTotal += lStep.EffectiveHold();
            }

            return lTotal;
        }

        /** The step at InIndex, or nullptr if out of range. */
        const AnimationStep* StepAt(Uint32 InIndex) const noexcept
        {
            return (InIndex < StepCount()) ? &Steps[InIndex] : nullptr;
        }
    };

    /** Which step is showing, and whether the clip will ever show anything else. */
    struct AnimationSample
    {
        Uint32 Step      = 0;
        bool   bFinished = false;
    };

    /**
     * Which step InClip shows InTimeSeconds after it started. Stateless (no drift; a hitch skips ahead).
     * bFinished: nothing new will be shown (end of a Once clip, or a clip that cannot advance).
     * Loop and PingPong clips never finish. Bad input gives step 0.
     */
    AnimationSample SampleClip(const AnimationClipData& InClip, float InTimeSeconds);
}
