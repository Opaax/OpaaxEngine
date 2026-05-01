#include "EngineConfig.h"

#include "Core/Log/OpaaxLog.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Opaax
{
    // Default values mirror the previous hardcoded constants so a missing
    // engine.config.json yields behaviour identical to pre-M1 builds.
    OpaaxString EngineConfig::s_WindowTitle           = OpaaxString("Opaax Engine");
    Uint32      EngineConfig::s_WindowWidth           = 1280;
    Uint32      EngineConfig::s_WindowHeight          = 720;
    OpaaxString EngineConfig::s_EngineAssetsRoot      = OpaaxString("EngineAssets");
    OpaaxString EngineConfig::s_GameAssetsRoot        = OpaaxString("GameAssets");
    OpaaxString EngineConfig::s_EngineManifestRelPath = OpaaxString("EngineAssets/AssetManifest.json");
    OpaaxString EngineConfig::s_GameManifestRelPath   = OpaaxString("GameAssets/AssetManifest.json");
    OpaaxString EngineConfig::s_LogLevel              = OpaaxString("trace");
    OpaaxString EngineConfig::s_DefaultSceneRelPath   = OpaaxString("");

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
                { "gameRoot",       s_GameAssetsRoot.CStr()        },
                { "engineManifest", s_EngineManifestRelPath.CStr() },
                { "gameManifest",   s_GameManifestRelPath.CStr()   }
            };
            lRoot["log"]          = { { "level", s_LogLevel.CStr() } };
            lRoot["defaultScene"] = s_DefaultSceneRelPath.CStr();

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
            if (lA.contains("gameRoot")       && lA["gameRoot"].is_string())
            {
                s_GameAssetsRoot = OpaaxString(lA["gameRoot"].get<std::string>().c_str());
            }
            if (lA.contains("engineManifest") && lA["engineManifest"].is_string())
            {
                s_EngineManifestRelPath = OpaaxString(lA["engineManifest"].get<std::string>().c_str());
            }
            if (lA.contains("gameManifest")   && lA["gameManifest"].is_string())
            {
                s_GameManifestRelPath = OpaaxString(lA["gameManifest"].get<std::string>().c_str());
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

        if (lRoot.contains("defaultScene") && lRoot["defaultScene"].is_string())
        {
            s_DefaultSceneRelPath = OpaaxString(lRoot["defaultScene"].get<std::string>().c_str());
        }

        OPAAX_CORE_INFO("EngineConfig: loaded '{}' (window={}x{}, log={})",
            InAbsPath, s_WindowWidth, s_WindowHeight, s_LogLevel);

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
                { "gameRoot",       s_GameAssetsRoot.CStr()        },
                { "engineManifest", s_EngineManifestRelPath.CStr() },
                { "gameManifest",   s_GameManifestRelPath.CStr()   }
            };
            lRoot["log"]          = { { "level", s_LogLevel.CStr() } };
            lRoot["defaultScene"] = s_DefaultSceneRelPath.CStr();

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
