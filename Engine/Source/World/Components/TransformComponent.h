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
    //   the draw call converts (Maths::DegreesToRadians). Scale is deliberately absent until
    //   something reads it (X5).
    // =============================================================================
    struct TransformComponent
    {
        Vector2F Position = { 0.f, 0.f };

        /** Degrees, counter-clockwise. */
        float    Rotation = 0.f;

        // Satisfies CComponent. _WITH_DEFAULT is the required variant, not a preference: the plain
        // macro reads every field with at(), which THROWS on a missing key.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TransformComponent, Position, Rotation)

        OPAAX_PROPERTIES(TransformComponent,
                         OPAAX_PROP(Position),
                         OPAAX_PROP(Rotation).SetRange(-360.f, 360.f))
    };
}
