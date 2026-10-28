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
        OpaaxString Title  = OpaaxString("Opaax Engine");
        Uint32      Width  = 1280;
        Uint32      Height = 720;

        EWindowMode Mode = EWindowMode::Windowed;

        /** Waits for the screen's refresh before showing a frame: no tearing, one frame a refresh. */
        bool bVSync = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WindowSettings, Title, Width, Height, Mode, bVSync)

        // The window is made with the others at startup; vsync applies at once.
        OPAAX_PROPERTIES(WindowSettings,
                         OPAAX_PROP(Title).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Width).SetRange(320.f, 7680.f).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Height).SetRange(240.f, 4320.f).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Mode).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(bVSync).SetTooltip("Wait for the screen's refresh: no tearing.\n"
                                                       "Off in a run paced by --frame-pacing."))
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

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TimeSettings, FixedRate, bSnapToRefresh)

        OPAAX_PROPERTIES(TimeSettings,
                         OPAAX_PROP(FixedRate).SetRange(MIN_FIXED_RATE, MAX_FIXED_RATE)
                             .SetTooltip("Fixed steps a second: physics, movement, OnFixedUpdate.\n"
                                         "Moving things are drawn between steps, smooth at any screen rate."),
                         OPAAX_PROP(bSnapToRefresh)
                             .SetTooltip("With vsync, count each frame as whole refreshes of the screen:\n"
                                         "the timer's noise no longer shakes moving things.\n"
                                         "Off for a variable-refresh screen (G-Sync, FreeSync)."))
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
