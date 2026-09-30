#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // TransformComponent — an entity's position, rotation and scale, relative to its parent.
    //   Every entity has one (World::CreateEntity adds it).
    //   Use EntityHierarchy::WorldTransform for the world pose.
    //   Rotation is in degrees. Scale multiplies the Size of the other components.
    // =============================================================================
    struct TransformComponent
    {
        Vector2F Position = { 0.f, 0.f };

        /** Degrees, counter-clockwise. */
        float    Rotation = 0.f;

        /** Multiplies the component's Size. 1 = unscaled. */
        Vector2F Scale    = { 1.f, 1.f };

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TransformComponent, Position, Rotation, Scale)

        OPAAX_PROPERTIES(TransformComponent,
                         OPAAX_PROP(Position),
                         OPAAX_PROP(Rotation).SetRange(-360.f, 360.f),
                         OPAAX_PROP(Scale))
    };

    // =============================================================================
    // Composition (pure functions)
    //   Position is rotated and scaled by the parent then offset; rotation adds; scale multiplies.
    //   A rotated child of a non-uniformly scaled parent cannot be represented (like Unity's lossyScale).
    // =============================================================================

    /** InLocal placed under InParent. */
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
     * Inverse of Compose: the local such that Compose(InParent, local) == InWorld.
     * A zero parent scale axis is passed through unscaled.
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
