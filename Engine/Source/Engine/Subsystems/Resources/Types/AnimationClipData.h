#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringIDJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    struct TextureResource;       // only NAMED — TResourcePath never completes its parameter
    struct SpriteSheetResource;

    // =============================================================================
    // AnimationClipData — ONE animation, as DATA. The `.opaaxclip` payload.
    //
    //   The SpriteSheetData / SpriteSheetFile / SpriteSheetResource stack, one layer over.
    //
    //   A CLIP IS ITS OWN ASSET so the things that hang off a clip — a notify track, curves —
    //   can arrive as one field here, with no library, component or subsystem change. It also
    //   makes a clip reusable across characters; a library-embedded clip is not.
    //
    //   TWO SOURCES, and the precedence is SpriteComponent's own rule one layer down: a clip
    //   with a Sheet reads each step's Frame, one without reads each step's Texture.
    // =============================================================================

    /** What happens when a clip reaches its end. */
    enum class EAnimPlayMode : Uint8
    {
        Once,
        Loop,
        PingPong
    };

    /** I11: the mapping lives with the enum, found by ADL. */
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
        /** SHEET clips: the SpriteFrame's name. Resolved to an index ONCE, when the clip binds. */
        OpaaxStringID Frame;

        /** TEXTURE-LIST clips: the image. Read only when the clip names no sheet. */
        TResourcePath<TextureResource> Texture;

        /**
         * How many ticks this step holds, at the clip's Fps.
         *
         * Integer rather than seconds because pixel-art timing is quantized — Unreal's
         * PaperFlipbook keyframe and Aseprite both count this way, and it removes float drift
         * from the total. Zero is read as one: a step that shows for no time is a typo, not a
         * feature, and dropping it silently would be the wrong-answer failure.
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
     *
     * NO OPAAX_PROPERTIES for Steps — it is a TDynArray and no property drawer draws a list. The
     * editor folds over the clip's own fields and over the SELECTED step, both of which are
     * reflected; the list itself is the panel's UI, which is what a list has to be to be reorderable.
     */
    struct AnimationClipData
    {
        /** Asset-relative ("Sheets/Hero.opaaxsheet"). SET means the steps name frames by Name. */
        TResourcePath<SpriteSheetResource> Sheet;

        TDynArray<AnimationStep> Steps;

        /** Ticks per second. The steps' Hold counts in these. */
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
         * The whole clip's length in ticks — the sum of every step's Hold.
         *
         * Inline like every other member here: the struct carries no OPAAX_API (it is plain data,
         * **I6**), so a member defined in the DLL's .cpp is unresolvable from the exe. Caught by
         * the test link, which is the first thing outside the DLL to name one.
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

        /** The step InIndex names, or nullptr. Out of range answers null rather than clamping. */
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
     * Which step InClip is showing InTimeSeconds after it started.
     *
     * STATELESS — the answer is a pure function of the time, never of the previous frame — so
     * playback cannot drift and a frame hitch skips ahead rather than queueing up the frames it
     * missed. That is also what makes it testable with no world, no GL and no clock, the way
     * MakeFrameUV, SliceGrid and PlanQuadBatches are.
     *
     * `bFinished` means "nothing new will be shown from here": the end of a Once clip, and any
     * clip that cannot advance at all (no steps, a zero Fps). A Loop or PingPong clip that can
     * advance never finishes.
     *
     * Degenerate input answers step 0 — never a division by zero, never an out-of-range index.
     */
    OPAAX_API AnimationSample SampleClip(const AnimationClipData& InClip, float InTimeSeconds);
}
