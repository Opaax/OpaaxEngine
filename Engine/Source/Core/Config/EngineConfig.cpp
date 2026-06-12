#include "EngineConfig.h"

#include "Core/Log/OpaaxLog.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Opaax
{
    // Default values mirror the previous hardcoded constants so a missing
    // engine.config.json keeps the engine bootable with unchanged behavior.
    OpaaxString EngineConfig::s_WindowTitle           = OpaaxString("Opaax Engine");
    Uint32      EngineConfig::s_WindowWidth           = 1280;
    Uint32      EngineConfig::s_WindowHeight          = 720;
    OpaaxString EngineConfig::s_EngineAssetsRoot      = OpaaxString("Engine/Assets");
    OpaaxString EngineConfig::s_EngineManifestRelPath = OpaaxString("Engine/Assets/AssetManifest.json");
    OpaaxString EngineConfig::s_LogLevel              = OpaaxString("trace");
    OpaaxString EngineConfig::s_RenderBackend         = OpaaxString("OpenGL");
    bool        EngineConfig::s_RenderInterpolation   = true;
    OpaaxString EngineConfig::s_PhysicsBackend        = OpaaxString("Box2D");
    bool        EngineConfig::s_PhysicsWorldBoundsEnabled  = false;
    Vector2F    EngineConfig::s_PhysicsWorldBoundsMin      = { -100000.f, -100000.f };
    Vector2F    EngineConfig::s_PhysicsWorldBoundsMax      = {  100000.f,  100000.f };
    OpaaxString EngineConfig::s_PhysicsWorldBoundsResponse = OpaaxString("EventAndDestroy");

    bool EngineConfig::GenerateDefault(const OpaaxString& InAbsPath)
    {
        try
        {
            const std::filesystem::path lPath(InAbsPath.CStr());
            std::filesystem::create_directories(lPath.parent_path());

            std::ofstream lFile(InAbsPath.CStr());
            if (!lFile.is_open())
            {
                OPAAX_CORE_ERROR("EngineConfig::GenerateDefault — cannot create '{}'",
                    InAbsPath);
                return false;
            }

            nlohmann::json lRoot;
            lRoot["version"] = 1;
            lRoot["window"]  = {
                { "title",  s_WindowTitle.CStr()  },
                { "width",  s_WindowWidth         },
                { "height", s_WindowHeight        }
            };
            lRoot["assets"] = {
                { "engineRoot",     s_EngineAssetsRoot.CStr()      },
                { "engineManifest", s_EngineManifestRelPath.CStr() }
            };
            lRoot["log"]    = { { "level",   s_LogLevel.CStr()      } };
            lRoot["render"]  = {
                { "backend",       s_RenderBackend.CStr() },
                { "interpolation", s_RenderInterpolation   }
            };
            lRoot["physics"] = {
                { "backend", s_PhysicsBackend.CStr() },
                { "worldBounds", {
                    { "enabled",  s_PhysicsWorldBoundsEnabled },
                    { "min",      { s_PhysicsWorldBoundsMin.x, s_PhysicsWorldBoundsMin.y } },
                    { "max",      { s_PhysicsWorldBoundsMax.x, s_PhysicsWorldBoundsMax.y } },
                    { "response", s_PhysicsWorldBoundsResponse.CStr() }
                }}
            };

            lFile << lRoot.dump(4);

            OPAAX_CORE_INFO("EngineConfig: generated default config at '{}'", InAbsPath);
            return true;
        }
        catch (const std::exception& e)
        {
            OPAAX_CORE_ERROR("EngineConfig::GenerateDefault — exception: {}", e.what());
            return false;
        }
    }

    bool EngineConfig::Load(const OpaaxString& InAbsPath)
    {
        std::ifstream lFile(InAbsPath.CStr());

        if (!lFile.is_open())
        {
            OPAAX_CORE_WARN("EngineConfig::Load — '{}' not found, generating default.",
                InAbsPath);
            return GenerateDefault(InAbsPath);
        }

        nlohmann::json lRoot;
        try
        {
            lFile >> lRoot;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            OPAAX_CORE_ERROR("EngineConfig::Load — parse error in '{}': {}",
                InAbsPath, e.what());
            return false;
        }

        // Each block is read defensively — a missing or wrong-typed field
        // leaves the corresponding default in place rather than aborting.
        if (lRoot.contains("window") && lRoot["window"].is_object())
        {
            const auto& lWin = lRoot["window"];
            if (lWin.contains("title")  && lWin["title"].is_string())
            {
                s_WindowTitle = OpaaxString(lWin["title"].get<std::string>().c_str());
            }
            if (lWin.contains("width")  && lWin["width"].is_number_unsigned())
            {
                s_WindowWidth = lWin["width"].get<Uint32>();
            }
            if (lWin.contains("height") && lWin["height"].is_number_unsigned())
            {
                s_WindowHeight = lWin["height"].get<Uint32>();
            }
        }

        if (lRoot.contains("assets") && lRoot["assets"].is_object())
        {
            const auto& lA = lRoot["assets"];
            if (lA.contains("engineRoot")     && lA["engineRoot"].is_string())
            {
                s_EngineAssetsRoot = OpaaxString(lA["engineRoot"].get<std::string>().c_str());
            }
            if (lA.contains("engineManifest") && lA["engineManifest"].is_string())
            {
                s_EngineManifestRelPath = OpaaxString(lA["engineManifest"].get<std::string>().c_str());
            }
        }

        if (lRoot.contains("log") && lRoot["log"].is_object())
        {
            const auto& lLog = lRoot["log"];
            if (lLog.contains("level") && lLog["level"].is_string())
            {
                s_LogLevel = OpaaxString(lLog["level"].get<std::string>().c_str());
            }
        }

        if (lRoot.contains("render") && lRoot["render"].is_object())
        {
            const auto& lR = lRoot["render"];
            if (lR.contains("backend") && lR["backend"].is_string())
            {
                s_RenderBackend = OpaaxString(lR["backend"].get<std::string>().c_str());
            }
            if (lR.contains("interpolation") && lR["interpolation"].is_boolean())
            {
                s_RenderInterpolation = lR["interpolation"].get<bool>();
            }
        }

        if (lRoot.contains("physics") && lRoot["physics"].is_object())
        {
            const auto& lP = lRoot["physics"];
            if (lP.contains("backend") && lP["backend"].is_string())
            {
                s_PhysicsBackend = OpaaxString(lP["backend"].get<std::string>().c_str());
            }

            if (lP.contains("worldBounds") && lP["worldBounds"].is_object())
            {
                const auto& lWB = lP["worldBounds"];
                if (lWB.contains("enabled") && lWB["enabled"].is_boolean())
                {
                    s_PhysicsWorldBoundsEnabled = lWB["enabled"].get<bool>();
                }
                if (lWB.contains("min") && lWB["min"].is_array() && lWB["min"].size() == 2
                    && lWB["min"][0].is_number() && lWB["min"][1].is_number())
                {
                    s_PhysicsWorldBoundsMin = { lWB["min"][0].get<float>(), lWB["min"][1].get<float>() };
                }
                if (lWB.contains("max") && lWB["max"].is_array() && lWB["max"].size() == 2
                    && lWB["max"][0].is_number() && lWB["max"][1].is_number())
                {
                    s_PhysicsWorldBoundsMax = { lWB["max"][0].get<float>(), lWB["max"][1].get<float>() };
                }
                if (lWB.contains("response") && lWB["response"].is_string())
                {
                    s_PhysicsWorldBoundsResponse = OpaaxString(lWB["response"].get<std::string>().c_str());
                }
            }
        }

        OPAAX_CORE_INFO("EngineConfig: loaded '{}' (window={}x{}, log={}, render={}, physics={})",
            InAbsPath, s_WindowWidth, s_WindowHeight, s_LogLevel, s_RenderBackend, s_PhysicsBackend);

        return true;
    }

    bool EngineConfig::Save(const OpaaxString& InAbsPath)
    {
        try
        {
            const std::filesystem::path lPath(InAbsPath.CStr());
            std::filesystem::create_directories(lPath.parent_path());

            std::ofstream lFile(InAbsPath.CStr());
            if (!lFile.is_open())
            {
                OPAAX_CORE_ERROR("EngineConfig::Save — cannot write to '{}'",
                    InAbsPath);
                return false;
            }

            nlohmann::json lRoot;
            lRoot["version"] = 1;
            lRoot["window"]  = {
                { "title",  s_WindowTitle.CStr()  },
                { "width",  s_WindowWidth         },
                { "height", s_WindowHeight        }
            };
            lRoot["assets"] = {
                { "engineRoot",     s_EngineAssetsRoot.CStr()      },
                { "engineManifest", s_EngineManifestRelPath.CStr() }
            };
            lRoot["log"]    = { { "level",   s_LogLevel.CStr()      } };
            lRoot["render"]  = {
                { "backend",       s_RenderBackend.CStr() },
                { "interpolation", s_RenderInterpolation   }
            };
            lRoot["physics"] = {
                { "backend", s_PhysicsBackend.CStr() },
                { "worldBounds", {
                    { "enabled",  s_PhysicsWorldBoundsEnabled },
                    { "min",      { s_PhysicsWorldBoundsMin.x, s_PhysicsWorldBoundsMin.y } },
                    { "max",      { s_PhysicsWorldBoundsMax.x, s_PhysicsWorldBoundsMax.y } },
                    { "response", s_PhysicsWorldBoundsResponse.CStr() }
                }}
            };

            lFile << lRoot.dump(4);

            OPAAX_CORE_INFO("EngineConfig: saved '{}'", InAbsPath);
            return true;
        }
        catch (const std::exception& e)
        {
            OPAAX_CORE_ERROR("EngineConfig::Save — exception: {}", e.what());
            return false;
        }
    }
} // namespace Opaax
