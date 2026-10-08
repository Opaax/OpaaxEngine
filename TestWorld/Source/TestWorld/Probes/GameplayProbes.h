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
    // ControlsProbe — reads the Move action (an Axis2D from WASD, Input/Controls.opaaxinputmap, which
    //   it adds) every way a behaviour can: its value, polled phases and bound handlers for each
    //   trigger; the D key raw; and a Hold on the Charge action (Space).
    // =============================================================================
    class ControlsProbe : public Opaax::Behaviour
    {
    public:
        // This frame's state.
        float MoveX     = 0.f;
        float MoveY     = 0.f;
        bool  bMoving   = false;   // IsActionActive
        bool  bDownHeld = false;   // IsKeyDown(D)

        // Counts.
        Opaax::Int32 BoundStarts       = 0;
        Opaax::Int32 BoundCompletions  = 0;
        Opaax::Int32 BoundFrames       = 0;   // Triggered: every frame the action is on
        Opaax::Int32 PolledStarts      = 0;
        Opaax::Int32 PolledCompletions = 0;
        Opaax::Int32 DReleases         = 0;   // WasKeyReleased(D)
        Opaax::Int32 Charges           = 0;   // Hold

        OPAAX_PROPERTIES(ControlsProbe, OPAAX_PROP(MoveX), OPAAX_PROP(MoveY), OPAAX_PROP(bMoving), OPAAX_PROP(bDownHeld),
                         OPAAX_PROP(BoundStarts), OPAAX_PROP(BoundCompletions), OPAAX_PROP(BoundFrames),
                         OPAAX_PROP(PolledStarts), OPAAX_PROP(PolledCompletions), OPAAX_PROP(DReleases),
                         OPAAX_PROP(Charges))

        void OnStart() override
        {
            if (Opaax::InputMappingSubsystem* lActions = GetContext().Actions)
            {
                lActions->AddContextAsset(OPAAX_ID("Controls"), Opaax::OpaaxString("Input/Controls.opaaxinputmap"));
            }
            BindAction<&ControlsProbe::OnMoveStarted>(OPAAX_ID("Move"), Opaax::EInputTrigger::Started);
            BindAction<&ControlsProbe::OnMoveCompleted>(OPAAX_ID("Move"), Opaax::EInputTrigger::Completed);
            BindAction<&ControlsProbe::OnMoveFrame>(OPAAX_ID("Move"), Opaax::EInputTrigger::Triggered);
            BindAction<&ControlsProbe::OnCharge>(OPAAX_ID("Charge"), Opaax::EInputTrigger::Hold);
        }

        void OnUpdate(float) override
        {
            const Opaax::Vector2F lMove = GetAction(OPAAX_ID("Move")).AsAxis2D();
            MoveX     = lMove.x;
            MoveY     = lMove.y;
            bMoving   = IsActionActive(OPAAX_ID("Move"));
            bDownHeld = IsKeyDown(Opaax::EKeyCode::D);

            if (WasActionStarted(OPAAX_ID("Move")))   { ++PolledStarts; }
            if (WasActionCompleted(OPAAX_ID("Move"))) { ++PolledCompletions; }
            if (WasKeyReleased(Opaax::EKeyCode::D))   { ++DReleases; }
        }

        void OnMoveStarted(const Opaax::InputActionValue&)   { ++BoundStarts; }
        void OnMoveCompleted(const Opaax::InputActionValue&) { ++BoundCompletions; }
        void OnMoveFrame(const Opaax::InputActionValue&)     { ++BoundFrames; }
        void OnCharge(const Opaax::InputActionValue&)        { ++Charges; }
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
