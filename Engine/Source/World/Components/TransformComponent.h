#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // TransformComponent — WHERE an entity is. The one position in the engine: World::CreateEntity
    //   emplaces it beside EntityMeta, so every entity has one and Each<TransformComponent> is a
    //   complete view. That guarantee is what the editor's picking and its icon for an entity with
    //   nothing to draw both stand on — an entity with no anchor could not be clicked at all.
    //
    //   It replaced the Position that used to sit on SpriteComponent, DummyComponent and
    //   CameraComponent separately, where one entity could carry three of them and they could
    //   disagree. Size stays on the components: an extent is what a thing IS, not where it is.
    //
    //   Rotation is DEGREES — what an author types into the Inspector. Renderer2D takes radians, so
    //   the draw call converts (Maths::DegreesToRadians).
    //
    //   Scale arrived with ③'s gizmo and NOT before, which is X5's rule: it is a MULTIPLIER on the
    //   Size its components carry, and it landed in the same change as its three readers — both
    //   RendererManager passes and EntityQuery::TryGetBounds. A gizmo was the reader that made it
    //   real; ImGuizmo forced the timing, since its decompose always answers a scale and discarding
    //   one it had authored would have been a silent lie.
    // =============================================================================
    struct TransformComponent
    {
        Vector2F Position = { 0.f, 0.f };

        /** Degrees, counter-clockwise. */
        float    Rotation = 0.f;

        /** A MULTIPLIER on the component's own Size, not an extent — 1 is unscaled. */
        Vector2F Scale    = { 1.f, 1.f };

        // Satisfies CComponent. _WITH_DEFAULT is the required variant, not a preference: the plain
        // macro reads every field with at(), which THROWS on a missing key — and it is exactly what
        // lets Scale be added without touching a single `.opaaxmap` already on disk.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TransformComponent, Position, Rotation, Scale)

        OPAAX_PROPERTIES(TransformComponent,
                         OPAAX_PROP(Position),
                         OPAAX_PROP(Rotation).SetRange(-360.f, 360.f),
                         OPAAX_PROP(Scale))
    };
}
