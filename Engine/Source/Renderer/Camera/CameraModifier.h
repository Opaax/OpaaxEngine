#pragma once

#include "Core/OpaaxTypes.h"
#include "Object/Object.h"
#include "Renderer/CameraView.h"
#include "World/Entity/EntityTypes.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// A camera modifier changes the view a frame draws, after the director's blend and before the guard:
// a shake, a recoil, a lean toward the cursor. It works on game time, never on frames, so it is the
// same at any frame rate and holds when the world is paused, and it moves no entity, so no camera's
// rhythm turns mixed:
//
//   class RecoilModifier final : public Opaax::CameraModifier
//   {
//       OPAAX_OBJECT(RecoilModifier, Opaax::CameraModifier)
//
//   public:
//       double FiredAt = -1.0;   // the world's stepped time when the shot was fired
//
//       void Modify(const Opaax::CameraModifierContext& InContext, Opaax::CameraView& InOutView) const override
//       {
//           const double lSince = InContext.GameTime - FiredAt;
//           if (FiredAt >= 0.0 && lSince >= 0.0 && lSince < 0.2)
//           {
//               InOutView.Position.y -= 8.f * static_cast<float>(1.0 - lSince / 0.2);
//           }
//       }
//   };
//
//   // Given to the world's director, which owns it:
//   RecoilModifier& lRecoil = lDirector->AddModifier<RecoilModifier>();
//   lRecoil.FiredAt = GetWorld().GetSteppedTime();
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    class World;

    // =============================================================================
    // CameraModifierContext — what a modifier knows of the frame it changes.
    // =============================================================================
    struct CameraModifierContext
    {
        World& OwningWorld;

        /** The game time the frame shows (CameraDirector::GameTimeAt): a modifier is a function of it. */
        double GameTime = 0.0;

        /** The camera framing the world (blended to, when a blend is under way). */
        EntityID Live = ENTITY_NONE;
    };

    // =============================================================================
    // CameraModifier — changes the view a frame draws, after the blend (CameraDirector). Modifiers run
    //   by Priority, the lowest first, each on the view the ones before it left.
    // =============================================================================
    class CameraModifier : public Object
    {
        OPAAX_ABSTRACT_OBJECT(CameraModifier, Object)

    public:
        /** Lower runs first. */
        Int32 Priority = 0;

        /** Changes InOutView for a frame drawn at InContext.GameTime. */
        virtual void Modify(const CameraModifierContext& InContext, CameraView& InOutView) const = 0;
    };
}
