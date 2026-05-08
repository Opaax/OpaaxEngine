#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/Log/OpaaxLog.h"

#include <filesystem>

namespace Opaax
{
    /**
     * @class OpaaxPath
     *
     * Path resolver. The canonical "relative path" form is project-root-relative
     * (e.g. "Game/Assets/Textures/Player.png"). ToAbsolute() resolves against the
     * project source tree in editor builds, and falls back to the exe directory
     * in release/standalone builds — CMake post-build deploys assets under the
     * same relative paths in both, so a single canonical form works everywhere.
     *
     * Usage:
     *   OpaaxString lFull = OpaaxPath::ToAbsolute("Engine/Assets/Textures/Player.png");
     */
    class OPAAX_API OpaaxPath
    {
        // =============================================================================
        // Static
        // =============================================================================
    private:
        static bool IsAbsolute(const char* InPath) noexcept;

        //------------------------------------------------------------------------------

    public:
        /**
         * Should be called at engine init.
         */
        static void Init();

        /**
         * Resolve a project-root-relative path to an absolute path.
         * In editor builds (OPAAX_PROJECT_ROOT defined) → resolves against the
         * source tree. In release builds → falls back to the exe directory.
         * Already-absolute paths pass through unchanged.
         */
        static OpaaxString ToAbsolute(const char* InRelativePath);
        static OpaaxString ToAbsolute(const OpaaxString& InRelativePath);

        /**
         * Convert an absolute path to a project-root-relative path.
         * Strips s_ProjectRoot when available; otherwise strips s_BasePath
         * (release-build behavior). If InAbsPath is under neither, returns it
         * unchanged with a warning.
         */
        static OpaaxString ToProjectRelative(const char* InAbsPath) noexcept;
        static OpaaxString ToProjectRelative(const OpaaxString& InAbsPath) noexcept;

        //------------------------------------------------------------------------------
        //  Get - Set

        static const OpaaxString& GetBasePath() noexcept { return s_BasePath; }

        /**
         * Project source-tree root. Populated from the OPAAX_PROJECT_ROOT compile-time
         * define when present (editor builds). Empty in release.
         */
        static const OpaaxString& GetProjectRoot() noexcept { return s_ProjectRoot; }
        static bool HasProjectRoot() noexcept { return !s_ProjectRoot.IsEmpty(); }

        /**
         * Detect whether a path string is already absolute.
         */
        static bool IsAbsolutePath(const OpaaxString& InPath) noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static OpaaxString s_BasePath;
        static OpaaxString s_ProjectRoot;
    };

} // namespace Opaax
