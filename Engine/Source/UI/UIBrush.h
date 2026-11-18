#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
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

    // =============================================================================
    // UIBrush — what fills a widget's rect: a tint, and a texture or a sheet frame, 9-sliced by a
    //   border. What an image draws, and what each state of a button draws.
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

        /** 9-slice border in texture pixels. All zero = the sheet frame's own, if it has one. */
        UIMargin Border;

        /** Canvas units a border pixel takes on screen: sizes the corners and edges, not what they show. */
        float BorderScale = 1.f;

        /** True when it names an image (a sheet or a texture); false for a plain colour. */
        bool HasImage() const noexcept { return !Sheet.IsEmpty() || !Texture.IsEmpty(); }

        // _WITH_DEFAULT: a missing key keeps its default.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(UIBrush, Tint, Texture, Sheet, Frame, Border, BorderScale)

        OPAAX_PROPERTIES(UIBrush,
                         OPAAX_PROP(Tint),
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(Sheet).SetTooltip("Art cut from a sheet. Wins over Texture when set."),
                         OPAAX_PROP(Frame).SetRange(-1.f, 4096.f).SetDragStep(1.f).SetTooltip("-1 is the sheet's own default frame."),
                         OPAAX_PROP(Border).SetDragStep(1.f)    // pixels
                             .SetTooltip("9-slice border in texture pixels. All zero: the sheet frame's own."),
                         OPAAX_PROP(BorderScale).SetRange(0.01f, 16.f).SetDragStep(0.01f)
                             .SetTooltip("Canvas units a border pixel takes: 0.5 draws the corners\n"
                                         "half as big, showing the same art."))
    };

    /**
     * The image InBrush shows: InRuntime when set (a texture handed over by code), else its sheet
     * frame, else its texture.
     * @return True with Out filled when something drawable resolved; false when nothing is named
     *   (plain colour) or it is not ready yet (bOutNamed tells them apart)
     */
    bool ResolveBrush(const UIBuildContext& InContext, const UIBrush& InBrush, ITexture2D* InRuntime,
                      UIResolvedImage& Out, bool& bOutNamed);

    /** How DrawBrush went. */
    enum class EUIBrushDraw : Uint8
    {
        Drawn,
        NotReady,    // named but not loaded yet: nothing drawn, the widget rebuilds later
        Stretched    // a tiled image took too many tiles and was stretched instead
    };

    /**
     * Appends the quads InBrush draws over InRect (ResolveBrush, then BuildImageQuads): a plain
     * quad when nothing is named, nothing while what it names is not ready.
     */
    EUIBrushDraw DrawBrush(const UIBuildContext& InContext, const Bounds2D& InRect, const UIBrush& InBrush,
                           ITexture2D* InRuntime, TDynArray<UIQuad>& OutQuads);
}
