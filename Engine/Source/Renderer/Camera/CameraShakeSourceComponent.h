#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    class CameraShake;

    // =============================================================================
    // CameraShakeSourceComponent — a shake coming from an entity (an explosion, a machine): weaker the
    //   farther the view's centre is from it. In a Play world it plays when the entity starts if
    //   bPlayOnStart, or when gameplay asks (CameraShakeSubsystem::PlaySource), and stops with the entity.
    //   With bUseTrauma it plays for as long as the entity lives, at its trauma squared: gameplay raises
    //   the trauma (CameraShakeSubsystem::AddTrauma) and it falls by TraumaDecay a second. Give such a
    //   source an endless shake (Duration 0).
    // =============================================================================
    struct CameraShakeSourceComponent
    {
        TAssetRef<CameraShake> Shake;

        /** Times the shake's amplitudes. */
        float Scale = 1.f;

        /** Full strength within InnerRadius of the view's centre, none from OuterRadius. Outer 0: everywhere. */
        float InnerRadius = 0.f;
        float OuterRadius = 0.f;

        bool bPlayOnStart = true;

        /** Picks the shake's noise or phase: sources with the same seed shake alike. */
        Uint32 Seed = 0;

        /** Shakes at its trauma squared, which AddTrauma raises: plays for as long as the entity lives. */
        bool bUseTrauma = false;

        /** Trauma lost a second. */
        float TraumaDecay = 1.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraShakeSourceComponent, Shake, Scale, InnerRadius, OuterRadius,
                                                    bPlayOnStart, Seed, bUseTrauma, TraumaDecay)

        OPAAX_PROPERTIES(CameraShakeSourceComponent,
                         OPAAX_PROP(Shake).SetTooltip("The camera shake it plays."),
                         OPAAX_PROP(Scale).SetRange(0.f, 10.f).SetTooltip("Times the shake's amplitudes."),
                         OPAAX_PROP(InnerRadius).SetRange(0.f, 10000.f).SetTooltip("Full strength within this distance "
                                                                                   "of the view's centre."),
                         OPAAX_PROP(OuterRadius).SetRange(0.f, 10000.f).SetTooltip("None from this distance.\n"
                                                                                   "0: full strength everywhere."),
                         OPAAX_PROP(bPlayOnStart).SetTooltip("Plays when the entity starts in a Play world."),
                         OPAAX_PROP(Seed).SetTooltip("Picks the shake's noise: sources with the same seed shake alike."),
                         OPAAX_PROP(bUseTrauma).SetTooltip("Shakes at its trauma squared, raised by gameplay "
                                                           "(AddTrauma) and falling by TraumaDecay a second.\n"
                                                           "Plays for as long as the entity lives: give it an "
                                                           "endless shake (Duration 0)."),
                         OPAAX_PROP(TraumaDecay).SetRange(0.f, 10.f).SetTooltip("Trauma lost a second."))
    };
}
