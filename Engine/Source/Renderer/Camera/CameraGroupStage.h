#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Object/ObjectMacros.h"
#include "Renderer/Camera/CameraStage.h"
#include "World/Entity/Entity.h"

namespace Opaax
{
    // =============================================================================
    // Group framing (pure function)
    // =============================================================================
    /** Where a view framing a group is centred, and its OrthoSize. */
    struct CameraGroupFrame
    {
        Vector2F Center    = { 0.f, 0.f };
        float    OrthoSize = 0.f;
    };

    /**
     * The upright view that frames InBounds with InPadding world units each side on a screen InAspect
     * wide for 1 tall: centred on them, its OrthoSize the smallest that fits them both ways, kept in
     * [InMinSize, InMaxSize].
     */
    inline CameraGroupFrame FrameCameraGroup(const Bounds2D& InBounds, const float InPadding, const float InAspect,
                                             const float InMinSize, const float InMaxSize)
    {
        const float lHalfWidth  = InBounds.HalfExtent.x + InPadding;
        const float lHalfHeight = InBounds.HalfExtent.y + InPadding;
        const float lFits       = Maths::Max(lHalfHeight, lHalfWidth / Maths::Max(InAspect, 1.0e-4f));

        return CameraGroupFrame{ InBounds.Center, Maths::Clamp(lFits, InMinSize, Maths::Max(InMinSize, InMaxSize)) };
    }

    // =============================================================================
    // CameraGroupStage — a Body stage that frames several entities (FrameCameraGroup): centred on
    //   their bounds and zoomed to fit them with Padding at the screen's aspect (World::GetViewAspect),
    //   between MinSize and MaxSize, eased over Damping. It places and zooms the camera: on a camera
    //   with another Body stage, the one run last wins.
    // =============================================================================
    class CameraGroupStage final : public CameraStage
    {
        OPAAX_OBJECT(CameraGroupStage, CameraStage)

    public:
        /** The entities framed, by name: each found again once it is gone; those not found are left out. */
        TDynArray<OpaaxString> Targets;

        float Padding = 100.f;
        float MinSize = 200.f;
        float MaxSize = 1000.f;
        float Damping = 0.f;

        OPAAX_PROPERTIES(CameraGroupStage,
                         OPAAX_PROP(Targets).SetTooltip("The entities it frames, by name."),
                         OPAAX_PROP(Padding).SetRange(0.f, 10000.f).SetTooltip("World units kept around them, each "
                                                                               "side."),
                         OPAAX_PROP(MinSize).SetRange(1.f, 100000.f).SetTooltip("The closest it zooms in: the "
                                                                                "smallest OrthoSize."),
                         OPAAX_PROP(MaxSize).SetRange(1.f, 100000.f).SetTooltip("The furthest it zooms out: the "
                                                                                "largest OrthoSize."),
                         OPAAX_PROP(Damping).SetRange(0.f, 10.f).SetTooltip("Seconds it takes to settle on the "
                                                                            "group.\n0: on it at once."))

        /** The bounds of the targets found on the last step, or none. */
        const TOptional<Bounds2D>& GetTargetBounds() const noexcept { return m_TargetBounds; }

        //~Begin CameraStage interface
        ECameraStage GetStage() const override { return ECameraStage::Body; }
        void         Step(CameraRigPose& InOutPose, float InDeltaTime) override;
        //~End CameraStage interface

    private:
        TDynArray<Entity>   m_Found;
        TOptional<Bounds2D> m_TargetBounds;
        Vector2F            m_Velocity     = { 0.f, 0.f };
        float               m_SizeVelocity = 0.f;
    };
}
