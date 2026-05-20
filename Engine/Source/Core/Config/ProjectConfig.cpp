#include "ProjectConfig.h"

#include "Core/Log/OpaaxLog.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Opaax
{
    // Defaults mirror the historical hardcoded `Game` project values so a
    // missing project file yields behavior identical to pre-M2.6 builds.
    OpaaxString ProjectConfig::s_Name                    = OpaaxString("Game");
    OpaaxString ProjectConfig::s_AssetsRoot              = OpaaxString("Game/Assets");
    OpaaxString ProjectConfig::s_AssetsManifestRelPath   = OpaaxString("Game/Assets/AssetManifest.json");
    OpaaxString ProjectConfig::s_DefaultSceneRelPath     = OpaaxString("");
    OpaaxString ProjectConfig::s_EditorDefaultScenePath  = OpaaxString("Game/Assets/Scenes");

    bool ProjectConfig::GenerateDefault(const OpaaxString& InAbsPath)
    {
        try
        {
            const std::filesystem::path lPath(InAbsPath.CStr());
            std::filesystem::create_directories(lPath.parent_path());

            std::ofstream lFile(InAbsPath.CStr());
            if (!lFile.is_open())
            {
                OPAAX_CORE_ERROR("ProjectConfig::GenerateDefault — cannot create '{}'",
                    InAbsPath);
                return false;
            }

            nlohmann::json lRoot;
            lRoot["version"]                = 1;
            lRoot["name"]                   = s_Name.CStr();
            lRoot["assetsRoot"]             = s_AssetsRoot.CStr();
            lRoot["assetsManifest"]         = s_AssetsManifestRelPath.CStr();
            lRoot["defaultScene"]           = s_DefaultSceneRelPath.CStr();
            lRoot["editorDefaultScenePath"] = s_EditorDefaultScenePath.CStr();

            lFile << lRoot.dump(4);

            OPAAX_CORE_INFO("ProjectConfig: generated default project file at '{}'", InAbsPath);
            return true;
        }
        catch (const std::exception& e)
        {
            OPAAX_CORE_ERROR("ProjectConfig::GenerateDefault — exception: {}", e.what());
            return false;
        }
    }

    bool ProjectConfig::Load(const OpaaxString& InAbsPath)
    {
        std::ifstream lFile(InAbsPath.CStr());

        if (!lFile.is_open())
        {
            OPAAX_CORE_WARN("ProjectConfig::Load — '{}' not found, generating default.",
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
            OPAAX_CORE_ERROR("ProjectConfig::Load — parse error in '{}': {}",
                InAbsPath, e.what());
            return false;
        }

        // Each field is read defensively — a missing or wrong-typed value
        // leaves the corresponding default in place rather than aborting.
        if (lRoot.contains("name") && lRoot["name"].is_string())
        {
            s_Name = OpaaxString(lRoot["name"].get<std::string>().c_str());
        }
        if (lRoot.contains("assetsRoot") && lRoot["assetsRoot"].is_string())
        {
            s_AssetsRoot = OpaaxString(lRoot["assetsRoot"].get<std::string>().c_str());
        }
        if (lRoot.contains("assetsManifest") && lRoot["assetsManifest"].is_string())
        {
            s_AssetsManifestRelPath = OpaaxString(lRoot["assetsManifest"].get<std::string>().c_str());
        }
        if (lRoot.contains("defaultScene") && lRoot["defaultScene"].is_string())
        {
            s_DefaultSceneRelPath = OpaaxString(lRoot["defaultScene"].get<std::string>().c_str());
        }
        if (lRoot.contains("editorDefaultScenePath") && lRoot["editorDefaultScenePath"].is_string())
        {
            s_EditorDefaultScenePath = OpaaxString(lRoot["editorDefaultScenePath"].get<std::string>().c_str());
        }

        OPAAX_CORE_INFO("ProjectConfig: loaded '{}' (name='{}', assets='{}')",
            InAbsPath, s_Name, s_AssetsRoot);

        return true;
    }

    bool ProjectConfig::Save(const OpaaxString& InAbsPath)
    {
        try
        {
            const std::filesystem::path lPath(InAbsPath.CStr());
            std::filesystem::create_directories(lPath.parent_path());

            std::ofstream lFile(InAbsPath.CStr());
            if (!lFile.is_open())
            {
                OPAAX_CORE_ERROR("ProjectConfig::Save — cannot write to '{}'",
                    InAbsPath);
                return false;
            }

            nlohmann::json lRoot;
            lRoot["version"]                = 1;
            lRoot["name"]                   = s_Name.CStr();
            lRoot["assetsRoot"]             = s_AssetsRoot.CStr();
            lRoot["assetsManifest"]         = s_AssetsManifestRelPath.CStr();
            lRoot["defaultScene"]           = s_DefaultSceneRelPath.CStr();
            lRoot["editorDefaultScenePath"] = s_EditorDefaultScenePath.CStr();

            lFile << lRoot.dump(4);

            OPAAX_CORE_INFO("ProjectConfig: saved '{}'", InAbsPath);
            return true;
        }
        catch (const std::exception& e)
        {
            OPAAX_CORE_ERROR("ProjectConfig::Save — exception: {}", e.what());
            return false;
        }
    }
} // namespace Opaax
