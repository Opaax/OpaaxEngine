#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"

namespace Opaax
{
    /**
     * @class EngineConfig
     *
     * Static, process-wide engine configuration loaded from a single JSON file
     * (typically engine.config.json at the project root in editor builds, or
     * next to the executable in release builds). Defaults match the historical
     * hardcoded values, so behavior is unchanged when no file exists.
     *
     * NOTE: Load() must be called before any subsystem that consumes a value
     *   (window creation, asset manifest loading). Call site lives in
     *   CoreEngineApp ctor, right after OpaaxPath::Init().
     */
    class OPAAX_API EngineConfig
    {
    public:
        /**
         * Load the config from disk. If the file does not exist, a default file
         * is generated at InAbsPath and the in-memory defaults are kept.
         * @return true on success or after a successful default-generation;
         *         false if a file existed but failed to parse.
         */
        static bool Load(const OpaaxString& InAbsPath);

        /**
         * Persist the current values back to InAbsPath.
         */
        static bool Save(const OpaaxString& InAbsPath);

        // ---- Window ---------------------------------------------------------
        static const OpaaxString& WindowTitle()  noexcept { return s_WindowTitle; }
        static Uint32             WindowWidth()  noexcept { return s_WindowWidth; }
        static Uint32             WindowHeight() noexcept { return s_WindowHeight; }

        // ---- Assets ---------------------------------------------------------
        static const OpaaxString& EngineAssetsRoot()      noexcept { return s_EngineAssetsRoot; }
        static const OpaaxString& EngineManifestRelPath() noexcept { return s_EngineManifestRelPath; }

        // ---- Logging --------------------------------------------------------
        static const OpaaxString& LogLevel() noexcept { return s_LogLevel; }

        // ---- Render ---------------------------------------------------------
        // Graphics backend name ("OpenGL" today). Kept as a string here so Core/Config
        // carries no dependency on RHI — RHI maps this to its EBackend enum.
        static const OpaaxString& RenderBackend() noexcept { return s_RenderBackend; }

    private:
        static bool GenerateDefault(const OpaaxString& InAbsPath);

        static OpaaxString s_WindowTitle;
        static Uint32      s_WindowWidth;
        static Uint32      s_WindowHeight;
        static OpaaxString s_EngineAssetsRoot;
        static OpaaxString s_EngineManifestRelPath;
        static OpaaxString s_LogLevel;
        static OpaaxString s_RenderBackend;
    };
} // namespace Opaax
