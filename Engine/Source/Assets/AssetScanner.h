#pragma once

#include "AssetManifest.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // AssetScanner
    //
    // Scans one or more root directories for known asset types.
    // Merges discovered assets into AssetManifest — never overwrites existing entries.
    // Saves the updated manifest to disk after each scan.
    //
    // ID generation rule:
    //   Root     : "GameAssets"
    //   File     : "GameAssets/Textures/Player.png"
    //   ID       : "Textures/Player"    (relative to root, all extensions stripped)
    //   Compound : "GameAssets/Foo.scene.json" → "Foo"
    //
    // Type discovery is filename-pattern based (no parent-directory dependency),
    // so any asset can live in any folder.
    //
    // Supported extensions → asset type mapping:
    //   .png .jpg .jpeg .bmp .tga  →  "Texture2D"
    //   .wav .ogg .mp3             →  "AudioClip"
    //   .glsl .vert .frag          →  "Shader"
    //   *.scene.json               →  "Scene"      (lifecycle handled by SceneManager,
    //                                                manifest entry is informational)
    //   *.anim.json                →  "Animation"
    //   *.input.json               →  "InputMap"
    //   *.json (no compound)       →  "Data"
    //
    // NOTE: Scan is synchronous — call from main thread only.
    //   For large asset libraries this could be moved to a job,
    //   but for a small indie project synchronous is fine.
    // =============================================================================

    /**
     * @class AssetScanner
     *
     * Scans one or more root directories for known asset types.
     */
    class OPAAX_API AssetScanner
    {
    public:
        struct ScanConfig
        {
            // Root directory to scan — relative to base path
            OpaaxString RootDir;

            // Absolute path to the manifest file to update
            OpaaxString ManifestAbsPath;

            // If true, entries in the manifest that have no corresponding
            // file on disk are flagged as missing (not removed).
            bool bFlagMissing = true;
        };

        struct ScanResult
        {
            Uint32 Added    = 0;    // new entries added to manifest
            Uint32 Existing = 0;    // entries already in manifest, preserved
            Uint32 Missing  = 0;    // manifest entries with no file on disk
            Uint32 Skipped  = 0;    // files with unrecognised extensions
        };

        // Scan InConfig.RootDir, merge into AssetManifest, save to disk.
        // Returns scan statistics.
        static ScanResult Scan(const ScanConfig& InConfig);

    private:
        // Derive asset type string from filename pattern (extension + compound extension).
        static OpaaxStringID ResolveType(const std::filesystem::path& InPath) noexcept;

        // Generate logical ID from file path relative to root, stripping every extension.
        // "GameAssets/Textures/Player.png"   with root "GameAssets" → "Textures/Player"
        // "GameAssets/Scenes/Foo.scene.json" with root "GameAssets" → "Scenes/Foo"
        static OpaaxString GenerateID(const std::filesystem::path& InAbsFilePath,
                                      const std::filesystem::path& InAbsRootPath) noexcept;

        // Save current manifest state to disk.
        static bool SaveManifest(const OpaaxString& InAbsPath) noexcept;
    };

} // namespace Opaax
