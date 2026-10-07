#pragma once

#include <algorithm>

#include "Core/String/OpaaxStringJson.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Systems/WorldContext.h"

namespace TestWorld
{
    // =============================================================================
    // InputProbe — counts Space presses by polling the keyboard, and the Fire action (F, from
    //   Input/Test.opaaxinputmap, which it adds) through BindAction.
    // =============================================================================
    class InputProbe : public Opaax::Behaviour
    {
    public:
        Opaax::Int32 SpacePresses = 0;
        Opaax::Int32 Fires        = 0;

        OPAAX_PROPERTIES(InputProbe, OPAAX_PROP(SpacePresses), OPAAX_PROP(Fires))

        void OnStart() override
        {
            if (Opaax::InputMappingSubsystem* lActions = GetContext().Actions)
            {
                lActions->AddContextAsset(OPAAX_ID("Test"), Opaax::OpaaxString("Input/Test.opaaxinputmap"));
            }
            BindAction<&InputProbe::OnFire>(OPAAX_ID("Fire"), Opaax::EInputTrigger::Started);
        }

        void OnUpdate(float) override
        {
            if (WasKeyPressed(Opaax::EKeyCode::Space))
            {
                ++SpacePresses;
            }
        }

        void OnFire(const Opaax::InputActionValue&) { ++Fires; }
    };

    // =============================================================================
    // LaunchProbe — throws its dynamic body up at LaunchSpeed and records how high it went.
    // =============================================================================
    class LaunchProbe : public Opaax::Behaviour
    {
    public:
        float LaunchSpeed = 600.f;

        float StartY = 0.f;
        float PeakY  = 0.f;
        float Mass   = 0.f;

        OPAAX_PROPERTIES(LaunchProbe, OPAAX_PROP(LaunchSpeed), OPAAX_PROP(StartY), OPAAX_PROP(PeakY), OPAAX_PROP(Mass))

        void OnStart() override
        {
            StartY = GetTransform().Position.y;
            PeakY  = StartY;
            Mass   = GetMass();
            SetVelocity(Opaax::Vector2F{ 0.f, LaunchSpeed });
        }

        void OnUpdate(float) override { PeakY = std::max(PeakY, GetTransform().Position.y); }
    };

    // =============================================================================
    // AudioProbe — plays Clip once on start; bPlayed when the mixer took it.
    // =============================================================================
    class AudioProbe : public Opaax::Behaviour
    {
    public:
        Opaax::OpaaxString Clip = "Audio/Beep.wav";

        bool bPlayed = false;

        OPAAX_PROPERTIES(AudioProbe, OPAAX_PROP(Clip), OPAAX_PROP(bPlayed))

        void OnStart() override { bPlayed = PlaySound(Clip, 0.5f).IsValid(); }
    };
}
