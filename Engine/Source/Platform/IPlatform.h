#pragma once
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

#include "Application/Services/IAppService.h"

namespace Opaax
{
    class IFileSystem;
    // =============================================================================
    // IPlatform — OS abstraction (cores, time, env...).
    // =============================================================================
    class IPlatform : public IAppService
    {
    public:
        OPAAX_SERVICE_TYPE(IPlatform)

        //----- contract -------------------------------------------------------
        virtual Uint32 GetLogicalCoreCount() const = 0;
        virtual double GetTimeSeconds()      const = 0;
        
        /**
         * Absolute path to the running executable (from the OS, unlike argv[0]).
         */
        virtual OpaaxString GetExecutablePath() const = 0;

        /**
         * @return The platform name (Windows, Linux, Mac)
         */
        virtual OpaaxString GetPlatformName() const = 0;

        /**
         * @return The platform file system
         */
        virtual const IFileSystem& GetFileSystem() const = 0;

        /**
         * The OS folder for each user's application data: %APPDATA% on Windows, ~/.local/share on
         * Linux, ~/Library/Application Support on macOS.
         * @return Empty when the OS does not give one
         */
        virtual OpaaxString GetUserDataDirectory() const = 0;

        //----- null object ----------------------------------------------------
        static IPlatform& Null();
    };

    /**
     * The user data folder on Linux and macOS, from the environment's values: macOS uses
     * <InHome>/Library/Application Support; Linux an absolute InXdgDataHome, else <InHome>/.local/share.
     * Pure, so it is tested on every platform.
     * @return Empty when InHome is empty and no XDG folder applies
     */
    OpaaxString ResolvePosixUserDataDirectory(const OpaaxString& InXdgDataHome, const OpaaxString& InHome, bool bInMacOS);
}
