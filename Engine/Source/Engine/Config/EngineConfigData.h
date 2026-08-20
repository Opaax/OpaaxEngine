#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/MathsJson.hpp"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"

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
    //   Defaults match the historical hardcoded values, so a missing config keeps behaviour
    //   unchanged — and with _WITH_DEFAULT, so does a config missing any single key.
    // =============================================================================

    struct WindowSettings
    {
        OpaaxString Title  = OpaaxString("Opaax Engine");
        Uint32      Width  = 1280;
        Uint32      Height = 720;

        // Stringly-typed like Backend below: this header stays free of Window.h, and the enum
        // conversion happens at the point of use (MakeWindowProps).
        OpaaxString Mode = OpaaxString("Windowed");

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WindowSettings, Title, Width, Height, Mode)

        OPAAX_PROPERTIES(WindowSettings,
                         OPAAX_PROP(Title),
                         OPAAX_PROP(Width).SetRange(320.f, 7680.f),
                         OPAAX_PROP(Height).SetRange(240.f, 4320.f),
                         OPAAX_PROP(Mode))
    };

    struct AssetSettings
    {
        OpaaxString EngineRoot     = OpaaxString("Engine/Assets");
        OpaaxString EngineManifest = OpaaxString("Engine/Assets/AssetManifest.json");

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AssetSettings, EngineRoot, EngineManifest)

        OPAAX_PROPERTIES(AssetSettings,
                         OPAAX_PROP(EngineRoot),
                         OPAAX_PROP(EngineManifest))
    };

    struct LogSettings
    {
        OpaaxString Level = OpaaxString("trace");

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LogSettings, Level)

        OPAAX_PROPERTIES(LogSettings, OPAAX_PROP(Level))
    };

    struct RenderSettings
    {
        OpaaxString Backend       = OpaaxString("OpenGL");
        bool        Interpolation = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings, Backend, Interpolation)

        OPAAX_PROPERTIES(RenderSettings,
                         OPAAX_PROP(Backend),
                         OPAAX_PROP(Interpolation))
    };

    struct WorldBoundsSettings
    {
        bool        Enabled  = false;
        Vector2F    Min      = Vector2F(-100000.f, -100000.f);
        Vector2F    Max      = Vector2F(100000.f, 100000.f);
        OpaaxString Response = OpaaxString("EventAndDestroy");

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WorldBoundsSettings, Enabled, Min, Max, Response)

        OPAAX_PROPERTIES(WorldBoundsSettings,
                         OPAAX_PROP(Enabled),
                         OPAAX_PROP(Min),
                         OPAAX_PROP(Max),
                         OPAAX_PROP(Response))
    };

    struct PhysicsSettings
    {
        OpaaxString         Backend = OpaaxString("Box2D");
        WorldBoundsSettings WorldBounds;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PhysicsSettings, Backend, WorldBounds)

        OPAAX_PROPERTIES(PhysicsSettings,
                         OPAAX_PROP(Backend),
                         OPAAX_PROP(WorldBounds))
    };

    struct EngineConfigData
    {
        WindowSettings  Window;
        AssetSettings   Assets;
        LogSettings     Log;
        RenderSettings  Render;
        PhysicsSettings Physics;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EngineConfigData, Window, Assets, Log, Render, Physics)

        // NeedRestart on every group, because every reader of this file reads it once during boot:
        // the window is built from Window, RendererManager resolves Render.Backend at Startup, and
        // Assets / Log / Physics have no reader at all yet.
        OPAAX_PROPERTIES(EngineConfigData,
                         OPAAX_PROP(Window).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Assets).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Log).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Render).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Physics).SetFlags(EPropertyFlags::NeedRestart))
    };
}
