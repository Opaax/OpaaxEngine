#pragma once

#include <nlohmann/json.hpp>

#include "Audio/AudioBackend.h"
#include "Audio/AudioBank.h"
#include "Audio/AudioBusSettings.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Platform/Window/Window.h"
#include "Physics/PhysicsBackend.h"
#include "Physics/PhysicsTypes.h"
#include "RHI/RHIBackend.h"

namespace Opaax
{
    // =============================================================================
    // EngineConfigData — the engine section of <ProjectRoot>/Configs/Engine.config.
    //   Nested structs are nested JSON objects. A missing key keeps its default.
    //   NeedRestart is set on a group when all its fields are read at startup,
    //   otherwise on the individual fields.
    // =============================================================================

    struct WindowSettings
    {
        /** The most MaxFrameRate can be set to; 0 is no cap. */
        static constexpr Uint32 MAX_FRAME_RATE = 1000;

        OpaaxString Title  = OpaaxString("Opaax Engine");
        Uint32      Width  = 1280;
        Uint32      Height = 720;

        /** Windowed at Width x Height, borderless over the screen, or fullscreen at the screen's mode closest to it. */
        EWindowMode Mode = EWindowMode::Windowed;

        /** Waits for the screen's refresh before showing a frame: no tearing, one frame a refresh. */
        bool bVSync = true;

        /** The most frames a second; 0 for no cap. */
        Uint32 MaxFrameRate = 0;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WindowSettings, Title, Width, Height, Mode, bVSync, MaxFrameRate)

        // The title is set when the window is made. A game's window follows the mode and size at once;
        // the editor's keeps its own (the size the person gave it).
        OPAAX_PROPERTIES(WindowSettings,
                         OPAAX_PROP(Title).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Width).SetRange(320.f, 7680.f)
                             .SetTooltip("The window's width when windowed; in fullscreen, the screen's mode\n"
                                         "closest to Width x Height. A game's window follows it at once,\n"
                                         "the editor's keeps its own size."),
                         OPAAX_PROP(Height).SetRange(240.f, 4320.f)
                             .SetTooltip("The window's height when windowed; in fullscreen, the screen's mode\n"
                                         "closest to Width x Height. A game's window follows it at once,\n"
                                         "the editor's keeps its own size."),
                         OPAAX_PROP(Mode)
                             .SetTooltip("Windowed, Borderless (over the whole screen, at its own resolution)\n"
                                         "or Fullscreen (at the screen's mode closest to Width x Height).\n"
                                         "A game's window follows it at once, the editor's keeps its own."),
                         OPAAX_PROP(bVSync).SetTooltip("Wait for the screen's refresh: no tearing.\n"
                                                       "Off in a run paced by --frame-pacing."),
                         OPAAX_PROP(MaxFrameRate).SetRange(0.f, static_cast<float>(MAX_FRAME_RATE))
                             .SetTooltip("The most frames a second; 0 for no cap.\n"
                                         "With vsync, a cap that divides the refresh rate (120 on 240 Hz)\n"
                                         "shows each frame for whole refreshes. Off in a run paced by --frame-pacing."))
    };

    struct TimeSettings
    {
        /** The range FixedRate is kept in, steps a second. */
        static constexpr float MIN_FIXED_RATE = 10.f;
        static constexpr float MAX_FIXED_RATE = 1000.f;

        /** Fixed steps a second: physics, movement and every OnFixedUpdate run at this rate. */
        float FixedRate = 60.f;

        /** With vsync, each frame counts as whole refreshes of the screen: the timer's noise does not shake motion. */
        bool bSnapToRefresh = true;

        /** The most fixed steps a frame runs: a longer frame drops the rest and the game slows for it. */
        Uint32 MaxStepsPerFrame = 8;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TimeSettings, FixedRate, bSnapToRefresh, MaxStepsPerFrame)

        OPAAX_PROPERTIES(TimeSettings,
                         OPAAX_PROP(FixedRate).SetRange(MIN_FIXED_RATE, MAX_FIXED_RATE)
                             .SetTooltip("Fixed steps a second: physics, movement, OnFixedUpdate.\n"
                                         "Moving things are drawn between steps, smooth at any screen rate."),
                         OPAAX_PROP(bSnapToRefresh)
                             .SetTooltip("With vsync, count each frame as whole refreshes of the screen:\n"
                                         "the timer's noise no longer shakes moving things.\n"
                                         "Off for a variable-refresh screen (G-Sync, FreeSync)."),
                         OPAAX_PROP(MaxStepsPerFrame).SetRange(1.f, 64.f)
                             .SetTooltip("The most fixed steps one frame runs.\n"
                                         "A longer frame (a stall) drops the rest: the game slows for it\n"
                                         "instead of slow steps making every frame longer."))
    };

    struct RenderSettings
    {
        // Requested backend. Unsupported backends (e.g. Vulkan without SDK) fall back, with a warning.
        EBackend Backend = EBackend::OpenGL;

        /**
         * Interpolate fixed-step poses for display, so motion is smooth on fast screens.
         */
        bool bInterpolation = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings, Backend, bInterpolation)

        OPAAX_PROPERTIES(RenderSettings,
                         OPAAX_PROP(Backend).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(bInterpolation)
                             .SetTooltip("Smooth motion between fixed steps.\n"
                                         "Display only — gameplay always reads the raw pose.\n"
                                         "Subtle at 60Hz with vsync, pronounced above it."))
    };

    struct WorldBoundsSettings
    {
        /**
         * Off by default: a body falling forever is better than one silently destroyed.
         */
        bool bEnabled = false;

        Vector2F Min = { -100000.f, -100000.f };
        Vector2F Max = {  100000.f,  100000.f };

        /**
         * What the engine does when a body leaves the bounds. The event always fires;
         * EventAndDestroy also destroys the entity.
         */
        EWorldBoundsResponse Response = EWorldBoundsResponse::EventAndDestroy;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WorldBoundsSettings, bEnabled, Min, Max, Response)

        OPAAX_PROPERTIES(WorldBoundsSettings,
                         OPAAX_PROP(bEnabled).SetTooltip("Reap dynamic bodies that leave the box below.\n"
                                                         "Off by default: an endless scroller has no bounds."),
                         OPAAX_PROP(Min),
                         OPAAX_PROP(Max),
                         OPAAX_PROP(Response))
    };

    struct PhysicsSettings
    {
        /**
         * Physics implementation.
         */
        EPhysicsBackend Backend = EPhysicsBackend::Box2D;

        /** Acceleration on dynamic bodies, world units / s^2. Y-up, so falling is negative. */
        Vector2F Gravity = { 0.f, -981.f };

        /** World units per metre (~100 in 2D). */
        float LengthUnitsPerMeter = 100.f;

        /** Solver sub-steps per fixed step. Higher is more stable but costs more. */
        Int32 SubStepCount = 4;

        /** Optional kill volume. */
        WorldBoundsSettings WorldBounds;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PhysicsSettings, Backend, Gravity,
                                                    LengthUnitsPerMeter, SubStepCount, WorldBounds)

        OPAAX_PROPERTIES(PhysicsSettings,
                         OPAAX_PROP(Backend),
                         OPAAX_PROP(Gravity),
                         OPAAX_PROP(LengthUnitsPerMeter).SetRange(1.f, 1000.f),
                         OPAAX_PROP(SubStepCount).SetRange(1.f, 16.f),
                         OPAAX_PROP(WorldBounds))
    };

    struct StatsSettings
    {
        /**
         * Profile a shipping build (dev builds always profile). Off by default.
         */
        bool EnableInShipBuild = false;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(StatsSettings, EnableInShipBuild)

        OPAAX_PROPERTIES(StatsSettings, OPAAX_PROP(EnableInShipBuild))
    };

    struct AudioSettings
    {
        /** Off: no audio device is opened; sounds play silently (a server, a headless run). */
        bool bEnabled = true;

        /** The library that opens the device and decodes sound files. */
        EAudioBackend Backend = EAudioBackend::Miniaudio;

        /** Linear: every bus is under the master. */
        float MasterVolume = 1.f;

        /** The mix tree under the master. Volumes apply at once; added or removed buses at restart. */
        TDynArray<AudioBusSettings> Buses = DefaultAudioBuses();

        /** Sounds playing at once; one more replaces the least important (with a warning). */
        Uint32 MaxVoices = 64;

        /** An audio bank held for the whole game (menus, the player, UI). */
        TAssetRef<AudioBank> GlobalBank;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioSettings, bEnabled, Backend, MasterVolume, Buses, MaxVoices,
                                                    GlobalBank)

        OPAAX_PROPERTIES(AudioSettings,
                         OPAAX_PROP(bEnabled).SetFlags(EPropertyFlags::NeedRestart)
                                             .SetTooltip("Off: no audio device; sounds play silently."),
                         OPAAX_PROP(Backend).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(MasterVolume).SetRange(0.f, 1.f),
                         OPAAX_PROP(Buses).SetTooltip("The buses under the master. A volume applies at once; "
                                                      "a bus added, removed or moved at the next start."),
                         OPAAX_PROP(MaxVoices).SetRange(1.f, 512.f).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(GlobalBank).SetFlags(EPropertyFlags::NeedRestart)
                                               .SetTooltip("An audio bank held for the whole game: menus, the player, UI."))
    };

    struct EngineConfigData
    {
        WindowSettings  Window;
        TimeSettings    Time;
        RenderSettings  Render;
        PhysicsSettings Physics;
        AudioSettings   Audio;
        StatsSettings   Stats;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EngineConfigData, Window, Time, Render, Physics, Audio, Stats)

        // NeedRestart on groups read at startup. Render and Audio are mixed, so only some fields have it.
        OPAAX_PROPERTIES(EngineConfigData,
                         OPAAX_PROP(Window),
                         OPAAX_PROP(Time),
                         OPAAX_PROP(Render),
                         OPAAX_PROP(Physics).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Audio),
                         OPAAX_PROP(Stats).SetFlags(EPropertyFlags::NeedRestart))
    };
}
