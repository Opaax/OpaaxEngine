#include "AssetScanner.h"
#include "Core/OpaaxPath.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace Opaax
{
    // =============================================================================
    // Extension → type mapping
    // =============================================================================
    OpaaxStringID AssetScanner::ResolveType(const std::filesystem::path& InPath) noexcept
    {
        const std::string lExt = InPath.extension().string();
        const std::string lDir = InPath.parent_path().filename().string();

        // Textures
        if (lExt == ".png"  || lExt == ".jpg" || lExt == ".jpeg" ||
            lExt == ".bmp"  || lExt == ".tga")
        {
            return OpaaxStringID("Texture2D");
        }

        // Audio
        if (lExt == ".wav" || lExt == ".ogg" || lExt == ".mp3")
        {
            return OpaaxStringID("AudioClip");
        }

        // Shaders
        if (lExt == ".glsl" || lExt == ".vert" || lExt == ".frag")
        {
            return OpaaxStringID("Shader");
        }

        // JSON — disambiguate by parent directory name
        if (lExt == ".json")
        {
            if (lDir == "Animations") { return OpaaxStringID("Animation"); }
            if (lDir == "InputMaps")  { return OpaaxStringID("InputMap");  }
            if (lDir == "Scenes")     { return OpaaxStringID("Scene");     }
            // Generic JSON — could be anything
            return OpaaxStringID("Data");
        }

        return OpaaxStringID("Unknown");
    }

    // =============================================================================
    // ID generation
    // =============================================================================
    OpaaxString AssetScanner::GenerateID(
        const std::filesystem::path& InAbsFilePath,
        const std::filesystem::path& InAbsRootPath) noexcept
    {
        // Make path relative to root
        const std::filesystem::path lRelPath =
            std::filesystem::relative(InAbsFilePath, InAbsRootPath);

        // Remove extension
        std::filesystem::path lIDPath = lRelPath;
        lIDPath.replace_extension("");

        // Normalise separators to forward slash
        std::string lID = lIDPath.generic_string();

        return OpaaxString(lID.c_str());
    }

    // =============================================================================
    // Scan
    // =============================================================================
    AssetScanner::ScanResult AssetScanner::Scan(const ScanConfig& InConfig)
    {
        ScanResult lResult;

        const OpaaxString lAbsRoot = OpaaxPath::Resolve(InConfig.RootDir);
        const std::filesystem::path lRootPath(lAbsRoot.CStr());

        if (!std::filesystem::exists(lRootPath))
        {
            OPAAX_CORE_WARN("AssetScanner::Scan — root dir '{}' does not exist, skipped.",
                InConfig.RootDir);
            return lResult;
        }

        OPAAX_CORE_INFO("AssetScanner: scanning '{}'...", lAbsRoot);

        // --- Scan files on disk ---
        for (const auto& lDirEntry :
             std::filesystem::recursive_directory_iterator(lRootPath))
        {
            if (!lDirEntry.is_regular_file()) { continue; }

            const std::filesystem::path& lFilePath = lDirEntry.path();
            const OpaaxStringID lType = ResolveType(lFilePath);

            // Skip unknown types and scenes (managed by SceneManager)
            if (lType == OpaaxStringID("Unknown"))
            {
                ++lResult.Skipped;
                continue;
            }

            // Generate logical ID
            const OpaaxString lID = GenerateID(lFilePath, lRootPath);
            const OpaaxStringID lStringID(lID);

            // Generate relative path (relative to base path, not root)
            const std::string lAbsFileStr = lFilePath.generic_string();
            const OpaaxString lRelPath =
                OpaaxPath::MakeRelative(OpaaxString(lAbsFileStr.c_str()));

            // --- Merge logic ---
            if (AssetManifest::Contains(lStringID))
            {
                // Already in manifest — preserve existing entry
                ++lResult.Existing;
                OPAAX_CORE_TRACE("AssetScanner: '{}' already in manifest, preserved.", lID);
            }
            else
            {
                // New entry — add to manifest
                AssetDescriptor lDesc;
                lDesc.ID      = lStringID;
                lDesc.RelPath = lRelPath;
                lDesc.Type    = lType;

                AssetManifest::Add(std::move(lDesc));
                ++lResult.Added;

                OPAAX_CORE_INFO("AssetScanner: added '{}' → '{}'", lID, lRelPath);
            }
        }

        // --- Flag missing entries ---
        if (InConfig.bFlagMissing)
        {
            for (const auto& [lKey, lDesc] : AssetManifest::GetAll())
            {
                const OpaaxString lAbsPath = OpaaxPath::Resolve(lDesc.RelPath);
                if (!std::filesystem::exists(lAbsPath.CStr()))
                {
                    AssetManifest::SetMissing(lDesc.ID, true);
                    ++lResult.Missing;
                    OPAAX_CORE_WARN("AssetScanner: '{}' missing on disk — '{}'",
                        lDesc.ID, lDesc.RelPath);
                }
                else
                {
                    AssetManifest::SetMissing(lDesc.ID, false);
                }
            }
        }

        // --- Save updated manifest ---
        SaveManifest(InConfig.ManifestAbsPath);

        OPAAX_CORE_INFO("AssetScanner: done — added={} existing={} missing={} skipped={}",
            lResult.Added, lResult.Existing, lResult.Missing, lResult.Skipped);

        return lResult;
    }

    // =============================================================================
    // Save
    // =============================================================================
    bool AssetScanner::SaveManifest(const OpaaxString& InAbsPath) noexcept
    {
        nlohmann::json lRoot;
        lRoot["assets"] = nlohmann::json::array();

        for (const auto& [lKey, lDesc] : AssetManifest::GetAll())
        {
            nlohmann::json lEntry;
            lEntry["id"]   = lDesc.ID.ToString().CStr();
            lEntry["path"] = lDesc.RelPath.CStr();
            lEntry["type"] = lDesc.Type.ToString().CStr();

            if (lDesc.bMissing)
            {
                lEntry["missing"] = true;
            }

            lRoot["assets"].push_back(lEntry);
        }

        std::ofstream lFile(InAbsPath.CStr());
        if (!lFile.is_open())
        {
            OPAAX_CORE_ERROR("AssetScanner::SaveManifest — cannot write to '{}'",
                InAbsPath);
            return false;
        }

        lFile << lRoot.dump(4);
        OPAAX_CORE_INFO("AssetScanner: manifest saved to '{}'", InAbsPath);
        return true;
    }

} // namespace Opaax