#pragma once

#include <nlohmann/json.hpp>

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

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WindowSettings, Title, Width, Height, Mode)

        OPAAX_PROPERTIES(WindowSettings,
                         OPAAX_PROP(Title),
                         OPAAX_PROP(Width).SetRange(320.f, 7680.f),
                         OPAAX_PROP(Height).SetRange(240.f, 4320.f),
                         OPAAX_PROP(Mode))
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

    struct EngineConfigData
    {
        WindowSettings  Window;
        RenderSettings  Render;
        PhysicsSettings Physics;
        StatsSettings   Stats;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EngineConfigData, Window, Render, Physics, Stats)

        // NeedRestart on groups read at startup. Render is mixed, so only Backend has the flag.
        OPAAX_PROPERTIES(EngineConfigData,
                         OPAAX_PROP(Window).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Render),
                         OPAAX_PROP(Physics).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Stats).SetFlags(EPropertyFlags::NeedRestart))
    };
}
