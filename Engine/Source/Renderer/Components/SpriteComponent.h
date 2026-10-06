#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"
#include "Renderer/RenderLayer.h"

namespace Opaax
{
    // Forward-declared: TResourcePath only needs the name.
    struct TextureResource;
    struct SpriteSheetResource;

    // =============================================================================
    // SpriteComponent — an image drawn in the world, at the entity's Transform.
    //   A Sheet wins when set, otherwise the Texture, otherwise nothing is drawn.
    // =============================================================================
    struct SpriteComponent
    {
        /** Asset-relative ("Textures/Hero.png"). Empty draws nothing. */
        TResourcePath<TextureResource> Texture;

        /** Asset-relative ("Sheets/Hero.opaaxsheet"). Wins over Texture. */
        TResourcePath<SpriteSheetResource> Sheet;

        /** Sheet frame. Negative = the sheet's DefaultFrame. Ignored without a sheet. */
        Int32        Frame        = -1;

        Vector2F     Size         = { 100.f, 100.f };

        /** Multiplied with the texture. White keeps it unchanged. */
        LinearColor  Color        = { 1.f, 1.f, 1.f, 1.f };

        bool         bVisible     = true;

        /** Layer, then order within it (lower = behind). See RenderLayer.h. */
        ERenderLayer Layer        = ERenderLayer::Default;
        Int16        OrderInLayer = 0;

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpriteComponent,
                                                    Texture, Sheet, Frame, Size, Color, bVisible, Layer, OrderInLayer)

        // Editable in the Inspector (each field type has its own drawer).
        OPAAX_PROPERTIES(SpriteComponent,
                         OPAAX_PROP(Texture).SetTooltip("The whole image. Ignored while a Sheet is set."),
                         OPAAX_PROP(Sheet).SetTooltip("A sliced image. Set, it WINS over Texture."),
                         OPAAX_PROP(Frame).SetRange(-1.f, 4096.f)
                                          .SetTooltip("Which of the sheet's frames to draw.\n"
                                                      "-1 means the sheet's own default frame."),
                         OPAAX_PROP(Size).SetRange(1.f, 4096.f)
                                         .SetTooltip("World size. The frame decides WHAT is drawn,\n"
                                                     "this decides how big — they are separate."),
                         OPAAX_PROP(Color),
                         OPAAX_PROP(bVisible),
                         OPAAX_PROP(Layer),
                         OPAAX_PROP(OrderInLayer))
    };
}
