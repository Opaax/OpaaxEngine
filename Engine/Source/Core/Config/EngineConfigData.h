#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxMathTypes.h"

namespace Opaax
{
    // =============================================================================
    // JSON keys — mirror EngineConfig's nested schema (window/assets/log/render/physics).
    // =============================================================================
    namespace Opaax_Engine_Config
    {
        inline const char* CONFIG_FILE_NAME = "engine.config.json";

        inline const char* VERSION_KEY      = "version";
        inline const char* WINDOW_KEY       = "window";
        inline const char* ASSETS_KEY       = "assets";
        inline const char* LOG_KEY          = "log";
        inline const char* RENDER_KEY       = "render";
        inline const char* PHYSICS_KEY      = "physics";
        inline const char* WORLD_BOUNDS_KEY = "worldBounds";

        inline const char* TITLE_KEY           = "title";
        inline const char* WIDTH_KEY           = "width";
        inline const char* HEIGHT_KEY          = "height";
        inline const char* ENGINE_ROOT_KEY     = "engineRoot";
        inline const char* ENGINE_MANIFEST_KEY = "engineManifest";
        inline const char* LEVEL_KEY           = "level";
        inline const char* BACKEND_KEY         = "backend";
        inline const char* INTERPOLATION_KEY   = "interpolation";
        inline const char* ENABLED_KEY         = "enabled";
        inline const char* MIN_KEY             = "min";
        inline const char* MAX_KEY             = "max";
        inline const char* RESPONSE_KEY        = "response";
    }

    // =============================================================================
    // EngineConfigData — POD copied 1:1 from EngineConfig. Defaults match the historical
    // hardcoded values, so a missing config keeps behavior unchanged.
    // =============================================================================
    struct EngineConfigData
    {
        //----- window ---------------------------------------------------------
        OpaaxString WindowTitle  = OpaaxString("Opaax Engine");
        Uint32      WindowWidth  = 1280;
        Uint32      WindowHeight = 720;

        //----- assets ---------------------------------------------------------
        OpaaxString EngineAssetsRoot      = OpaaxString("Engine/Assets");
        OpaaxString EngineManifestRelPath = OpaaxString("Engine/Assets/AssetManifest.json");

        //----- log ------------------------------------------------------------
        OpaaxString LogLevel = OpaaxString("trace");

        //----- render ---------------------------------------------------------
        OpaaxString RenderBackend       = OpaaxString("OpenGL");
        bool        RenderInterpolation = true;

        //----- physics --------------------------------------------------------
        OpaaxString PhysicsBackend             = OpaaxString("Box2D");
        bool        PhysicsWorldBoundsEnabled  = false;
        Vector2F    PhysicsWorldBoundsMin      = Vector2F(-100000.f, -100000.f);
        Vector2F    PhysicsWorldBoundsMax      = Vector2F( 100000.f,  100000.f);
        OpaaxString PhysicsWorldBoundsResponse = OpaaxString("EventAndDestroy");
    };

    // Pure, tolerant parser — bad JSON / missing fields keep the defaults, never throws.
    OPAAX_API EngineConfigData ParseEngineConfig(const OpaaxString& InJsonText);
    // Serialize to pretty JSON — the template written when no config file exists.
    OPAAX_API OpaaxString      SerializeEngineConfig(const EngineConfigData& InData);
}
