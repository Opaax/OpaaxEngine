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
     * Resolves asset paths relative to the executable, not the working directory.
     * The executable location is always reliable. Assets are deployed next to the exe by CMake post-build commands. So we resolve from there.
     *
     * OpaaxString lFull = OpaaxPath::Resolve("EngineAssets/Textures/Player.png");
     *
     * // Use the OPAAX_ASSET macro to keep callsites clean
     * auto lTex = AssetRegistry::Load<OpenGLTexture2D>(OPAAX_ASSET("EngineAssets/Textures/Player.png"));
     */
    class OPAAX_API OpaaxPath
    {
        // =============================================================================
        // Static
        // =============================================================================
    private:
        /**
         * 
         * @param InPath 
         * @return
         */
        static bool IsAbsolute(const char* InPath) noexcept;

        //------------------------------------------------------------------------------
        
    public:
        /**
         * Should be call at engine init
         */
        static void Init();
        
        /**
         * Convert a relative path to an absolute path
         * @param InRelativePath The path to resolve
         * @return Absolute paths (start with drive letter or /) pass through unchanged.
         */
        static OpaaxString Resolve(const char* InRelativePath);

        /**
         * Convert a relative path to an absolute path
         * @param InRelativePath The path to resolve as OpaaxString
         * @return Absolute paths (start with drive letter or /) pass through unchanged.
         */
        static OpaaxString Resolve(const OpaaxString& InRelativePath);

        //------------------------------------------------------------------------------
        //  Get - Set

        /**
         * 
         * @return 
         */
        FORCEINLINE static const OpaaxString& GetBasePath() noexcept { return s_BasePath; }
     
        /**
         * Convert an absolute path to a path relative to the base path.
         * @param InAbsPath Absolute path as char*
         * @return If InAbsPath does not start with the base path, returns InAbsPath unchanged
         */
        FORCEINLINE static OpaaxString MakeRelative(const char* InAbsPath) noexcept;

       /**
         * Convert an absolute path to a path relative to the base path.
         * @param InAbsPath Absolute path as OpaaxString
         * @return If InAbsPath does not start with the base path, returns InAbsPath unchanged
         */
        FORCEINLINE static OpaaxString MakeRelative(const OpaaxString& InAbsPath) noexcept;
     
        /**
         * To detect whether an ID string is already an absolute path.
         * @param InPath 
         * @return 
         */
        FORCEINLINE static bool IsAbsolutePath(const OpaaxString& InPath) noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        static OpaaxString s_BasePath;
    };

    /**
     * 
     * @param RelPath: Resolves a relative asset path at the callsite.
     * The resolved OpaaxString is interned into an OpaaxStringID.
     *
     * auto lTex = AssetRegistry::Load<OpenGLTexture2D>(OPAAX_ASSET("EngineAssets/Textures/Player.png"));
     */
#define OPAAX_ASSET(RelPath) ::Opaax::OpaaxStringID(::Opaax::OpaaxPath::Resolve(RelPath))
 
} // namespace Opaax