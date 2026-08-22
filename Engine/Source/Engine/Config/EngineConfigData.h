#pragma once

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"   // every enum field below writes its ToString label
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringJson.h"
#include "Core/Window/Window.h"              // EWindowMode
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

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings, Backend)

        OPAAX_PROPERTIES(RenderSettings, OPAAX_PROP(Backend))
    };

    struct EngineConfigData
    {
        WindowSettings Window;
        RenderSettings Render;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EngineConfigData, Window, Render)

        // NeedRestart on every group, because every reader of this file reads it once during boot:
        // the window is built from Window, RendererManager resolves Render.Backend at Startup.
        OPAAX_PROPERTIES(EngineConfigData,
                         OPAAX_PROP(Window).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(Render).SetFlags(EPropertyFlags::NeedRestart))
    };
}
