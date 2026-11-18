#pragma once

#include <nlohmann/json.hpp>

#include "Animation/Assets/AnimationClip.h"   // complete: a widget class visits its clip reference
#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Log/Logger.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Renderer/Textures/SpriteSheet.h"   // complete: a widget class visits its sheet reference
#include "Renderer/Textures/Texture2D.h"     // and its texture reference
#include "Resources/Asset/AssetRef.h"
#include "UI/UIImageSource.h"
#include "UI/UIMargin.h"
#include "UI/UIQuad.h"

namespace Opaax
{
    class ITexture2D;

    inline constexpr LogCategory LogUIBrush{"UIBrush"};

    // =============================================================================
    // UIBrush — what fills a widget's rect: a tint, and a clip, a sheet frame or a texture, 9-sliced
    //   by a border. What an image draws, and what each state of a button draws.
    // =============================================================================
    struct UIBrush
    {
        /** Multiplies the image; the colour of a brush with no image. */
        LinearColor Tint;

        /** A texture path (accepts a dropped .png). */
        TAssetRef<Texture2D> Texture;

        /** A sheet path. Wins over Texture. */
        TAssetRef<SpriteSheet> Sheet;

        /** Sheet frame; -1 = the sheet's default. */
        Int32 Frame = -1;

        /** A clip played from when its owner starts it. Wins over Sheet and Texture, which show if it does not load. */
        TAssetRef<AnimationClip> Animation;

        /** 9-slice border in texture pixels. All zero = the sheet frame's own, if it has one. */
        UIMargin Border;

        /** Canvas units a border pixel takes on screen: sizes the corners and edges, not what they show. */
        float BorderScale = 1.f;

        /** A plain InTint rect (no image). */
        static UIBrush FromTint(const LinearColor& InTint)
        {
            UIBrush lBrush;
            lBrush.Tint = InTint;
            return lBrush;
        }

        /** True when it names an image (a clip, a sheet or a texture); false for a plain colour. */
        bool HasImage() const noexcept { return !Animation.IsEmpty() || !Sheet.IsEmpty() || !Texture.IsEmpty(); }

        // _WITH_DEFAULT: a missing key keeps its default.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UIBrush, Tint, Texture, Sheet, Frame, Animation, Border, BorderScale)

        OPAAX_PROPERTIES(UIBrush,
                         OPAAX_PROP(Tint),
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(Sheet).SetTooltip("Art cut from a sheet. Wins over Texture when set."),
                         OPAAX_PROP(Frame).SetRange(-1.f, 4096.f).SetDragStep(1.f).SetTooltip("-1 is the sheet's own default frame."),
                         OPAAX_PROP(Animation).SetTooltip("A clip, played while shown in a running game or Play.\n"
                                                          "Wins over Sheet and Texture, which show if it does not load."),
                         OPAAX_PROP(Border).SetDragStep(1.f)    // pixels
                             .SetTooltip("9-slice border in texture pixels. All zero: the sheet frame's own."),
                         OPAAX_PROP(BorderScale).SetRange(0.01f, 16.f).SetDragStep(0.01f)
                             .SetTooltip("Canvas units a border pixel takes: 0.5 draws the corners\n"
                                         "half as big, showing the same art."))
    };

    /** What went wrong with a brush's clip; it still draws something. */
    enum class EUIBrushIssue : Uint8
    {
        None,
        ClipMissing,     // the clip did not load: the brush's Sheet or Texture shows
        SheetMissing,    // the clip's sheet did not load: no frame name resolves, its default frame shows
        FrameUnknown     // a step names a frame its sheet does not have: the sheet's default frame shows
    };

    /** Where a brush's clip is at a moment. */
    struct UIBrushClip
    {
        const AnimationClip* Clip = nullptr;   // null when none is named or it did not load
        Uint32               Step = 0;
        bool                 bFinished = true; // nothing new will show (a Once clip at its end)
        EUIBrushIssue        Issue = EUIBrushIssue::None;
    };

    /**
     * InBrush's clip InSeconds after it started (SampleClip, stateless). Its clip is read through
     * InAssets each time: a saved clip plays its new steps.
     */
    UIBrushClip SampleBrushClip(IUIAssetProvider* InAssets, const UIBrush& InBrush, float InSeconds);

    /**
     * The image InBrush shows: InRuntime when set (a texture handed over by code), else its clip's step
     * at InSeconds (its frame found by name in the clip's sheet, or its texture), else its sheet frame,
     * else its texture. OutClip, when given, says where the clip was and what went wrong.
     * @return True with Out filled when something drawable resolved; false when nothing is named
     *   (plain colour) or it is not ready yet (bOutNamed tells them apart)
     */
    bool ResolveBrush(const UIBuildContext& InContext, const UIBrush& InBrush, ITexture2D* InRuntime,
                      float InSeconds, UIResolvedImage& Out, bool& bOutNamed, UIBrushClip* OutClip = nullptr);

    /** How DrawBrush went. */
    enum class EUIBrushDraw : Uint8
    {
        Drawn,
        NotReady,    // named but not loaded yet: nothing drawn, the widget rebuilds later
        Stretched    // a tiled image took too many tiles and was stretched instead
    };

    /**
     * Appends the quads InBrush draws over InRect at InSeconds into its clip (ResolveBrush, then
     * BuildImageQuads): a plain quad when nothing is named, nothing while what it names is not ready.
     */
    EUIBrushDraw DrawBrush(const UIBuildContext& InContext, const Bounds2D& InRect, const UIBrush& InBrush,
                           ITexture2D* InRuntime, float InSeconds, TDynArray<UIQuad>& OutQuads,
                           UIBrushClip* OutClip = nullptr);

    /**
     * What InBrush draws, for automation and logs: "clip#step" while its clip plays, "sheet#frame",
     * "texture", "runtime" for a texture handed over by code, "none" for a plain colour.
     */
    OpaaxString DescribeBrush(const UIBrush& InBrush, bool bInRuntime, const UIBrushClip& InClip);

    /** The message for InIssue on InBrush, to log once; empty for None. */
    OpaaxString DescribeBrushIssue(const UIBrush& InBrush, const UIBrushClip& InClip);
}
