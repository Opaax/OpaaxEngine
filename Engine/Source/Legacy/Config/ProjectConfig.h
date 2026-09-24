#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"

namespace Opaax
{
    /**
     * @class ProjectConfig
     *
     * Static, process-wide configuration of the active consumer project (the
     * "game" the engine is currently serving). One `<Name>.opaaxproj` JSON file
     * per project. Loaded once at bootstrap by CoreEngineApp after EngineConfig.
     *
     * Defaults mirror the historical `Game` project values so a missing or
     * unresolved project file leaves the engine bootable with prior behavior.
     *
     * NOTE: Load() must run after EngineConfig::Load() and before any consumer
     *   reads AssetsRoot() / AssetsManifestRelPath() (manifest loading, asset
     *   browser scan, editor scene-dialog default).
     */
    class OPAAX_API ProjectConfig
    {
    public:
        /**
         * Load the project file from disk. If the file does not exist, a
         * default file is generated at InAbsPath and the in-memory defaults
         * are kept.
         * @return true on success or after a successful default-generation;
         *         false if a file existed but failed to parse.
         */
        static bool Load(const OpaaxString& InAbsPath);

        /**
         * Persist the current values back to InAbsPath.
         */
        static bool Save(const OpaaxString& InAbsPath);

        // ---- Identity -------------------------------------------------------
        static const OpaaxString& Name()                   noexcept { return s_Name; }

        // ---- Assets ---------------------------------------------------------
        static const OpaaxString& AssetsRoot()             noexcept { return s_AssetsRoot; }
        static const OpaaxString& AssetsManifestRelPath()  noexcept { return s_AssetsManifestRelPath; }

        // ---- Scene ----------------------------------------------------------
        static const OpaaxString& DefaultSceneRelPath()    noexcept { return s_DefaultSceneRelPath; }

        // ---- Editor ---------------------------------------------------------
        // Default directory for Save/Open scene dialogs. Authoring layout —
        // released games never read this.
        static const OpaaxString& EditorDefaultScenePath() noexcept { return s_EditorDefaultScenePath; }

    private:
        static bool GenerateDefault(const OpaaxString& InAbsPath);

        static OpaaxString s_Name;
        static OpaaxString s_AssetsRoot;
        static OpaaxString s_AssetsManifestRelPath;
        static OpaaxString s_DefaultSceneRelPath;
        static OpaaxString s_EditorDefaultScenePath;
    };
} // namespace Opaax
