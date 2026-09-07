#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"            // Vector2F — Physics.Gravity
#include "Core/Maths/MathsJson.hpp"          // ...and the json bridge that writes it
#include "Core/Reflection/OpaaxEnumJson.h"   // every enum field below writes its ToString label
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Window/Window.h"              // EWindowMode
#include "Physics/PhysicsBackend.h"          // EPhysicsBackend
#include "Physics/PhysicsTypes.h"            // EWorldBoundsResponse — PODs over Core, NOT the seam
#include "RHI/RHIBackend.h"                  // EBackend

namespace Opaax
{
    // =============================================================================
    // EngineConfigData — the engine runtime block of <ProjectRoot>/Configs/Engine.config.
    //
    //   NESTED STRUCTS MIRROR THE FILE. Each group is a type of its own, so the json nests because
    //   the C++ nests — instead of a flat struct plus a hand-written serializer that knew how to
    //   fold it. Serialization is the same one macro a component carries; there is no codec, no key
    //   constants and no parser here, which is the whole point (one idiom, not two).
    //
    //   Every field is read ONCE AT BOOT today, which is what NeedRestart says on each group. The
    //   flag sits on the GROUP rather than on every field, and it becomes per-field the day
    //   something is read live.
    //
    //   EVERY FIELD HERE HAS A READER. The Assets / Log / Physics groups and Render.Interpolation
    //   were deleted on 2026-08-21 — they had none, and a settings screen offering values that do
    //   nothing is worse than a short one. Each comes back with the system that reads it.
    //   *Physics came back on 2026-09-07 with the physics seam, exactly on that clause.*
    //
    //   Defaults match the historical hardcoded values, so a missing config keeps behaviour
    //   unchanged — and with _WITH_DEFAULT, so does a config missing any single key.
    // =============================================================================

    struct WindowSettings
    {
        OpaaxString Title  = OpaaxString("Opaax Engine");
        Uint32      Width  = 1280;
        Uint32      Height = 720;

        // A REAL enum, so the editor gives it a dropdown and a typo is not expressible. It still
        // writes "Windowed" — the json bridge stores the ToString label, never the ordinal — so the
        // file did not change when this stopped being a string.
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
        // What the project ASKS for. Whether it can be honoured is ResolveSupportedBackend's answer,
        // asked at the point of use — Vulkan is a legal thing to write here and is coerced, loudly.
        EBackend Backend = EBackend::OpenGL;

        /**
         * Blend a fixed-step pose toward the next one for DISPLAY, so motion does not step at the
         * fixed rate on a faster screen. Deleted in the 2026-08-21 cleanup for having no reader;
         * back with one (PH21). ON by default — the interpolated picture is the correct one.
         */
        bool bInterpolation = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings, Backend, bInterpolation)

        OPAAX_PROPERTIES(RenderSettings,
                         OPAAX_PROP(Backend),
                         OPAAX_PROP(bInterpolation)
                             .SetTooltip("Smooth motion between fixed steps.\n"
                                         "Display only — gameplay always reads the raw pose.\n"
                                         "Subtle at 60Hz with vsync, pronounced above it."))
    };

    struct WorldBoundsSettings
    {
        /**
         * OFF by default, and generously sized when on. A kill volume is the wrong default for an
         * endless scroller, and a body quietly vanishing is worse than one falling forever.
         */
        bool bEnabled = false;

        Vector2F Min = { -100000.f, -100000.f };
        Vector2F Max = {  100000.f,  100000.f };

        /**
         * What the ENGINE does after the event, which always fires either way. EventOnly leaves
         * the reaction to the game; EventAndDestroy also reaps the entity.
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
         * Which implementation backs IPhysicsWorld. A real enum, so the editor gives it a dropdown
         * and an unbuildable backend is not expressible — the same treatment Render.Backend gets.
         */
        EPhysicsBackend Backend = EPhysicsBackend::Box2D;

        /** Acceleration on dynamic bodies, world units / s^2. Y-up, so falling is negative. */
        Vector2F Gravity = { 0.f, -981.f };

        /** How many world units make a metre. ~100 is the 2D convention the sprites are authored at. */
        float LengthUnitsPerMeter = 100.f;

        /** Solver sub-steps per fixed step. Higher is a stabler stack for more cost. */
        Int32 SubStepCount = 4;

        /** The optional kill volume. Its own type, so the json nests because the C++ nests. */
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
         * Profile a SHIPPING build. A dev build always profiles — that is what dev means — so this
         * asks the one question the build cannot answer for itself, and it is a config rather than
         * a CMake flag precisely so profiling a shipped game needs no recompile.
         *
         * Off by default: a shipped binary should not carry live instrumentation unless asked.
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

        // NeedRestart on every group, because every reader of this file reads it once during boot:
        // the window is built from Window, RendererManager resolves Render.Backend at Startup, the
        // stats service is provided-or-not in Bootstrap, and a physics world is built from Physics
        // when a Play world starts — which a running one cannot be re-founded on.
        OPAAX_PROPERTIES(EngineConfigData,
                         OPAAX_PROP(Window).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Render).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Physics).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Stats).SetFlags(EPropertyFlags::NeedRestart))
    };
}
