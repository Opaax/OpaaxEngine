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
    // TextureResource is only NAMED here — TResourcePath never completes its parameter — so a
    // component header does not drag the RHI into every TU that draws one.
    struct TextureResource;

    // =============================================================================
    // SpriteComponent — an image drawn in the world. The first component that references a
    //   RESOURCE, and what the whole texture path exists to serve.
    //
    //   Position lives here rather than on a transform because no transform component exists yet;
    //   it moves the day one does, and so does DummyComponent's. Rotation is deliberately absent
    //   for the same reason: Renderer2D::DrawSprite takes one, but a rotation that a transform will
    //   own belongs to the transform, not to two components that would then disagree.
    //
    //   No UVs either. The renderer's call takes them so a sprite sheet needs no second entry
    //   point, but hand-typed atlas floats are worse authoring than none — a sheet is its own
    //   resource type, with its own editor.
    // =============================================================================
    struct SpriteComponent
    {
        /** Asset-relative ("Textures/Hero.png"). EMPTY draws nothing — a real state, not an error. */
        TResourcePath<TextureResource> Texture;

        Vector2F     Position     = { 0.f, 0.f };
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
                                                    Texture, Position, Size, Color, bVisible, Layer, OrderInLayer)

        // What the Inspector draws, with no drawer written for it. Every field type resolves to a
        // built-in specialization: the path gets a drag & drop target, the layer a dropdown, the
        // colour a picker — each from its TYPE alone (I15).
        OPAAX_PROPERTIES(SpriteComponent,
                         OPAAX_PROP(Texture),
                         OPAAX_PROP(Position),
                         OPAAX_PROP(Size).SetRange(1.f, 4096.f),
                         OPAAX_PROP(Color),
                         OPAAX_PROP(bVisible),
                         OPAAX_PROP(Layer),
                         OPAAX_PROP(OrderInLayer))
    };
}
