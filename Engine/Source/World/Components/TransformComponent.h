#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // TransformComponent — WHERE an entity is, RELATIVE TO ITS PARENT (§HR). The one position in
    //   the engine: World::CreateEntity emplaces it beside EntityMeta, so every entity has one and
    //   Each<TransformComponent> is a complete view. That guarantee is what the editor's picking
    //   and its icon for an entity with nothing to draw both stand on — an entity with no anchor
    //   could not be clicked at all.
    //
    //   LOCAL, not world. A root's local is its world, which is every entity there was before
    //   parenting; a child's is composed up the chain by EntityHierarchy::WorldTransform, and that
    //   is what every reader (renderer, picking, physics, camera) asks for. Nothing reads Position
    //   as a world coordinate any more except a writer that knows it holds a root.
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

    // =============================================================================
    // Composition — the pure half, and therefore the testable one (ResolveDisplayPose's idiom).
    //
    //   TRS only: position is rotated and scaled by the parent then translated, rotation adds,
    //   scale multiplies. A child rotated under a NON-UNIFORMLY scaled parent is a shear, which
    //   three fields cannot hold — Unity's `lossyScale` limit, accepted here for the same reason
    //   the scale tool already forces Local space.
    // =============================================================================

    /** InLocal placed under InParent (both in InParent's parent's frame). */
    inline TransformComponent Compose(const TransformComponent& InParent, const TransformComponent& InLocal) noexcept
    {
        const float lRad = Maths::DegreesToRadians(InParent.Rotation);
        const float lCos = Maths::Cos(lRad);
        const float lSin = Maths::Sin(lRad);

        const Vector2F lScaled = InLocal.Position * InParent.Scale;

        TransformComponent lWorld;
        lWorld.Position = InParent.Position + Vector2F{ lCos * lScaled.x - lSin * lScaled.y,
                                                        lSin * lScaled.x + lCos * lScaled.y };
        lWorld.Rotation = InParent.Rotation + InLocal.Rotation;
        lWorld.Scale    = InParent.Scale * InLocal.Scale;

        return lWorld;
    }

    /**
     * The local that Compose(InParent, local) == InWorld — Compose's inverse.
     * A zero parent scale axis has no inverse; that axis is passed through unscaled rather
     * than divided into infinity.
     */
    inline TransformComponent ToLocal(const TransformComponent& InParent, const TransformComponent& InWorld) noexcept
    {
        const float lRad = Maths::DegreesToRadians(InParent.Rotation);
        const float lCos = Maths::Cos(lRad);
        const float lSin = Maths::Sin(lRad);

        const Vector2F lDelta = InWorld.Position - InParent.Position;
        const Vector2F lUnrotated{  lCos * lDelta.x + lSin * lDelta.y,
                                   -lSin * lDelta.x + lCos * lDelta.y };

        const auto lSafeDivide = [](const float InValue, const float InBy) noexcept
        {
            return InBy != 0.f ? InValue / InBy : InValue;
        };

        TransformComponent lLocal;
        lLocal.Position = { lSafeDivide(lUnrotated.x, InParent.Scale.x), lSafeDivide(lUnrotated.y, InParent.Scale.y) };
        lLocal.Rotation = InWorld.Rotation - InParent.Rotation;
        lLocal.Scale    = { lSafeDivide(InWorld.Scale.x, InParent.Scale.x), lSafeDivide(InWorld.Scale.y, InParent.Scale.y) };

        return lLocal;
    }
}
