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
    // Only NAMED here — TResourcePath never completes its parameter — so a component header does
    // not drag the RHI or the resource system into every TU that draws one.
    struct TextureResource;
    struct SpriteSheetResource;

    // =============================================================================
    // SpriteComponent — an image drawn in the world. The first component that references a
    //   RESOURCE, and what the whole texture path exists to serve.
    //
    //   WHERE it draws is TransformComponent's, and so is its rotation — one position per entity,
    //   never one per component. A local Offset (Godot's Sprite2D.offset) is the growth point for
    //   art that sits off its entity's origin; nothing needs it yet.
    //
    //   TWO WAYS TO NAME AN IMAGE, and the precedence is the contract: a Sheet wins when it is set,
    //   otherwise the Texture, otherwise nothing is drawn. Both exist because a plain image — a
    //   backdrop, a UI panel — must not need a `.opaaxsheet` beside it to be usable.
    // =============================================================================
    struct SpriteComponent
    {
        /** Asset-relative ("Textures/Hero.png"). EMPTY draws nothing — a real state, not an error. */
        TResourcePath<TextureResource> Texture;

        /** Asset-relative ("Sheets/Hero.opaaxsheet"). Set, it WINS over Texture. */
        TResourcePath<SpriteSheetResource> Sheet;

        /** Which of the sheet's frames. NEGATIVE = the sheet's own DefaultFrame. Ignored with no sheet. */
        Int32        Frame        = -1;

        Vector2F     Size         = { 100.f, 100.f };

        /** Multiplied into the sample. White draws the texture unchanged. */
        LinearColor  Color        = { 1.f, 1.f, 1.f, 1.f };

        bool         bVisible     = true;

        /** Coarse band, then the fine tie-break inside it (lower = behind). See RenderLayer.h. */
        ERenderLayer Layer        = ERenderLayer::Default;
        Int16        OrderInLayer = 0;

        // Satisfies CComponent. _WITH_DEFAULT is the required variant, not a preference: the plain
        // macro reads every field with at(), which THROWS on a missing key — so adding a field here
        // would refuse every map saved before it existed, at boot, inside Level::MountAll.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SpriteComponent,
                                                    Texture, Sheet, Frame, Size, Color, bVisible, Layer, OrderInLayer)

        // What the Inspector draws, with no drawer written for it. Every field type resolves to a
        // built-in specialization: the path gets a drag & drop target, the layer a dropdown, the
        // colour a picker — each from its TYPE alone (I15).
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
