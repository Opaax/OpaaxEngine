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
     * Path resolver. Everything lives under a WORKSPACE root — the directory that holds the
     * engine and every game project side by side:
     *
     *     <workspace>/
     *         Engine/    Assets/  Source/  ...     <- engine root
     *         Sandbox/   Assets/  ...              <- project root (when running Sandbox)
     *         Game/      Assets/  ...
     *
     * Roots (all normalised to '/'):
     *   BasePath      — the running executable's own directory.
     *   WorkspaceRoot — holds Engine/ + the game projects. Editor: baked from the OPAAX_WORKSPACE_DIR
     *                   compile define (= CMAKE_SOURCE_DIR). Release: BasePath (the deploy mirrors this
     *                   layout under the exe dir).
     *   EngineRoot    — "<WorkspaceRoot>/Engine"     (holds the engine's Assets).
     *   ProjectRoot   — "<WorkspaceRoot>/<AppName>", AppName = exe stem (e.g. "Sandbox").
     *
     * Resolvers (already-absolute inputs pass through unchanged):
     *   ToAbsolute("Engine/Assets/..")  -> under WorkspaceRoot. The asset-manifest layer speaks this
     *                                      workspace-relative form (entries carry the Engine/ or <App>/ prefix).
     *   EngineToAbsolute("Assets/..")   -> under EngineRoot.
     *   ProjectToAbsolute("Assets/..")  -> under ProjectRoot.
     *   ToProjectRelative(abs)          -> strips WorkspaceRoot (round-trips ToAbsolute).
     */
    class OPAAX_API OpaaxPath
    {
        // =============================================================================
        // Static
        // =============================================================================
    private:
        static bool        IsAbsolute(const char* InPath) noexcept;
        static OpaaxString ResolveAgainst(const OpaaxString& InBase, const char* InRel);

        //------------------------------------------------------------------------------

    public:
        /**
         * Resolve every root from the running executable. Call once at engine init,
         * before any consumer (config load, asset manifests) resolves a path.
         */
        static void Init();

        //------------------------------------------------------------------------------
        //  Resolvers

        /**
         * Resolve a workspace-root-relative path (e.g. "Engine/Assets/..", "Sandbox/Assets/..").
         * This is the form the asset manifest stores and resolves.
         */
        static OpaaxString ToAbsolute(const char* InRelativePath);
        static OpaaxString ToAbsolute(const OpaaxString& InRelativePath);

        /** Resolve an engine-root-relative path (e.g. "Assets/.." under "<workspace>/Engine"). */
        static OpaaxString EngineToAbsolute(const char* InRelativePath);
        static OpaaxString EngineToAbsolute(const OpaaxString& InRelativePath);

        /** Resolve a project-root-relative path (e.g. "Assets/.." under "<workspace>/<AppName>"). */
        static OpaaxString ProjectToAbsolute(const char* InRelativePath);
        static OpaaxString ProjectToAbsolute(const OpaaxString& InRelativePath);

        /**
         * Convert an absolute path to a workspace-root-relative path (the manifest form). Strips
         * WorkspaceRoot, else BasePath. If the input is under neither it is returned unchanged
         * with a warning.
         */
        static OpaaxString ToProjectRelative(const char* InAbsPath) noexcept;
        static OpaaxString ToProjectRelative(const OpaaxString& InAbsPath) noexcept;

        //------------------------------------------------------------------------------
        //  Get - Set

        static const OpaaxString& GetBasePath()      noexcept { return s_BasePath; }
        static const OpaaxString& GetAppName()       noexcept { return s_AppName; }

        /** Workspace root — holds Engine/ + the game projects. */
        static const OpaaxString& GetWorkspaceRoot() noexcept { return s_WorkspaceRoot; }

        /** Engine root, "<WorkspaceRoot>/Engine" (holds the engine's Assets). */
        static const OpaaxString& GetEngineRoot()    noexcept { return s_EngineRoot; }

        /** The active app's own folder, "<WorkspaceRoot>/<AppName>". */
        static const OpaaxString& GetProjectRoot()   noexcept { return s_ProjectRoot; }

        static bool HasEngineRoot()  noexcept { return !s_EngineRoot.IsEmpty(); }
        static bool HasProjectRoot() noexcept { return !s_ProjectRoot.IsEmpty(); }

        /** Detect whether a path string is already absolute. */
        static bool IsAbsolutePath(const OpaaxString& InPath) noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static OpaaxString s_BasePath;
        static OpaaxString s_AppName;
        static OpaaxString s_WorkspaceRoot;
        static OpaaxString s_EngineRoot;
        static OpaaxString s_ProjectRoot;
    };

} // namespace Opaax
