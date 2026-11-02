#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Object/ObjectMacros.h"
#include "Renderer/Camera/CameraStage.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/Entity.h"

namespace Opaax
{
    // =============================================================================
    // Confine (pure function)
    // =============================================================================
    /**
     * InPosition moved so a view reaching InViewHalfExtent each side of it stays inside InBounds. On an
     * axis where the view is wider than the bounds it is centred on them; an axis whose bounds have no
     * extent is left free.
     */
    inline Vector2F ConfineCameraView(const Vector2F InPosition, const Vector2F InViewHalfExtent,
                                      const Bounds2D& InBounds)
    {
        Vector2F lPosition = InPosition;
        for (Int32 lAxis = 0; lAxis < 2; ++lAxis)
        {
            if (InBounds.HalfExtent[lAxis] <= 0.f)
            {
                continue;
            }

            const float lRoom = InBounds.HalfExtent[lAxis] - InViewHalfExtent[lAxis];
            lPosition[lAxis]  = lRoom <= 0.f ? InBounds.Center[lAxis]
                                             : Maths::Clamp(InPosition[lAxis], InBounds.Center[lAxis] - lRoom,
                                                            InBounds.Center[lAxis] + lRoom);
        }
        return lPosition;
    }

    /** A confiner's rectangle: InCenter and InSize in the world, or on InPlace (its position and scale). */
    inline Bounds2D CameraConfinerBounds(const Vector2F InCenter, const Vector2F InSize, const TransformComponent* InPlace)
    {
        return InPlace != nullptr
                   ? Bounds2D::FromCenterSize(InPlace->Position + InCenter * InPlace->Scale, InSize * InPlace->Scale)
                   : Bounds2D::FromCenterSize(InCenter, InSize);
    }

    // =============================================================================
    // CameraConfinerStage — a Constraint stage that keeps the whole view inside a rectangle
    //   (ConfineCameraView): the view's size, the screen's aspect (World::GetViewAspect) and a
    //   rolled view's corners counted. Placed on the entity named BoundsEntity when one is (its
    //   position and scale), so the bounds move with it.
    // =============================================================================
    class CameraConfinerStage final : public CameraStage
    {
        OPAAX_OBJECT(CameraConfinerStage, CameraStage)

    public:
        /** The rectangle's centre and size: in the world, or on BoundsEntity. A size of 0 leaves that axis free. */
        Vector2F Center = { 0.f, 0.f };
        Vector2F Size   = { 0.f, 0.f };

        /** The entity it is placed on, by name (found again once it is gone); empty: in the world. */
        OpaaxString BoundsEntity;

        OPAAX_PROPERTIES(CameraConfinerStage,
                         OPAAX_PROP(Center).SetTooltip("The rectangle's centre: in the world, or from BoundsEntity."),
                         OPAAX_PROP(Size).SetTooltip("The rectangle's width and height, the view kept inside.\n"
                                                     "0: that axis is free."),
                         OPAAX_PROP(BoundsEntity).SetTooltip("The entity the rectangle is placed on, by name: it "
                                                             "moves and scales with it.\nEmpty: in the world."))

        /** The bounds now, or none: BoundsEntity named and not found. */
        TOptional<Bounds2D> GetBounds();

        //~Begin CameraStage interface
        ECameraStage GetStage() const override { return ECameraStage::Constraint; }
        void         Step(CameraRigPose& InOutPose, float InDeltaTime) override;
        //~End CameraStage interface

    private:
        Entity m_BoundsEntity;
    };
}
