#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "Engine/Subsystems/Resources/Types/FontStyle.h"
#include "Renderer/RenderLayer.h"

namespace Opaax
{
    // Only NAMED here — TResourcePath never completes its parameter — so a component header does
    // not drag the resource system into every TU that draws one.
    struct FontFaceResource;
    struct FontFamilyResource;

    // =============================================================================
    // TextComponent — a string drawn in the world. What ⑥ S4's whole font stack exists to serve.
    //
    //   WHERE it draws is TransformComponent's, exactly as a sprite's is. The position is the
    //   TOP-LEFT of the first line and the text runs right and down from it — no alignment, because
    //   centring belongs to a UI pass that does not exist yet.
    //
    //   TWO WAYS TO NAME A TYPEFACE, and the precedence is the contract: a Font family wins when it
    //   is set, otherwise the Face, otherwise nothing is drawn. SpriteComponent's Sheet-over-Texture
    //   rule, and the same reason — a single label, a debug readout, a game whose whole UI is one
    //   weight, must not need a `.opaaxfont` beside it to be usable.
    //
    //   The Style is only consulted through a family. A Face names one file, and that file already
    //   IS a weight and a slant and a script.
    // =============================================================================
    struct TextComponent
    {
        /** What it says. UTF-8, so Greek and Cyrillic are ordinary content. '\n' breaks the line. */
        OpaaxString Text = "Text";

        /** Asset-relative ("/Engine/Fonts/Roboto.opaaxfont"). Set, it WINS over Face. */
        TResourcePath<FontFamilyResource> Font;

        /** Which cut of the family to ask for. Ignored with no Font — a Face is already one cut. */
        FontStyleKey Style;

        /** One `.ttf` directly, for text that needs no family. Used only while Font is empty. */
        TResourcePath<FontFaceResource> Face;

        /** Cap-to-cap height in world units. Far above the 32px bake softens the edges. */
        float Size = 32.f;

        /** Multiplied into the glyph coverage. White draws the face's own anti-aliasing unchanged. */
        LinearColor Color = { 1.f, 1.f, 1.f, 1.f };

        float LineHeightScale = 1.f;

        bool bKerning = true;
        bool bVisible = true;

        /** Coarse band, then the fine tie-break inside it (lower = behind). See RenderLayer.h. */
        ERenderLayer Layer        = ERenderLayer::Default;
        Int16        OrderInLayer = 0;

        // Satisfies CComponent. _WITH_DEFAULT is the required variant, not a preference: the plain
        // macro reads every field with at(), which THROWS on a missing key — so adding a field here
        // would refuse every map saved before it existed, at boot, inside Level::MountAll.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TextComponent,
                                                    Text, Font, Style, Face, Size, Color,
                                                    LineHeightScale, bKerning, bVisible,
                                                    Layer, OrderInLayer)

        // Style is a GROUP, not a widget: it describes its own fields, so the Inspector folds it into
        // a tree node of four dropdowns with no drawer written for it (I15).
        OPAAX_PROPERTIES(TextComponent,
                         OPAAX_PROP(Text),
                         OPAAX_PROP(Font).SetTooltip("A font family. Ignored while empty;\n"
                                                     "set, it WINS over Face."),
                         OPAAX_PROP(Style).SetTooltip("Which cut of the family to ask for.\n"
                                                      "Ignored when no Font is set."),
                         OPAAX_PROP(Face).SetTooltip("One .ttf directly, for text that needs\n"
                                                     "no family asset beside it."),
                         OPAAX_PROP(Size).SetRange(1.f, 512.f)
                                         .SetTooltip("Cap height in world units. The atlas is baked\n"
                                                     "at 32 — far above that, edges soften."),
                         OPAAX_PROP(Color),
                         OPAAX_PROP(LineHeightScale).SetRange(0.5f, 4.f),
                         OPAAX_PROP(bKerning).SetTooltip("Tightens pairs like AV and To."),
                         OPAAX_PROP(bVisible),
                         OPAAX_PROP(Layer),
                         OPAAX_PROP(OrderInLayer))
    };
}
