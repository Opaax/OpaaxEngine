#pragma once

#include "Core/Algo/Algo.h"

#include "Audio/AudioGlobalBank.h"
#include "Audio/AudioManager.h"
#include "Audio/AudioSubsystem.h"
#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxStringJson.h"
#include "Input/InputManager.h"
#include "Input/Mapping/InputMappingSubsystem.h"
#include "World/Behaviour/Behaviour.h"
#include "World/Systems/WorldContext.h"

namespace TestWorld
{
    // =============================================================================
    // InputProbe — counts Space presses by polling the keyboard, and the Fire action (F, from
    //   Input/Test.opaaxasset, which it adds) through BindAction.
    // =============================================================================
    class InputProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(InputProbe, Opaax::Behaviour)

    public:
        Opaax::Int32 SpacePresses = 0;
        Opaax::Int32 Fires        = 0;

        OPAAX_PROPERTIES(InputProbe, OPAAX_PROP(SpacePresses), OPAAX_PROP(Fires))

        void OnStart() override
        {
            if (Opaax::InputMappingSubsystem* lActions = GetContext().Actions)
            {
                lActions->AddContextAsset(OPAAX_ID("Test"), Opaax::OpaaxString("Input/Test.opaaxasset"));
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
    // ControlsProbe — reads the Move action (an Axis2D from WASD, Input/Controls.opaaxasset, which
    //   it adds) every way a behaviour can: its value, polled phases and bound handlers for each
    //   trigger; the D key raw; and a Hold on the Charge action (Space).
    // =============================================================================
    class ControlsProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(ControlsProbe, Opaax::Behaviour)

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
                lActions->AddContextAsset(OPAAX_ID("Controls"), Opaax::OpaaxString("Input/Controls.opaaxasset"));
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
    // PointerProbe — the pointer as the game reads it: its position in window pixels (GetInput), and
    //   the left button as a key.
    // =============================================================================
    class PointerProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(PointerProbe, Opaax::Behaviour)

    public:
        float        X           = 0.f;
        float        Y           = 0.f;
        bool         bLeftDown   = false;
        Opaax::Int32 LeftPresses = 0;

        OPAAX_PROPERTIES(PointerProbe, OPAAX_PROP(X), OPAAX_PROP(Y), OPAAX_PROP(bLeftDown), OPAAX_PROP(LeftPresses))

        void OnUpdate(float) override
        {
            const Opaax::Vector2F lPointer = GetInput().GetMousePosition();
            X         = lPointer.x;
            Y         = lPointer.y;
            bLeftDown = IsKeyDown(Opaax::EKeyCode::Mouse_Left);

            if (WasKeyPressed(Opaax::EKeyCode::Mouse_Left)) { ++LeftPresses; }
        }
    };

    // =============================================================================
    // LaunchProbe — throws its dynamic body up at LaunchSpeed and records how high it went.
    // =============================================================================
    class LaunchProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(LaunchProbe, Opaax::Behaviour)

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

        void OnUpdate(float) override { PeakY = Opaax::Maths::Max(PeakY, GetTransform().Position.y); }
    };

    // =============================================================================
    // AudioProbe — plays Clip once on start; bPlayed when the mixer took it.
    // =============================================================================
    class AudioProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(AudioProbe, Opaax::Behaviour)

    public:
        Opaax::OpaaxString Clip = "Audio/Beep.wav";

        bool bPlayed = false;

        OPAAX_PROPERTIES(AudioProbe, OPAAX_PROP(Clip), OPAAX_PROP(bPlayed))

        void OnStart() override { bPlayed = PlaySound(Clip, 0.5f).IsValid(); }
    };

    // =============================================================================
    // AudioControlProbe — plays Clip at a place in the world and stops it, then plays its entity's
    //   AudioSource and stops it, checking each step with the world's audio.
    // =============================================================================
    class AudioControlProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(AudioControlProbe, Opaax::Behaviour)

    public:
        Opaax::OpaaxString Clip = "Audio/Beep.wav";

        bool         bPlayedAt      = false;
        Opaax::Int32 StoppedSounds  = 0;   // playing sounds before StopSound, minus after
        bool         bSourcePlaying = false;
        bool         bSourceStopped = false;

        OPAAX_PROPERTIES(AudioControlProbe, OPAAX_PROP(Clip), OPAAX_PROP(bPlayedAt), OPAAX_PROP(StoppedSounds),
                         OPAAX_PROP(bSourcePlaying), OPAAX_PROP(bSourceStopped))

        void OnStart() override
        {
            m_Placed  = PlaySoundAt(Clip, Opaax::Vector2F{ 400.f, 0.f }, 0.5f);
            bPlayedAt = m_Placed.IsValid();

            SetTimer<&AudioControlProbe::StopPlaced>(0.05f);
            SetTimer<&AudioControlProbe::StartSource>(0.1f);
            SetTimer<&AudioControlProbe::StopSource>(0.2f);
        }

    private:
        void StopPlaced()
        {
            const Opaax::Uint64 lBefore = PlayingCount();
            StopSound(m_Placed);
            StoppedSounds = static_cast<Opaax::Int32>(lBefore - PlayingCount());
        }

        void StartSource()
        {
            PlayAudioSource();
            bSourcePlaying = IsSourcePlaying();
        }

        void StopSource()
        {
            StopAudioSource();
            bSourceStopped = !IsSourcePlaying();
        }

        Opaax::Uint64 PlayingCount() const
        {
            const Opaax::AudioSubsystem* lAudio = GetSubsystem<Opaax::AudioSubsystem>();
            return (lAudio != nullptr) ? lAudio->GetPlayingCount() : 0;
        }

        bool IsSourcePlaying() const
        {
            const Opaax::AudioSubsystem* lAudio = GetSubsystem<Opaax::AudioSubsystem>();
            return lAudio != nullptr && lAudio->IsSourcePlaying(GetEntity().GetHandle());
        }

        Opaax::SoundHandle m_Placed;
    };

    // =============================================================================
    // StepsProbe — plays the Steps clip definition, then draws 50 more footsteps from it as the audio
    //   does: none is the one just before, and every clip is heard.
    // =============================================================================
    class StepsProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(StepsProbe, Opaax::Behaviour)

    public:
        Opaax::TAssetRef<Opaax::AudioClipDef> Steps;

        bool         bPlayed     = false;
        bool         bNeverTwice = false;
        Opaax::Int32 Distinct    = 0;

        OPAAX_PROPERTIES(StepsProbe, OPAAX_PROP(Steps), OPAAX_PROP(bPlayed), OPAAX_PROP(bNeverTwice), OPAAX_PROP(Distinct))

        void OnStart() override
        {
            bPlayed = PlaySound(Steps.Path).IsValid();

            const Opaax::TAssetHandle<Opaax::AudioClipDef> lSteps = Opaax::LoadAsset(GetContext().Resources,
                                                                                     GetContext().Paths, Steps);
            const Opaax::AudioSubsystem* lAudio = GetSubsystem<Opaax::AudioSubsystem>();
            if (lSteps.Get() == nullptr || lAudio == nullptr || lAudio->GetAudio() == nullptr)
            {
                return;
            }

            Opaax::TDynArray<bool> lHeard(lSteps.Get()->Clips.size(), false);
            Opaax::Int32           lLast = -1;
            bNeverTwice = true;

            for (int lStep = 0; lStep < 50; ++lStep)
            {
                Opaax::PlaySoundParams lParams;
                const Opaax::Int32 lIndex = lAudio->GetAudio()->SelectClip(*lSteps.Get(), OPAAX_ID("StepsProbe"), lParams);
                bNeverTwice = bNeverTwice && lIndex >= 0 && lIndex != lLast;
                lLast       = lIndex;

                if (lIndex >= 0)
                {
                    lHeard[lIndex] = true;
                }
            }

            Distinct = static_cast<Opaax::Int32>(Opaax::Algo::Count(lHeard, true));
        }
    };

    // =============================================================================
    // BankProbe — how many sounds the game's global audio bank holds (Audio.GlobalBank).
    // =============================================================================
    class BankProbe : public Opaax::Behaviour
    {
        OPAAX_OBJECT(BankProbe, Opaax::Behaviour)

    public:
        Opaax::Int32 GlobalSounds = 0;

        OPAAX_PROPERTIES(BankProbe, OPAAX_PROP(GlobalSounds))

        void OnStart() override
        {
            if (const Opaax::AudioGlobalBank* lGlobal = GetGameSubsystem<Opaax::AudioGlobalBank>())
            {
                GlobalSounds = static_cast<Opaax::Int32>(lGlobal->GetHold().GetClipCount());
            }
        }
    };
}
