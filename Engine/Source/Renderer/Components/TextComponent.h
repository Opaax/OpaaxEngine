#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"
#include "Renderer/Text/FontStyle.h"
#include "Renderer/RenderLayer.h"

namespace Opaax
{
    // Forward-declared: TResourcePath only needs the name.
    struct FontFaceResource;
    struct FontFamilyResource;

    // =============================================================================
    // TextComponent — text drawn in the world. The Transform is the top-left of the first line.
    //   A Font family wins when set, otherwise the Face, otherwise nothing is drawn.
    //   Style is only used with a family.
    // =============================================================================
    struct TextComponent
    {
        /** UTF-8 text. '\n' breaks the line. */
        OpaaxString Text = "Text";

        /** Asset-relative ("/Engine/Fonts/Roboto.opaaxfont"). Wins over Face. */
        TResourcePath<FontFamilyResource> Font;

        /** Style to ask the family for. Ignored without a Font. */
        FontStyleKey Style;

        /** A single .ttf, used when Font is empty. */
        TResourcePath<FontFaceResource> Face;

        /** Cap height in world units. */
        float Size = 32.f;

        /** Multiplied with the glyphs. */
        LinearColor Color = { 1.f, 1.f, 1.f, 1.f };

        float LineHeightScale = 1.f;

        bool bKerning = true;
        bool bVisible = true;

        /** Layer, then order within it (lower = behind). See RenderLayer.h. */
        ERenderLayer Layer        = ERenderLayer::Default;
        Int16        OrderInLayer = 0;

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TextComponent,
                                                    Text, Font, Style, Face, Size, Color,
                                                    LineHeightScale, bKerning, bVisible,
                                                    Layer, OrderInLayer)

        // Style shows as a group of four dropdowns.
        OPAAX_PROPERTIES(TextComponent,
                         OPAAX_PROP(Text).SetFlags(EPropertyFlags::Multiline)
                                         .SetTooltip("Enter breaks the line. UTF-8, so Greek and\n"
                                                     "Cyrillic are ordinary content."),
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
